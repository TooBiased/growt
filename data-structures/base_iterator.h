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

#include <tuple>

namespace growt
{

// Forward Declarations ********************************************************
template <class, bool>
class MappedRefGrowT;
template <class, bool>
class ReferenceGrowT;
template <class, bool>
class IteratorGrowt;

template <class, bool>
class ReferenceBase;



// Mapped Reference
template <class BaseTable, bool is_const = false>
class MappedRefBase
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
    friend class MappedRefGrowT;
    template <class, bool>
    friend class ReferenceGrowT;
    template <class, bool>
    friend class ReferenceBase;
public:
    using value_type   = typename
        std::conditional<is_const, const value_nc, value_nc>::type;

    MappedRefBase(value_type copy, pointer_intern ptr)
        : _copy(copy), _ptr(ptr) { }

    inline void refresh() { _copy = *_ptr; }

    template<bool is_const2 = is_const>
    inline typename std::enable_if<!is_const2>::type operator=(const mapped_type& value)
    { _ptr->setData(value); }
    template<class F>
    inline void update   (const mapped_type& value, F f)
    { _ptr->update(_copy.first, value, f); }
    inline bool compare_exchange(mapped_type& exp, const mapped_type& val)
    {
        auto temp = value_intern(_copy.first, exp);
        if (_ptr->CAS(temp, value_intern(_copy.first, val)))
        { _copy.second = val; return true; }
        else
        { _copy.second = temp.second; exp = temp.second; return false; }
    }

    inline operator mapped_type()  const { return _copy.second; }

private:
    pair_type      _copy;
    pointer_intern _ptr;
};




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

    using mapped_ref     = MappedRefBase<BaseTable, is_const>;

    template <class, bool>
    friend class ReferenceGrowT;
    template <class, bool>
    friend class MappedRefGrowT;
public:
    using value_type   = typename
        std::conditional<is_const, const value_nc, value_nc>::type;

    ReferenceBase(value_type copy, pointer_intern ptr)
        : second(copy, ptr), first(second._copy.first) { }

    inline void refresh() { second.refresh(); }

    template<class F>
    inline void update   (const mapped_type& value, F f)
    { second.update(value, f); }
    inline bool compare_exchange(mapped_type& exp, const mapped_type& val)
    {
        return second.compare_exchange(exp,val);
    }

    inline operator pair_type()  const
    { return second._copy; }
    inline operator value_type() const
    { return reinterpret_cast<value_type>(second._copy); }

    mapped_ref      second;
    const key_type& first;
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
        : _copy(copy), _ptr(ptr), _eptr(eptr) { }

    IteratorBase(const IteratorBase& rhs)
        : _copy(rhs._copy), _ptr(rhs._ptr), _eptr(rhs._eptr) { }
    IteratorBase& operator=(const IteratorBase& r)
    { _copy = r._copy; _ptr = r._ptr; _eptr = r._eptr; return *this; }

    ~IteratorBase() = default;

    // Basic Iterator Functionality ********************************************
    inline IteratorBase& operator++()
    {
        ++_ptr;
        while ( _ptr < _eptr && (_ptr->is_empty() || _ptr->is_deleted())) { ++_ptr; }
        _copy.first  = _ptr->get_key();
        _copy.second = _ptr->get_data();
        if (_ptr == _eptr)
        {
            _ptr  = nullptr;
            _copy = std::make_pair(key_type(), mapped_type());
        }
        return *this;
    }

    inline IteratorBase& operator++(int)
    {
        IteratorBase copy(*this);
        ++(*this);
        return copy;
    }

    inline reference operator* () const { return reference(_copy, _ptr); }
    // pointer   operator->() const { return  _ptr; }

    inline bool operator==(const IteratorBase& rhs) const { return _ptr == rhs._ptr; }
    inline bool operator!=(const IteratorBase& rhs) const { return _ptr != rhs._ptr; }

    // Functions necessary for concurrency *************************************
    inline void refresh () { _copy = *_ptr; }

    inline bool erase()
    {
        auto temp   = value_intern(reinterpret_cast<value_type>(_copy));
        while (!temp.is_deleted)
        {
            if (_ptr->atomic_delete(temp))
            { this->operator++(); return true; }
        }
        this->operator++();
        return false;
    }

private:
    pair_type      _copy;
    pointer_intern _ptr;
    pointer_intern _eptr;
};


}
