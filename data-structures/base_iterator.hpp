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
class growt_mapped_reference;
template <class, bool>
class growt_reference;
template <class, bool>
class growt_iterator;

template <class, bool>
class base_reference;



// Mapped Reference
template <class BaseTable, bool is_const = false>
class base_mapped_reference
{
private:
    using base_table_type  = BaseTable;
    using key_type         = typename base_table_type::key_type;
    using mapped_type      = typename base_table_type::mapped_type;
    using pair_type        = std::pair<key_type, mapped_type>;
    using value_nc         = std::pair<const key_type, mapped_type>;
    //using value_intern    = typename base_table_type::value_intern;
    //using pointer_intern  = value_intern*;
    using slot_config      = typename base_table_type::slot_config;
    using slot_type        = typename slot_config::slot_type;
    using atomic_slot_type = typename slot_config::atomic_slot_type;

    template <class, bool>
    friend class growt_mapped_reference;
    template <class, bool>
    friend class growt_reference;
    template <class, bool>
    friend class base_reference;
public:
    using value_type       = typename std::conditional<is_const,
                                                       const value_nc,
                                                       value_nc>::type;

    base_mapped_reference(value_type copy, atomic_slot_type* ptr)
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
        auto temp = slot_type(_copy.first, exp);
        if (_ptr->CAS(temp, slot_type(_copy.first, val)))
        { _copy.second = val; return true; }
        else
        { _copy.second = temp.second; exp = temp.second; return false; }
    }

    inline operator mapped_type()  const { return _copy.second; }

private:
    pair_type         _copy;
    atomic_slot_type* _ptr;
};




// Reference *******************************************************************
template <class BaseTable, bool is_const = false>
class base_reference
{
private:
    using base_table_type  = BaseTable;
    using key_type         = typename base_table_type::key_type;
    using mapped_type      = typename base_table_type::mapped_type;
    using pair_type        = std::pair<key_type, mapped_type>;
    using value_nc         = std::pair<const key_type, mapped_type>;
    // using value_intern    = typename base_table_type::value_intern;
    // using pointer_intern  = value_intern*;
    using slot_config      = typename base_table_type::slot_config;
    using slot_type        = typename slot_config::slot_type;
    using atomic_slot_type = typename slot_config::atomic_slot_type;

    using mapped_ref      = base_mapped_reference<BaseTable, is_const>;

    template <class, bool>
    friend class growt_reference;
    template <class, bool>
    friend class growt_mapped_reference;
public:
    using value_type      = typename std::conditional<is_const,
                                                      const value_nc,
                                                      value_nc>::type;

    base_reference(value_type copy, atomic_slot_type* ptr)
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
class base_iterator
{
private:
    using base_table_type   = BaseTable;
    using key_type          = typename base_table_type::key_type;
    using mapped_type       = typename base_table_type::mapped_type;
    using pair_type         = std::pair<key_type, mapped_type>;
    using value_nc          = std::pair<const key_type, mapped_type>;
    // using value_intern      = typename base_table_type::value_intern;
    // using pointer_intern    = value_intern*;
    using slot_config       = typename base_table_type::slot_config;
    using slot_type         = typename slot_config::slot_type;
    using atomic_slot_type  = typename slot_config::atomic_slot_type;

    template <class, bool>
    friend class growt_iterator;
    template <class, bool>
    friend class growt_reference;
public:
    using difference_type   = std::ptrdiff_t;
    using value_type        = typename std::conditional<is_const,
                                                      const value_nc,
                                                      value_nc>::type;
    using reference         = base_reference<BaseTable, is_const>;
    // using pointer    = value_type*;
    using iterator_category = std::forward_iterator_tag;

    // template<class T, bool b>
    // friend void swap(base_iterator<T,b>& l, base_iterator<T,b>& r);
    template<class T, bool b>
    friend bool operator==(const base_iterator<T,b>& l, const base_iterator<T,b>& r);
    template<class T, bool b>
    friend bool operator!=(const base_iterator<T,b>& l, const base_iterator<T,b>& r);

    // Constructors ************************************************************
    base_iterator(const pair_type& copy, atomic_slot_type* ptr, atomic_slot_type* eptr)
        : _copy(copy), _ptr(ptr), _eptr(eptr) { }

    base_iterator(const base_iterator& rhs)
        : _copy(rhs._copy), _ptr(rhs._ptr), _eptr(rhs._eptr) { }

    base_iterator& operator=(const base_iterator& r)
    { _copy = r._copy; _ptr = r._ptr; _eptr = r._eptr; return *this; }

    ~base_iterator() = default;

    // Basic Iterator Functionality ********************************************
    inline base_iterator& operator++()
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

    inline base_iterator& operator++(int)
    {
        base_iterator copy(*this);
        ++(*this);
        return copy;
    }

    inline reference operator* () const { return reference(_copy, _ptr); }
    // pointer   operator->() const { return  _ptr; }

    inline bool operator==(const base_iterator& rhs) const { return _ptr == rhs._ptr; }
    inline bool operator!=(const base_iterator& rhs) const { return _ptr != rhs._ptr; }

    // Functions necessary for concurrency *************************************
    inline void refresh () { _copy = *_ptr; }

    inline bool erase()
    {
        auto temp   = slot_type(reinterpret_cast<value_type>(_copy));
        while (!temp.is_deleted)
        {
            if (_ptr->atomic_delete(temp))
            { this->operator++(); return true; }
        }
        this->operator++();
        return false;
    }

private:
    pair_type         _copy;
    atomic_slot_type* _ptr;
    atomic_slot_type* _eptr;
};


}
