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

namespace growt
{
/*
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

public:
    using value_type   = typename
        std::conditional<is_const, const value_nc, value_nc>::type;

    ReferenceBase(value_type copy, pointer_intern ptr)
        : ptr(ptr), copy(copy) { }

    void refresh()                          { copy = *ptr; }
    void operator=(const mapped_type value) { ptr->setData(value); }
    template<class F>
    void update   (const mapped_type value, F f)
    { ptr->update(copy.first, value, f); }

    operator user_type() const        { return copy; }

// private:
    pointer_intern ptr;
    pair_type      copy;
};





template<class Table, bool is_const = false>
class ReferenceGrowT
{
private:
    using Table_t      = Table;
    using key_type     = typename Table_t::key_type;
    using mapped_type  = typename Table_t::mapped_type;

    using RefBase_t    = typename Table_t::typename BaseTable_t::reference;
    using HashPtrRef_t = typename Table_t::HashPtrRef_t;
    using pair_type    = std::pair<key_type, mapped_type>;
public:

    ReferenceGrowT(ReferenceBase ref, size_t ver, Table_t& table)
        : tab(table), version(ver), ref(ref) { }

    void refresh()
    {

        version = tab.execute(
            [](HashPtrRef_t t, ReferenceGrowT& sref) -> void
            {
                if (t->version == sref.version)
                {
                    sref.ref.refresh();
                }
                else
                {
                    sref.version = t->version;
                    auto temp    = t->find(sref.ref.copy.first);
                    sref.ref     = *temp;
                }
            }, *this);
    }
    void operator=(const mapped_type value) { } // TODO
    template<class F>
    void update   (const mapped_type value, F f) { } // TODO

    operator user_type() const        { return user_type(ref); }
private:
    Table_t&  tab;
    size_t    version;
    RefBase_t ref;
};
*/





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

public:
    using difference_type = std::ptrdiff_t;
    using value_type = typename std::conditional<is_const, const value_nc, value_nc>::type;
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
        while ( (++ptr).isEmpty() && ptr < eptr ) { }
        if (ptr == eptr)
        {
            ptr  = nullptr;
            copy = std::make_pair(key_type(), mapped_type());
        }
        return *this;
    }
    // reference operator* () const { return reference(ptr, copy, val); }
    // pointer   operator->() const { return  ptr; }

    bool operator==(const IteratorBase& rhs) const { return ptr == rhs.ptr; }
    bool operator!=(const IteratorBase& rhs) const { return ptr != rhs.ptr; }

    // Functions necessary for concurrency *************************************
    void refresh () { copy = *ptr; }

    // bool compare_exchange(value_intern& expect, value_intern val)
    // bool replace(mapped_type val)
    // bool erase()

    // template <class Functor>
    // bool update(value_intern inp2)

private:
    pair_type      copy;
    pointer_intern ptr;
    pointer_intern eptr;
};


template <class Table, bool is_const = false>
class IteratorGrowT
{
private:
    using Table_t        = Table;
    using key_type       = typename Table_t::key_type;
    using mapped_type    = typename Table_t::mapped_type;
    using pair_type      = std::pair<key_type, mapped_type>;
    using value_nc       = std::pair<const key_type, mapped_type>;
    using value_intern   = typename Table_t::value_intern;
    using pointer_intern = value_intern*;

    using HashPtrRef_t   = typename Table_t::HashPtrRef_t;

    using BTable_t       = typename Table_t::BaseTable_t;
    using BIterator_t    = typename BTable_t::iterator;

public:
    using difference_type = std::ptrdiff_t;
    using value_type = typename std::conditional<is_const, const value_nc, value_nc>::type;
    using reference  = value_type&;
    using pointer    = value_type*;
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

    ~IteratorGrowT() = default;


    // Basic Iterator Functionality ********************************************
    IteratorGrowT& operator++(int = 0)
    {
        tab.execute(
            [](HashPtrRef_t t, IteratorGrowT& sit) -> void
            {
                sit.base_refresh_ptr(t);
                sit.it++;
            },
            *this);
        return *this;
    }
    // reference operator* () const { return reference(ptr, copy, val); }
    // pointer   operator->() const { return  ptr; }

    bool operator==(const IteratorGrowT& rhs) const { return it == rhs.it; }
    bool operator!=(const IteratorGrowT& rhs) const { return it != rhs.it; }

    // Functions necessary for concurrency *************************************
    void refresh()
    {
        tab.execute(
            [](HashPtrRef_t t, IteratorGrowT& sit) -> void
            {
                sit.base_refresh_ptr(t);
                sit.it.refresh();
            },
            *this);
    }

    // bool compare_exchange(value_intern& expect, value_intern val)
    // bool replace(mapped_type val)
    // bool erase()

    // template <class Functor>
    // bool update(value_intern inp2)

private:
    Table_t&    tab;
    size_t      version;
    BIterator_t it;

    // the table should be locked, while this is called
    bool base_refresh_ptr(HashPtrRef_t ht)
    {
        if (ht->version == version) return false;

        version = ht->version;
        it      = ht->find(it.copy.first).first;
        return true;
    }
};
}
