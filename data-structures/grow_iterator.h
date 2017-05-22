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

#include"base_iterator.h"

namespace growt
{

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
public:
    using value_type   = typename RefBase_t::value_type;

    ReferenceGrowT(RefBase_t ref, size_t ver, Table_t& table)
        : tab(table), version(ver), ref(ref) { }


    // Functions necessary for concurrency *************************************
    void refresh()
    {
        tab.execute(
            [](HashPtrRef_t t, ReferenceGrowT& sref) -> int
            {
                sref.base_refresh_ptr(t);
                sref.ref.refresh();
                return 0;
            }, *this);
    }

    template<bool is_const2 = is_const>
    typename std::enable_if<!is_const2>::type operator=(const mapped_type& value)
    {
        tab.execute(
            [](HashPtrRef_t t, ReferenceGrowT& sref, const mapped_type& value) -> int
            {
                sref.base_refresh_ptr(t);
                sref.ref.operator=(value);
                return 0;
            }, *this, value);
    }
    template<class F>
    void update   (const mapped_type& value, F f)
    {
        tab.execute(
            [](HashPtrRef_t t, ReferenceGrowT& sref, const mapped_type& value, F f) -> int
            {
                sref.base_refresh_ptr(t);
                sref.ref.update(value, f);
                return 0;
            }, *this, value, f);
    }
    bool compare_exchange(mapped_type& exp, const mapped_type &val)
    {
        return tab.execute(
            [](HashPtrRef_t t, ReferenceGrowT& sref,
               mapped_type& exp, const mapped_type& val)
            {
                sref.base_refresh_ptr(t);
                return sref.ref.compare_exchange(exp, val);
            }, *this, std::ref(exp), val);
    }

    operator pair_type()  const { return pair_type (ref); }
    operator value_type() const { return value_type(ref); }

private:
    Table_t&  tab;
    size_t    version;
    RefBase_t ref;

    // the table should be locked, while this is called
    bool base_refresh_ptr(HashPtrRef_t ht)
    {
        if (ht->version == version) return false;

        version = ht->version;
        auto it = ht->find(ref.copy.first);
        ref.copy = it.copy;
        ref.ptr  = it.ptr;
        return true;
    }
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
        : tab(table), version(version), it(it) { }

    IteratorGrowT(const IteratorGrowT& rhs)
        : tab(rhs.tab), version(rhs.version), it(rhs.it) { }
    IteratorGrowT& operator=(const IteratorGrowT& r)
    { tab = r.tab; version = r.version; it = r.it; return *this; }

    //template <bool is_const2 = is_const>
    //typename std::enable_if<is_const2, IteratorGrowT<Table, is_const>&>::type
    //operator=(const IteratorGrowT<Table,false>& r)
    //{ tab = r.tab; version = r.version; it = r.it; return *this; }

    ~IteratorGrowT() = default;


    // Basic Iterator Functionality ********************************************
    IteratorGrowT& operator++(int = 0)
    {
        tab.cexecute(
            [](HashPtrRef_t t, IteratorGrowT& sit) -> int
            {
                sit.base_refresh_ptr(t);
                sit.it++;
                return 0;
            }, *this);
        return *this;
    }

    reference operator* () const { return reference(*it, version, tab); }
    // pointer   operator->() const { return  ptr; }

    bool operator==(const IteratorGrowT& rhs) const { return it == rhs.it; }
    bool operator!=(const IteratorGrowT& rhs) const { return it != rhs.it; }

    // Functions necessary for concurrency *************************************
    void refresh()
    {
        tab.cexecute(
            [](HashPtrRef_t t, IteratorGrowT& sit) -> int
            {
                sit.base_refresh_ptr(t);
                sit.it.refresh();
                return 0;
            },
            *this);
    }

private:
    Table_t&    tab;
    size_t      version;
    BIterator_t it;

    // the table should be locked, while this is called
    bool base_refresh_ptr(HashPtrRef_t ht)
    {
        if (ht->version == version) return false;

        version = ht->version;
        it      = static_cast<BTable_t&>(*ht).find(it.copy.first);
        return true;
    }
};
}
