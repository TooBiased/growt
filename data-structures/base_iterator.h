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

namespace growt
{

// Forward Declarations ********************************************************
template <class, bool>
class ReferenceGrowT;
template <class, bool>
class IteratorGrowt;


// Reference *******************************************************************
template <class BaseTable, bool is_const = false>
class ReferenceBase
{
private:
    using BTable_t       = BaseTable;
    using key_type       = typename BTable_t::key_type;
    using mapped_type    = typename BTable_t::mapped_type;
    using pair_type      = std::pair<key_type, mapped_type>;
    using value_nc       = std::pair<const key_type, mapped_type>;
    using value_intern   = typename BTable_t::value_intern;
    using pointer_intern = value_intern*;

    template <class, bool>
    friend class ReferenceGrowT;
public:
    using value_type   = typename
        std::conditional<is_const, const value_nc, value_nc>::type;

    ReferenceBase(value_type _copy, pointer_intern ptr)
        : copy(_copy), ptr(ptr), first(copy.first), second(copy.second) { }

    void refresh()                          { copy = *ptr; }

    template<bool is_const2 = is_const>
    typename std::enable_if<!is_const2>::type operator=(const mapped_type& value) { ptr->setData(value); }
    template<class F>
    void update   (const mapped_type& value, F f) { ptr->update(copy.first, value, f); }
    bool compare_exchange(mapped_type& exp, const mapped_type& val)
    {
        auto temp = value_intern(copy.first, exp);
        if (ptr->CAS(temp, value_intern(copy.first, val)))
        { copy.second = val; return true; }
        else
        { copy.second = temp.second; exp = temp.second; return false; }
    }

    operator pair_type()  const { return copy; }
    operator value_type() const { return reinterpret_cast<value_type>(copy); }

private:
    pair_type      copy;
    pointer_intern ptr;

public:
    const key_type& first;
    const mapped_type& second;
};


// Iterator ********************************************************************
template <class BaseTable, bool is_const = false>
class IteratorBase
{
private:
    using BTable_t       = BaseTable;
    using key_type       = typename BTable_t::key_type;
    using mapped_type    = typename BTable_t::mapped_type;
    using pair_type      = std::pair<key_type, mapped_type>;
    using value_nc       = std::pair<const key_type, mapped_type>;
    using value_intern   = typename BTable_t::value_intern;
    using pointer_intern = value_intern*;

    template <class, bool>
    friend class IteratorGrowT;
    template <class, bool>
    friend class ReferenceGrowT;
public:
    using difference_type = std::ptrdiff_t;
    using value_type = typename std::conditional<is_const, const value_nc, value_nc>::type;
    using reference  = ReferenceBase<BaseTable, is_const>;
    // using pointer    = value_type*;
    using iterator_category = std::forward_iterator_tag;

    // template<class T, bool b>
    // friend void swap(IteratorBase<T,b>& l, IteratorBase<T,b>& r);
    template<class T, bool b>
    friend bool operator==(const IteratorBase<T,b>& l, const IteratorBase<T,b>& r);
    template<class T, bool b>
    friend bool operator!=(const IteratorBase<T,b>& l, const IteratorBase<T,b>& r);

    // Constructors ************************************************************
    IteratorBase(const pair_type& copy, value_intern* ptr, value_intern* eptr)
        : copy(copy), ptr(ptr), eptr(eptr) { }

    IteratorBase(const IteratorBase& rhs)
        : copy(rhs.copy), ptr(rhs.ptr), eptr(rhs.eptr) { }
    IteratorBase& operator=(const IteratorBase& r)
    { copy = r.copy; ptr = r.ptr; eptr = r.eptr; return *this; }

    ~IteratorBase() = default;

    // Basic Iterator Functionality ********************************************
    IteratorBase& operator++(int = 0)
    {
        ++ptr;
        while ( ptr < eptr && (ptr->isEmpty() || ptr->isDeleted())) { ++ptr; }
        if (ptr == eptr)
        {
            ptr  = nullptr;
            copy = std::make_pair(key_type(), mapped_type());
        }
        return *this;
    }

    reference operator* () const { return reference(copy, ptr); }
    // pointer   operator->() const { return  ptr; }

    bool operator==(const IteratorBase& rhs) const { return ptr == rhs.ptr; }
    bool operator!=(const IteratorBase& rhs) const { return ptr != rhs.ptr; }

    // Functions necessary for concurrency *************************************
    void refresh () { copy = *ptr; }

    bool erase()
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
};


}
