/*******************************************************************************
 * data-structures/grow_iterator.h
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <functional>

#include "base_iterator.h"

namespace growt
{

template<class, bool>
class growt_reference;

template<class Table, bool is_const = false>
class growt_mapped_reference
{
private:
    using table_type            = typename std::conditional<is_const,
                                                            const Table,
                                                            Table>::type;

    using key_type              = typename table_type::key_type;
    using mapped_type           = typename table_type::mapped_type;

    using base_mapped_reference = typename std::conditional<is_const,
                                    typename table_type::base_table_type
                                                ::const_reference::mapped_ref,
                                    typename table_type::base_table_type
                                                ::reference::mapped_ref>::type;
    using hash_ptr_reference    = typename table_type::hash_ptr_reference;
    using pair_type             = std::pair<key_type, mapped_type>;

    template <class, bool>
    friend class growt_reference;
public:
    //using value_type   = typename base_reference::value_type;

    growt_mapped_reference(base_mapped_reference mref, size_t ver, table_type& table)
        : _tab(table), _version(ver), _mref(mref) { }


    // Functions necessary for concurrency *************************************
    inline void refresh()
    {
        _tab.execute(
            [](hash_ptr_reference t, growt_mapped_reference& sref) -> int
            {
                sref.base_refresh_ptr(t);
                sref.ref.refresh();
                return 0;
            }, *this);
    }

    template<bool is_const2 = is_const>
    inline typename std::enable_if<!is_const2>::type operator=(const mapped_type& value)
    {
        _tab.execute(
            [](hash_ptr_reference t, growt_mapped_reference& sref, const mapped_type& value) -> int
            {
                sref.base_refresh_ptr(t);
                sref.ref.operator=(value);
                return 0;
            }, *this, value);
    }
    template<class F>
    inline void update   (const mapped_type& value, F f)
    {
        _tab.execute(
            [](hash_ptr_reference t, growt_mapped_reference& sref, const mapped_type& value, F f) -> int
            {
                sref.base_refresh_ptr(t);
                sref.ref.update(value, f);
                return 0;
            }, *this, value, f);
    }
    inline bool compare_exchange(mapped_type& exp, const mapped_type &val)
    {
        return _tab.execute(
            [](hash_ptr_reference t, growt_mapped_reference& sref,
               mapped_type& exp, const mapped_type& val)
            {
                sref.base_refresh_ptr(t);
                return sref.ref.compare_exchange(exp, val);
            }, *this, std::ref(exp), val);
    }

    inline operator mapped_type()  const { return mapped_type (_mref); }

private:
    table_type&           _tab;
    size_t                _version;
    base_mapped_reference _mref;

    // the table should be locked, while this is called
    inline bool base_refresh_ptr(hash_ptr_reference ht)
    {
        if (ht->_version == _version) return false;

        _version = ht->_version;
        auto it = ht->find(_mref._copy.first);
        _mref._copy = it._copy;
        _mref._ptr  = it._ptr;
        return true;
    }

};


template<class Table, bool is_const = false>
class growt_reference
{
private:
    using table_type         = typename std::conditional<is_const, const Table, Table>::type;

    using key_type           = typename table_type::key_type;
    using mapped_type        = typename table_type::mapped_type;

    using base_reference     = typename std::conditional<is_const,
                                                   typename table_type::base_table_type::const_reference,
                                                   typename table_type::base_table_type::reference>::type;
    using hash_ptr_reference = typename table_type::hash_ptr_reference;
    using pair_type          = std::pair<key_type, mapped_type>;

    using mapped_ref         = growt_mapped_reference<Table, is_const>;
public:
    using value_type         = typename base_reference::value_type;

    growt_reference(base_reference ref, size_t ver, table_type& table)
        : second(ref.second, ver, table),
          first(second._mref._copy.first) { }


    // Functions necessary for concurrency *************************************
    inline void refresh() { second.refresh(); }

    template<class F>
    inline void update   (const mapped_type& value, F f)
    { second.update(value, f); }
    inline bool compare_exchange(mapped_type& exp, const mapped_type &val)
    { return second.compare_exchange(exp, val); }

    inline operator pair_type()  const { return pair_type (second.ref); }
    inline operator value_type() const { return value_type(second.ref); }

private:
    // table_type&  tab;
    // size_t    _version;
    // base_reference ref;

    // the table should be locked, while this is called
    // inline bool base_refresh_ptr(hash_ptr_reference ht)
    // {
    //     if (ht->_version == _version) return false;

    //     _version = ht->_version;
    //     auto it = ht->find(ref._copy.first);
    //     ref._copy = it._copy;
    //     ref._ptr  = it._ptr;
    //     return true;
    // }

public:
    mapped_ref      second;
    const key_type& first;
    //const mapped_type& second;
};





template <class Table, bool is_const = false>
class growt_iterator
{
private:
    using table_type         = typename std::conditional<is_const, const Table, Table>::type;
    using key_type           = typename table_type::key_type;
    using mapped_type        = typename table_type::mapped_type;
    using pair_type          = std::pair<key_type, mapped_type>;
    using value_nc           = std::pair<const key_type, mapped_type>;
    using value_intern       = typename table_type::value_intern;
    using pointer_intern     = value_intern*;
    using hash_ptr_reference = typename table_type::hash_ptr_reference;
    using base_table_type    = typename std::conditional<is_const,
                                            const typename table_type::base_table_type,
                                            typename table_type::base_table_type>::type;
    using base_iterator      = typename std::conditional<is_const,
                                            typename base_table_type::const_iterator,
                                            typename base_table_type::iterator>::type;

public:
    using difference_type    = std::ptrdiff_t;
    using value_type         = typename base_iterator::value_type;
    using reference          = growt_reference<Table, is_const>;
    // using pointer           = value_type*;
    using iterator_category = std::forward_iterator_tag;

    // template<class T, bool b>
    // friend void swap(growt_iterator<T,b>& l, growt_iterator<T,b>& r);
    template<class T, bool b>
    friend bool operator==(const growt_iterator<T,b>& l, const growt_iterator<T,b>& r);
    template<class T, bool b>
    friend bool operator!=(const growt_iterator<T,b>& l, const growt_iterator<T,b>& r);


    // Constructors ************************************************************
    growt_iterator(base_iterator it, size_t version, table_type& table)
        : _tab(table), _version(version), _it(it) { }

    growt_iterator(const growt_iterator& rhs)
        : _tab(rhs._tab), _version(rhs._version), _it(rhs._it) { }

    growt_iterator& operator=(const growt_iterator& r)
    {
        if (this == &r) return *this;
        this->~growt_iterator();

        new (this) growt_iterator(r);

        return *this;
    }

    //template <bool is_const2 = is_const>
    //typename std::enable_if<is_const2, growt_iterator<Table, is_const>&>::type
    //operator=(const growt_iterator<Table,false>& r)
    //{ tab = r.tab; _version = r._version; it = r.it; return *this; }

    ~growt_iterator() = default;


    // Basic Iterator Functionality ********************************************
    inline growt_iterator& operator++()
    {
        _tab.cexecute(
            [](hash_ptr_reference t, growt_iterator& sit) -> int
            {
                sit.base_refresh_ptr(t);
                ++sit._it;
                return 0;
            }, *this);
        return *this;
    }

    inline growt_iterator& operator++(int)
    {
        growt_iterator copy(*this);
        ++(*this);
        return copy;
    }

    inline reference operator* () const { return reference(*_it, _version, _tab); }
    // pointer   operator->() const { return  _ptr; }

    inline bool operator==(const growt_iterator& rhs) const { return _it == rhs._it; }
    inline bool operator!=(const growt_iterator& rhs) const { return _it != rhs._it; }

    // Functions necessary for concurrency *************************************
    inline void refresh()
    {
        _tab.cexecute(
            [](hash_ptr_reference t, growt_iterator& sit) -> int
            {
                sit.base_refresh_ptr(t);
                sit._it.refresh();
                return 0;
            },
            *this);
    }

private:
    table_type&   _tab;
    size_t        _version;
    base_iterator _it;

    // the table should be locked, while this is called
    inline bool base_refresh_ptr(hash_ptr_reference ht)
    {
        if (ht->_version == _version) return false;

        _version = ht->_version;
        _it      = static_cast<base_table_type&>(*ht).find(_it._copy.first);
        return true;
    }
};
}
