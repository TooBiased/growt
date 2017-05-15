#pragma once

/*******************************************************************************
 * data-structures/iterator_base.h
 *
 * MarkableElements represent the cells of a table, that has to be able to mark
 * a copied cell (used in uaGrow and paGrow). They encapsulate some CAS and
 * update methods.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

template <class Table>
{
private:

}

template<class Table, class Validator, bool is_const = false>
class ReferenceBase
{
private:
    using Table_t      = Table;
    using key_type     = typename Table_t::key_type;
    using mapped_type  = typename Table_t::mapped_type;
    using value_intern = typename Table_t::value_intern;
    using Valid_t      = Validator;

    using pointer      = value_intern&;
    using reference    = ReferenceBase<Table,Validator,is_const>;

public:
    ReferenceBase(pointer ptr, value_intern copy, Valid_t val)
        : ptr(ptr), copy(copy), val(val) { }

    void operator=(mapped_type value)
    {

    }

    operator user_type() const
    { return std::make_pair(copy.getKey(), copy.getData()); }

private:
    pointer      ptr;
    value_intern copy;
    Valid_t      val;
}

// template <class Table, class Ptr>
// class iterator_incr
// {
// private:
//     using Pointer_t = Ptr;
// public:
//     iterator_incr(const Table&) { }

//     iterator_incr(const iterator_incr&) = default;
//     iterator_incr& operator=(const iterator_incr&) = default;

//     Pointer_t next(Pointer_t) { return nullptr; }
// };

template <class Table, class Validator, class Increment, bool is_const = false>
class IteratorBase
{
private:
    using Table_t      = Table;

    using key_type     = typename Table_t::key_type;
    using mapped_type  = typename Table_t::mapped_type;
    using value_intern = typename Table_t::value_intern;

    using Incr_t   = Increment;
    using Valid_t  = Validator;

public:
    using difference_type = std::ptrdiff_t;
    using value_type = typename std::conditional<is_const, const value_intern, value_intern>::type;
    using reference  = value_type&;
    using pointer    = value_type*;
    using iterator_category = std::forward_iterator_tag;

    // template<class T, bool b>
    // friend void swap(IteratorBase<T,b>& l, IteratorBase<T,b>& r);
    template<class T, bool b>
    friend bool operator==(const IteratorBase<T,b>& l, const IteratorBase<T,b>& r);
    template<class T, bool b>
    friend bool operator!=(const IteratorBase<T,b>& l, const IteratorBase<T,b>& r);


    // Constructors ************************************************************

    IteratorBase(value_type* pair_, const Table_t& table)
        : ptr(reinterpret_cast<pointer>(pair_)), incr(table) { }

    IteratorBase(const IteratorBase& rhs) : ptr(rhs.ptr), incr(rhs.incr) { }
    IteratorBase& operator=(const IteratorBase& r)
    { ptr = r.ptr; incr = r.incr; return *this; }

    ~IteratorBase() = default;


    // Basic Iterator Functionality

    IteratorBase& operator++(int = 0) { ptr = incr.next(ptr); return *this; }
    reference operator* () const { return reference(ptr, copy, val); }
    // pointer   operator->() const { return  ptr; }

    bool compare_exchange(value_intern& expect, value_intern val)
    {

    }
    bool replace(mapped_type val)
    {

    }
    bool erase()
    {

    }
    template <class Functor>
    bool update(value_intern inp2)
    {

    }

    bool operator==(const IteratorBase& rhs) const { return ptr == rhs.ptr; }
    bool operator!=(const IteratorBase& rhs) const { return ptr != rhs.ptr; }

private:
    pointer      ptr;
    value_intern copy;
    Valid_t      val;
    Incr_t       incr;

};
