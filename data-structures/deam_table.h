/*******************************************************************************
 * data-structures/deam_gtable.h
 *
 * Defines the growtable architecture:
 *   DeamTable       - the global facade of our table
 *   DeamTableData   - the actual global object (immovable core)
 *   DeamTableHandle - local handles on the global object (thread specific)
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2017 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <atomic>
#include <memory>
//#include <iostream>

#include "data-structures/grow_iterator.h"
#include "example/update_fcts.h"
#include "utils/concurrentptrarray.h"

namespace growt {



// FORWARD DECLARATION OF THE HANDLE CLASS
template<class> class DeamTableHandle;
// AND THE STATIONARY DATA OBJECT (GLOBAL OBJECT ON HEAP)
template<class> class DeamTableData;

template<class HashTable>
class DeamTable
{
private:
    using This_t           = DeamTable<HashTable>;
    using DTD_t            = DeamTableData<This_t>;
    using BaseTable_t      = HashTable;
    friend DTD_t;

    //const double max_fill  = MaxFill/100.;

    std::unique_ptr<DTD_t> dtData;

public:
    using Handle           = DeamTableHandle<DTD_t>;
    friend Handle;

    DeamTable (size_t size) : dtData(new DTD_t(size)) { }

    DeamTable (const DeamTable& source)            = delete;
    DeamTable& operator= (const DeamTable& source) = delete;

    DeamTable (DeamTable&& source)            = default;
    DeamTable& operator= (DeamTable&& source) = default;

    ~DeamTable() = default;

    Handle getHandle()
    {
        return Handle(*dtData);
    }

};





// GLOBAL TABLE OBJECT (THIS CANNOT BE USED WITHOUT CREATING A HANDLE)
template<typename Parent>
class DeamTableData
{
private:
    // TYPEDEFS
    using Parent_t         = Parent;
    using BaseTable_t      = typename Parent::BaseTable_t;
    using value_intern     = typename BaseTable_t::value_intern;

public:
    using size_type        = size_t;
    using Handle           = typename Parent::Handle;
    friend Handle;


    DeamTableData(size_type size_)
        : table(size_), handle_ptr(64),
          grow_thresh(size_),
          elements(0), dummies(0), grow_count(0)
    { }

    DeamTableData(const DeamTableData& source) = delete;
    DeamTableData& operator=(const DeamTableData& source) = delete;
    DeamTableData(DeamTableData&&) = delete;
    DeamTableData& operator=(DeamTableData&&) = delete;
    ~DeamTableData() = default;

    size_type element_count_approx() { return elements.load()-dummies.load(); }

private:

    static constexpr size_type n_elem_grow = BaseTable_t::block_grow / BaseTable_t::init_factor;

    BaseTable_t table;
    mutable ConcurrentPtrArray<Handle> handle_ptr;

    // APPROXIMATE COUNTS
    alignas(64) std::atomic_int grow_thresh;
    alignas(64) std::atomic_int elements;
    alignas(64) std::atomic_int dummies;
    alignas(64) std::atomic_int grow_count;
};




// HANDLE OBJECTS EVERY THREAD HAS TO CREATE ONE HANDLE (THEY CANNOT BE SHARED)
template<class DeamTableData>
class DeamTableHandle
{
private:
    using This_t             = DeamTableHandle<DeamTableData>;
    using Parent_t           = typename DeamTableData::Parent_t;
    using BaseTable_t        = typename DeamTableData::BaseTable_t;
    friend DeamTableData;

public:
    using value_intern       = typename BaseTable_t::value_intern;

    using key_type           = typename BaseTable_t::key_type;
    using mapped_type        = typename BaseTable_t::mapped_type;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = typename BaseTable_t::iterator;
    using const_iterator     = typename BaseTable_t::const_iterator;
    using size_type          = size_t;
    using difference_type    = std::ptrdiff_t;
    using reference          = typename BaseTable_t::reference;//ReferenceDeamT<This_t, false>;
    using const_reference    = typename BaseTable_t::const_reference;//ReferenceDeamT<This_t, true>;
    using mapped_reference       = typename BaseTable_t::mapped_reference;//MappedRefDeamT<This_t, false>;
    using const_mapped_reference = typename BaseTable_t::const_mapped_reference;//MappedRefDeamT<This_t, true>;
    using insert_return_type = std::pair<iterator, bool>;

    using local_iterator       = void;
    using const_local_iterator = void;
    using node_type            = void;

private:
    using basetable_iterator = typename BaseTable_t::iterator;
    using basetable_insert_return_type = typename BaseTable_t::insert_return_intern;
    using basetable_citerator = typename BaseTable_t::const_iterator;

    friend iterator;
    friend reference;
    friend const_iterator;
    friend const_reference;
    friend mapped_reference;
    friend const_mapped_reference;

public:
    DeamTableHandle() = delete;
    DeamTableHandle(DeamTableData &data);
    DeamTableHandle(Parent_t      &parent);

    DeamTableHandle(const DeamTableHandle& source) = delete;
    DeamTableHandle& operator=(const DeamTableHandle& source) = delete;

    DeamTableHandle(DeamTableHandle&& source);
    DeamTableHandle& operator=(DeamTableHandle&& source);

    ~DeamTableHandle();

    iterator begin();
    iterator end();
    const_iterator cbegin() const;
    const_iterator cend()   const;
    const_iterator begin()  const { return cbegin(); }
    const_iterator end()    const { return cend(); }

    insert_return_type insert(const key_type& k, const mapped_type& d);
    size_type          erase (const key_type& k);
    iterator           find  (const key_type& k);
    const_iterator     find  (const key_type& k) const;

    insert_return_type insert_or_assign(const key_type& k, const mapped_type& d)
    { return insertOrUpdate(k, d, example::Overwrite(), d); }

    mapped_reference operator[](const key_type& k)
    { return (*insert(k, mapped_type())).second; }

    template <class F, class ... Types>
    insert_return_type update
    (const key_type& k, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_type update_unsafe
    (const key_type& k, F f, Types&& ... args);


    template <class F, class ... Types>
    insert_return_type insertOrUpdate
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_type insertOrUpdate_unsafe
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);


    size_type element_count_approx() { return dtData.element_count_approx(); }
    size_type element_count_unsafe();

    size_type hash(const key_type& k)
    {
        return table.h(k);
    }

    size_type hasher(const key_type& k)
    {
        return table.hash(k);
    }


private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    size_type      handle_id;
    DeamTableData& dtData;
    BaseTable_t&   table;

    // inline void         helpGrowBlock() const { wtf... }
    // inline void         rlsTable() const { local_exclusion.rlsTable(); }
    // inline HashPtrRef_t getTable() const { return local_exclusion.getTable(); }

    // template<typename Functor, typename ... Types>
    // inline typename std::result_of<Functor(HashPtrRef_t, Types&& ...)>::type
    // execute (Functor f, Types&& ... param)
    // {
    //     HashPtrRef_t temp = local_exclusion.getTable();
    //     auto result = std::forward<Functor>(f)
    //                       (temp, std::forward<Types>(param)...);
    //     rlsTable();
    //     return result;
    // }

    // template<typename Functor, typename ... Types>
    // inline typename std::result_of<Functor(HashPtrRef_t, Types&& ...)>::type
    // cexecute (Functor f, Types&& ... param) const
    // {
    //     HashPtrRef_t temp = local_exclusion.getTable();
    //     auto result = std::forward<Functor>(f)
    //                       (temp, std::forward<Types>(param)...);
    //     rlsTable();
    //     return result;
    // }

    //inline iterator makeIterator(const basetable_iterator& bit, size_t version)
    //{ return iterator( wtf... ); }
    //inline insert_return_type makeInsertRet(const basetable_iterator& bit,
    //                                        size_t version, bool inserted)
    //{ return std::make_pair(iterator( wtf... ), inserted); }
    //inline basetable_iterator bend()
    //{ return basetable_iterator (std::make_pair(key_type(), mapped_type()), nullptr, nullptr);}
    //inline basetable_iterator bcend()
    //{ return basetable_citerator(std::make_pair(key_type(), mapped_type()), nullptr, nullptr);}


    // LOCAL COUNTERS FOR SIZE ESTIMATION WITH SOME PADDING FOR
    // REDUCING CACHE EFFECTS
public:
    void update_numbers();

private:
    void inc_inserted();
    void inc_deleted();

    class alignas(64) LocalCount
    {
    public:
        int  version;
        int  updates;
        int  inserted;
        int  deleted;
        LocalCount() : version(-1), updates(0), inserted(0), deleted(0)
        {  }

        LocalCount(LocalCount&& rhs)
            : version(rhs.version), updates(rhs.updates),
              inserted(rhs.inserted), deleted(rhs.deleted)
        {
            rhs.version = 0;
        }

        LocalCount& operator=(LocalCount&& rhs)
        {
            version   = rhs.version;
            rhs.version  = 0;
            updates  = rhs.updates;
            inserted = rhs.inserted;
            deleted  = rhs.deleted;
            return *this;
        }

        void set(int ver, int upd, int in, int del)
        {
            updates  = upd;
            inserted = in;
            deleted  = del;
            version  = ver;
        }

        LocalCount(const LocalCount&) = delete;
        LocalCount& operator=(const LocalCount&) = delete;
    };
    LocalCount counts;
};







// CONSTRUCTORS AND ASSIGNMENTS ************************************************

template<class DeamTableData>
DeamTableHandle<DeamTableData>::DeamTableHandle(DeamTableData &data)
    : dtData(data), table(data.table),
      counts()
{
    handle_id = dtData.handle_ptr.push_back(this);
}

template<class DeamTableData>
DeamTableHandle<DeamTableData>::DeamTableHandle(Parent_t      &parent)
    : dtData(*(parent.dtData)), table(*(parent.dtData).table),
      counts()
{
    handle_id = dtData.handle_ptr.push_back(this);
}



template<class DeamTableData>
DeamTableHandle<DeamTableData>::DeamTableHandle(DeamTableHandle&& source)
    : handle_id(source.handle_id), dtData(source.dtData), table(source.table),
      counts(std::move(source.counts))
{
    dtData.handle_ptr.update(handle_id, this);
    source.handle_id = std::numeric_limits<size_t>::max();
}

template<class DeamTableData>
DeamTableHandle<DeamTableData>&
DeamTableHandle<DeamTableData>::operator=(DeamTableHandle&& source)
{
    if (this == &source) return *this;

    this->~DeamTableHandle();
    new (this) DeamTableHandle(std::move(source));
    return *this;
}



template<class DeamTableData>
DeamTableHandle<DeamTableData>::~DeamTableHandle()
{

    if (handle_id < std::numeric_limits<size_t>::max())
    {
        dtData.handle_ptr.remove(handle_id);
    }
    if (counts.version >= 0)
    {
        update_numbers();
    }
}









// MAIN HASH TABLE FUNCTIONALITY ***********************************************

template<class DeamTableData>
inline typename DeamTableHandle<DeamTableData>::insert_return_type
DeamTableHandle<DeamTableData>::insert(const key_type& k, const mapped_type& d)
{
    // int v = -1;
    // basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);
    // std::tie (v, result) = execute([](HashPtrRef_t t, const key_type& k, const mapped_type& d)
    //                                  ->std::pair<int,basetable_insert_return_type>
    //                                {
    //                                    std::pair<int,basetable_insert_return_type> result =
    //                                        std::make_pair(t->version,
    //                                                       t->insert_intern(k,d));
    //                                    return result;
    //                                }, k,d);

    basetable_iterator it;
    ReturnCode         c = ReturnCode::ERROR;
    std::tie(it, c) = table.insert_intern(k,d);

    switch(c)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted();
    case ReturnCode::UNSUCCESS_ALREADY_USED:
    case ReturnCode::TSX_UNSUCCESS_ALREADY_USED:
        return insert_return_type(it,true); // makeInsertRet(result.first, v, true);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        table.grow();
        return insert(k,d);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        return insert(k,d);
    default:
        return insert_return_type(end(), false); // makeInsertRet(bend(), v, false);
    }
}

template<class DeamTableData> template <class F, class ... Types>
inline typename DeamTableHandle<DeamTableData>::insert_return_type
DeamTableHandle<DeamTableData>::update(const key_type& k, F f, Types&& ... args)
{
    // int v = -1;
    // basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    // std::tie (v, result) = execute(
    //     [](HashPtrRef_t t, const key_type& k, F f, Types&& ... args)
    //     ->std::pair<int,basetable_insert_return_type>
    //     {
    //         std::pair<int,basetable_insert_return_type> result =
    //             std::make_pair(t->version,
    //                            t->update_intern(k,f,std::forward<Types>(args)...));
    //         return result;
    //     },k,f,std::forward<Types>(args)...);

    basetable_iterator it;
    ReturnCode         c = ReturnCode::ERROR;
    std::tie(it, c) = table.update_intern(k,f,std::forward<Types>(args)...);

    switch(c)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return insert_return_type(it, true); // makeInsertRet(result.first, v, true);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        return update(k,f, std::forward<Types>(args)...);
    default:
        return insert_return_type(end(), false); // makeInsertRet(bend(), v, false);
    }
}

template<class DeamTableData> template <class F, class ... Types>
inline typename DeamTableHandle<DeamTableData>::insert_return_type
DeamTableHandle<DeamTableData>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    // int v = -1;
    // basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    // std::tie (v, result) = execute(
    //     [](HashPtrRef_t t, const key_type& k, F f, Types&& ... args)
    //     ->std::pair<int,basetable_insert_return_type>
    //     {
    //         std::pair<int,basetable_insert_return_type> result =
    //             std::make_pair(t->version,
    //                            t->update_unsafe_intern(k,f,std::forward<Types>(args)...));
    //         return result;
    //     },k,f,std::forward<Types>(args)...);

    basetable_iterator it;
    ReturnCode         c = ReturnCode::ERROR;
    std::tie(it, c) = table.update_unsafe_intern(k,f,std::forward<Types>(args)...);

    switch(c)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return insert_return_type(it, true); // makeInsertRet(result.first, v, true);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        return update_unsafe(k,f, std::forward<Types>(args)...);
    default:
        return insert_return_type(it, false); // makeInsertRet(bend(), v, false);
    }
}


template<class DeamTableData> template <class F, class ... Types>
inline typename DeamTableHandle<DeamTableData>::insert_return_type
DeamTableHandle<DeamTableData>::insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    // int v = -1;
    // basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    // std::tie (v, result) = execute(
    //     [](HashPtrRef_t t, const key_type& k, const mapped_type& d, F f, Types&& ... args)
    //     ->std::pair<int,basetable_insert_return_type>
    //     {
    //         std::pair<int,basetable_insert_return_type> result =
    //             std::make_pair(t->version,
    //                            t->insertOrUpdate_intern(k,d,f,std::forward<Types>(args)...));
    //         return result;
    //     },k,d,f,std::forward<Types>(args)...);

    basetable_iterator it;
    ReturnCode         c = ReturnCode::ERROR;
    std::tie(it, c) = table.insertOrUpdate_intern(k,f,std::forward<Types>(args)...);

    switch(c)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted();
        return insert_return_type(it, true); // makeInsertRet(result.first, v, true);
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return insert_return_type(it, false); // makeInsertRet(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        table.grow();
        return insertOrUpdate(k,d,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        return insertOrUpdate(k,d,f, std::forward<Types>(args)...);
    default:
        return insert_return_type(end(), false); // makeInsertRet(bend(), v, false);
    }
}

template<class DeamTableData> template <class F, class ... Types>
inline typename DeamTableHandle<DeamTableData>::insert_return_type
DeamTableHandle<DeamTableData>::insertOrUpdate_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    // int v = -1;
    // basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    // std::tie (v, result) = execute(
    //     [](HashPtrRef_t t, const key_type& k, const mapped_type& d, F f, Types&& ... args)
    //     ->std::pair<int,basetable_insert_return_type>
    //     {
    //         std::pair<int,basetable_insert_return_type> result =
    //             std::make_pair(t->version,
    //                            t->insertOrUpdate_unsafe_intern(k,d,f,std::forward<Types>(args)...));
    //         return result;
    //     },k,d,f,std::forward<Types>(args)...);

    basetable_iterator it;
    ReturnCode         c = ReturnCode::ERROR;
    std::tie(it, c) = table.insertOrUpdate_unsafe_intern(k,f,std::forward<Types>(args)...);

    switch(c)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted();
        return insert_return_type(it, true); // makeInsertRet(result.first, v, true);
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return insert_return_type(it, false); // makeInsertRet(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        table.grow();
        return insertOrUpdate(k,d,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        return insertOrUpdate(k,d,f, std::forward<Types>(args)...);
    default:
        return insert_return_type(end(), false); // makeInsertRet(bend(), v, false);
    }
}

template<class DeamTableData>
inline typename DeamTableHandle<DeamTableData>::iterator
DeamTableHandle<DeamTableData>::find(const key_type& k)
{
    // int v = -1;
    // basetable_iterator bit = bend();
    // std::tie (v, bit) = execute([](HashPtrRef_t t, const key_type & k) -> std::pair<int, basetable_iterator>
    //                             { return std::make_pair<int, basetable_iterator>(t->version, t->find(k)); },
    //                  k);
    // return makeIterator(bit, v);

    return table.find(k);
}

template<class DeamTableData>
inline typename DeamTableHandle<DeamTableData>::const_iterator
DeamTableHandle<DeamTableData>::find(const key_type& k) const
{
    // int v = -1;
    // basetable_citerator bit = bcend();
    // std::tie (v, bit) = cexecute([](HashPtrRef_t t, const key_type & k) -> std::pair<int, basetable_citerator>
    //                             { return std::make_pair<int, basetable_iterator>(t->version, t->find(k)); },
    //                  k);
    // return makeCIterator(bit, v);
    return table.find(k);
}

template<class DeamTableData>
inline typename DeamTableHandle<DeamTableData>::size_type
DeamTableHandle<DeamTableData>::erase(const key_type& k)
{
    // int v = -1;
    // ReturnCode result = ReturnCode::ERROR;
    // std::tie (v, result) = execute([](HashPtrRef_t t, const key_type& k)
    //                                  ->std::pair<int,ReturnCode>
    //                                {
    //                                    std::pair<int,ReturnCode> result =
    //                                        std::make_pair(t->version,
    //                                                       t->erase_intern(k));
    //                                    return result;
    //                                },
    //                                k);

    ReturnCode c = table.erase_intern(k);

    switch(c)
    {
    case ReturnCode::SUCCESS_DEL:
        inc_deleted();
        return 1;
    case ReturnCode::TSX_SUCCESS_DEL:
        inc_deleted();  // TSX DELETION COULD BE USED TO AVOID DUMMIES => dec_inserted()
        return 1;
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        return erase(k);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return 0;
    default:
        return 0;
    }
}






// ITERATOR FUNCTIONALITY ******************************************************

template<class DeamTableData>
inline typename DeamTableHandle<DeamTableData>::iterator
DeamTableHandle<DeamTableData>::begin()
{
    return table.begin();
}
template<class DeamTableData>
inline typename DeamTableHandle<DeamTableData>::iterator
DeamTableHandle<DeamTableData>::end()
{
    // return iterator(basetable_iterator(std::pair<key_type, mapped_type>(key_type(), mapped_type()),
    //                                    nullptr,nullptr),
    //              0, *this);
    return table.end();
}

template<class DeamTableData>
inline typename DeamTableHandle<DeamTableData>::const_iterator
DeamTableHandle<DeamTableData>::cbegin() const
{
    // return cexecute([](HashPtrRef_t t, const DeamTableHandle& gt)
    //                 -> const_iterator
    //                 {
    //                   return const_iterator(t->cbegin(), t->version, gt);
    //                 }, *this);
    return table.cbegin();
}
template<class DeamTableData>
inline typename DeamTableHandle<DeamTableData>::const_iterator
DeamTableHandle<DeamTableData>::cend()  const
{
    // return const_iterator(basetable_citerator(std::pair<key_type, mapped_type>(key_type(), mapped_type()),
    //                                    nullptr,nullptr), 0, *this);
    return table.cend();
}





// COUNTING FUNCTIONALITY ******************************************************

template<class DeamTableData>
inline void DeamTableHandle<DeamTableData>::update_numbers()
{
    counts.updates  = 0;

    dtData.dummies.fetch_add(counts.deleted,std::memory_order_relaxed);

    auto temp       = dtData.elements.fetch_add(counts.inserted, std::memory_order_acquire);
    temp           += counts.inserted;
    auto thresh      = dtData.grow_thresh.load(std::memory_order_acquire);

    while (temp > thresh)
    {
        if (dtData.grow_thresh.compare_exchange_weak(thresh, thresh+DeamTableData::n_elem_grow))
        {
            table.grow();
        }
    }
    counts.set(counts.version, 0,0,0);
}

template<class DeamTableData>
inline void DeamTableHandle<DeamTableData>::inc_inserted()
{

    ++counts.inserted;
    if (++counts.updates > 64)
    {
        update_numbers();
    }
}


template<class DeamTableData>
inline void DeamTableHandle<DeamTableData>::inc_deleted()
{
    ++counts.deleted;
    if (++counts.updates > 64)
    {
        update_numbers();
    }
}

template <typename DeamTableData>
inline typename DeamTableHandle<DeamTableData>::size_type
DeamTableHandle<DeamTableData>::element_count_unsafe()
{
    int temp = dtData.elements.load();
    temp    -= dtData.dummies.load();
    temp    += dtData.handle_ptr.forall([](This_t* h, int res)
                                        {
                                            int temp = res;
                                            temp += h->counts.inserted;
                                            temp -= h->counts.deleted;
                                            return temp;
                                        });
    return temp;
}

}
