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
class ReferenceGrowT;

template<class Table, bool is_const = false>
class MappedRefGrowT
{
private:
    using Table_t      = typename std::conditional<is_const, const Table, Table>::type;

    using key_type     = typename Table_t::key_type;
    using mapped_type  = typename Table_t::mapped_type;

    using MappedRefBase_t    = typename std::conditional<is_const,
                                   typename Table_t::BaseTable_t::const_reference::mapped_ref,
                                   typename Table_t::BaseTable_t::reference::mapped_ref>::type;
    using HashPtrRef_t = typename Table_t::HashPtrRef_t;
    using pair_type    = std::pair<key_type, mapped_type>;

    template <class, bool>
    friend class ReferenceGrowT;
public:
    //using value_type   = typename RefBase_t::value_type;

    MappedRefGrowT(MappedRefBase_t mref, size_t ver, Table_t& table)
        : _tab(table), _version(ver), _mref(mref) { }


    // Functions necessary for concurrency *************************************
    inline void refresh()
    {
        _tab.execute(
            [](HashPtrRef_t t, MappedRefGrowT& sref) -> int
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
            [](HashPtrRef_t t, MappedRefGrowT& sref, const mapped_type& value) -> int
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
            [](HashPtrRef_t t, MappedRefGrowT& sref, const mapped_type& value, F f) -> int
            {
                sref.base_refresh_ptr(t);
                sref.ref.update(value, f);
                return 0;
            }, *this, value, f);
    }
    inline bool compare_exchange(mapped_type& exp, const mapped_type &val)
    {
        return _tab.execute(
            [](HashPtrRef_t t, MappedRefGrowT& sref,
               mapped_type& exp, const mapped_type& val)
            {
                sref.base_refresh_ptr(t);
                return sref.ref.compare_exchange(exp, val);
            }, *this, std::ref(exp), val);
    }

    inline operator mapped_type()  const { return mapped_type (_mref); }

private:
    Table_t&        _tab;
    size_t          _version;
    MappedRefBase_t _mref;

    // the table should be locked, while this is called
    inline bool base_refresh_ptr(HashPtrRef_t ht)
    {
        if (ht->_version == _version) return false;

        _version = ht->_version;
        auto it = ht->find(_mref.copy.first);
        _mref.copy = it.copy;
        _mref.ptr  = it.ptr;
        return true;
    }

};


template<class Table, bool is_const = false>
class ReferenceGrowT
{
private:
    using Table_t      = typename std::conditional<is_const, const Table, Table>::type;

    using key_type     = typename Table_t::key_type;
    using mapped_type  = typename Table_t::mapped_type;

    using RefBase_t    = typename std::conditional<is_const,
                                                   typename Table_t::BaseTable_t::const_reference,
                                                   typename Table_t::BaseTable_t::reference>::type;
    using HashPtrRef_t = typename Table_t::HashPtrRef_t;
    using pair_type    = std::pair<key_type, mapped_type>;

    using mapped_ref   = MappedRefGrowT<Table, is_const>;
public:
    using value_type   = typename RefBase_t::value_type;

    ReferenceGrowT(RefBase_t ref, size_t ver, Table_t& table)
        : second(ref.second, ver, table),
          first(second._mref.copy.first) { }


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
    // Table_t&  tab;
    // size_t    _version;
    // RefBase_t ref;

    // the table should be locked, while this is called
    // inline bool base_refresh_ptr(HashPtrRef_t ht)
    // {
    //     if (ht->_version == _version) return false;

    //     _version = ht->_version;
    //     auto it = ht->find(ref.copy.first);
    //     ref.copy = it.copy;
    //     ref.ptr  = it.ptr;
    //     return true;
    // }

public:
    mapped_ref      second;
    const key_type& first;
    //const mapped_type& second;
};





template <class Table, bool is_const = false>
class IteratorGrowT
{
private:
    using Table_t        = typename std::conditional<is_const, const Table, Table>::type;
    using key_type       = typename Table_t::key_type;
    using mapped_type    = typename Table_t::mapped_type;
    using pair_type      = std::pair<key_type, mapped_type>;
    using value_nc       = std::pair<const key_type, mapped_type>;
    using value_intern   = typename Table_t::value_intern;
    using pointer_intern = value_intern*;

    using HashPtrRef_t   = typename Table_t::HashPtrRef_t;

    using BTable_t       = typename std::conditional<is_const,
                                            const typename Table_t::BaseTable_t,
                                            typename Table_t::BaseTable_t>::type;
    using BIterator_t    = typename std::conditional<is_const,
                                            typename BTable_t::const_iterator,
                                            typename BTable_t::iterator>::type;

public:
    using difference_type = std::ptrdiff_t;
    using value_type = typename BIterator_t::value_type;
    using reference  = ReferenceGrowT<Table, is_const>;
    // using pointer    = value_type*;
    using iterator_category = std::forward_iterator_tag;

    // template<class T, bool b>
    // friend void swap(IteratorGrowT<T,b>& l, IteratorGrowT<T,b>& r);
    template<class T, bool b>
    friend bool operator==(const IteratorGrowT<T,b>& l, const IteratorGrowT<T,b>& r);
    template<class T, bool b>
    friend bool operator!=(const IteratorGrowT<T,b>& l, const IteratorGrowT<T,b>& r);


    // Constructors ************************************************************
    IteratorGrowT(BIterator_t it, size_t version, Table_t& table)
        : _tab(table), _version(version), _it(it) { }

    IteratorGrowT(const IteratorGrowT& rhs)
        : _tab(rhs._tab), _version(rhs._version), _it(rhs._it) { }

    IteratorGrowT& operator=(const IteratorGrowT& r)
    {
        if (this == &r) return *this;
        this->~IteratorGrowT();

        new (this) IteratorGrowT(r);

        return *this;
    }

    //template <bool is_const2 = is_const>
    //typename std::enable_if<is_const2, IteratorGrowT<Table, is_const>&>::type
    //operator=(const IteratorGrowT<Table,false>& r)
    //{ tab = r.tab; _version = r._version; it = r.it; return *this; }

    ~IteratorGrowT() = default;


    // Basic Iterator Functionality ********************************************
    inline IteratorGrowT& operator++()
    {
        _tab.cexecute(
            [](HashPtrRef_t t, IteratorGrowT& sit) -> int
            {
                sit.base_refresh_ptr(t);
                ++sit._it;
                return 0;
            }, *this);
        return *this;
    }

    inline reference operator* () const { return reference(*_it, _version, _tab); }
    // pointer   operator->() const { return  ptr; }

    inline bool operator==(const IteratorGrowT& rhs) const { return _it == rhs._it; }
    inline bool operator!=(const IteratorGrowT& rhs) const { return _it != rhs._it; }

    // Functions necessary for concurrency *************************************
    inline void refresh()
    {
        _tab.cexecute(
            [](HashPtrRef_t t, IteratorGrowT& sit) -> int
            {
                sit.base_refresh_ptr(t);
                sit._it.refresh();
                return 0;
            },
            *this);
    }

private:
    Table_t&    _tab;
    size_t      _version;
    BIterator_t _it;

    // the table should be locked, while this is called
    inline bool base_refresh_ptr(HashPtrRef_t ht)
    {
        if (ht->_version == _version) return false;

        _version = ht->_version;
        _it      = static_cast<BTable_t&>(*ht).find(_it.copy.first);
        return true;
    }
};
}
