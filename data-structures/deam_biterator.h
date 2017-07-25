/*******************************************************************************
 * data-structures/base_iterator.h
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include "data-structures/base_iterator.h"

namespace growt
{

// Iterator ********************************************************************
template <class BaseTable, bool is_const = false>
class IteratorDeam
{
private:
    using BTable_t       = BaseTable;
    using key_type       = typename BTable_t::key_type;
    using mapped_type    = typename BTable_t::mapped_type;
    using pair_type      = std::pair<key_type, mapped_type>;
    using value_nc       = std::pair<const key_type, mapped_type>;
    using value_intern   = typename BTable_t::value_intern;
    using pointer_intern = value_intern*;

public:
    using difference_type = std::ptrdiff_t;
    using value_type = typename std::conditional<is_const, const value_nc, value_nc>::type;
    using reference  = ReferenceBase<BaseTable, is_const>;
    // using pointer    = value_type*;
    using iterator_category = std::forward_iterator_tag;

    // template<class T, bool b>
    // friend void swap(IteratorDeam<T,b>& l, IteratorDeam<T,b>& r);
    template<class T, bool b>
    friend bool operator==(const IteratorDeam<T,b>& l, const IteratorDeam<T,b>& r);
    template<class T, bool b>
    friend bool operator!=(const IteratorDeam<T,b>& l, const IteratorDeam<T,b>& r);

    // Constructors ************************************************************
    IteratorDeam() : copy(key_type(), mapped_type()), ptr(nullptr), eptr(nullptr), table(nullptr) { }
    IteratorDeam(const pair_type& copy, value_intern* ptr, BTable_t* table)
        : copy(copy), ptr(ptr),
          eptr(nullptr),
          table(table)
    {
        if (table) eptr = table->t.get() + table->capacity.load() + BTable_t::over_grow;
    }

    IteratorDeam(const IteratorDeam& rhs)
        : copy(rhs.copy), ptr(rhs.ptr), eptr(rhs.eptr), table(rhs.table) { }
    IteratorDeam& operator=(const IteratorDeam& r)
    { copy = r.copy; ptr = r.ptr; eptr = r.eptr; table = r.table; return *this;}

    ~IteratorDeam() = default;

    // Basic Iterator Functionality ********************************************
    inline IteratorDeam& operator++()
    {
        value_intern temp = value_intern::getEmpty();
        while (temp.isEmpty() || temp.isDeleted())
        {
            ++ptr;
            if (! checkPtr(ptr))
            {
                ptr  = nullptr;
                copy = std::make_pair(key_type(), mapped_type());
                eptr = nullptr;
                return *this;
            }
            auto temp   = *ptr;
            if (temp.isMarked) { --ptr; temp = value_intern::getEmpty(); }
        }

        copy  = temp;
        return *this;
    }

    inline reference operator* () const { return reference(copy, ptr); }
    // pointer   operator->() const { return  ptr; }

    inline bool operator==(const IteratorDeam& rhs) const { return ptr == rhs.ptr; }
    inline bool operator!=(const IteratorDeam& rhs) const { return ptr != rhs.ptr; }

    // Functions necessary for concurrency *************************************
    inline void refresh ()
    {
        value_intern temp = *ptr;
        while ( temp.isMarked() ) { /* wait */ }
        copy = *ptr;
    }

    inline bool erase()
    {
        auto temp   = value_intern(reinterpret_cast<value_type>(copy));
        auto result = false;
        while (!temp.isDeleted)
        {
            if (ptr->atomicDelete(temp))
            { this->operator++(); return true; }
        }
        this->operator++();
        return false;
    }

private:
    pair_type      copy;
    pointer_intern ptr;
    pointer_intern eptr;
    BTable_t*      table;

    void checkPtr()
    {
        if (ptr < eptr) return true;
        eptr = table->t.get() + table->capacity.load(std::memory_order_relaxed) + BTable_t::over_grow;
        return ptr < eptr;
    }
};


}
