/*******************************************************************************
 * data-structures/grow_table.h
 *
 * Defines the growtable architecture:
 *   GrowTable       - the global facade of our table
 *   GrowTableData   - the actual global object (immovable core)
 *   GrowTableHandle - local handles on the global object (thread specific)
 * The behavior of GrowTable can be specified using the Worker- and Exclusion-
 * strategies. They have significant influence esp. on how the table is grown.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <atomic>
#include <memory>
//#include <iostream>

#include "data-structures/returnelement.h"
#include "data-structures/grow_iterator.h"
#include "example/update_fcts.h"
#include "utils/concurrentptrarray.h"

namespace growt {

static const size_t migration_block_size = 4096;

template<class Table_t>
size_t blockwise_migrate(Table_t source, Table_t target)
{
    size_t n = 0;

    //get block + while block legal migrate and get new block
    size_t temp = source->_current_copy_block.fetch_add(migration_block_size);
    while (temp < source->_capacity)
    {
        n += source->migrate(*target, temp,
                             std::min(uint(temp+migration_block_size),
                                      uint(source->_capacity)));
        temp = source->_current_copy_block.fetch_add(migration_block_size);
    }
    return n;
}

template<size_t MxFill>
size_t resize(size_t curr_size, size_t n_in, size_t n_del)
{
    auto nsize = curr_size;
    double fill_rate = double(n_in - n_del)/double(curr_size);

    if (fill_rate > double(MxFill)/200.) nsize <<= 1;

    return nsize;
}





// FORWARD DECLARATION OF THE HANDLE CLASS
template<class> class GrowTableHandle;
// AND THE STATIONARY DATA OBJECT (GLOBAL OBJECT ON HEAP)
template<class> class GrowTableData;

template<class                  HashTable,
         template <class> class WorkerStrat,
         template <class> class ExclusionStrat>
class GrowTable
{
private:
    using This_t           = GrowTable<HashTable,
                                       WorkerStrat,
                                       ExclusionStrat>;
    using GTD_t            = GrowTableData<This_t>;
    using BaseTable_t      = HashTable;
    using WorkerStrat_t    = WorkerStrat<GTD_t>;
    using ExclusionStrat_t = ExclusionStrat<GTD_t>;
    friend GTD_t;

    //const double max_fill  = MaxFill/100.;

    std::unique_ptr<GTD_t> gtData;

public:
    using Handle           = GrowTableHandle<GTD_t>;
    friend Handle;

    GrowTable (size_t size) : gtData(new GTD_t(size)) { }

    GrowTable (const GrowTable& source)            = delete;
    GrowTable& operator= (const GrowTable& source) = delete;

    GrowTable (GrowTable&& source)            = default;
    GrowTable& operator= (GrowTable&& source) = default;

    ~GrowTable() = default;

    Handle getHandle()
    {
        return Handle(*gtData);
    }

};





// GLOBAL TABLE OBJECT (THIS CANNOT BE USED WITHOUT CREATING A HANDLE)
template<typename Parent>
class GrowTableData
{
private:
    // TYPEDEFS
    using Parent_t         = Parent;
    using BaseTable_t      = typename Parent::BaseTable_t;
    using WorkerStrat_t    = typename Parent::WorkerStrat_t;
    using ExclusionStrat_t = typename Parent::ExclusionStrat_t;

    friend WorkerStrat_t;
    friend ExclusionStrat_t;

public:
    using size_type        = size_t;
    using Handle           = typename Parent::Handle;
    friend Handle;


    GrowTableData(size_type size_)
        : global_exclusion(size_), global_worker(), // handle_ptr(64),
          elements(0), dummies(0), grow_count(0)
    { }

    GrowTableData(const GrowTableData& source) = delete;
    GrowTableData& operator=(const GrowTableData& source) = delete;
    GrowTableData(GrowTableData&&) = delete;
    GrowTableData& operator=(GrowTableData&&) = delete;
    ~GrowTableData() = default;

    size_type element_count_approx() { return elements.load()-dummies.load(); }

private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    mutable typename ExclusionStrat_t::global_data_t global_exclusion;
    mutable typename WorkerStrat_t   ::global_data_t global_worker;

    // mutable ConcurrentPtrArray<Handle> handle_ptr;

    // APPROXIMATE COUNTS
    alignas(64) std::atomic_int elements;
    alignas(64) std::atomic_int dummies;
    alignas(64) std::atomic_int grow_count;
};




// HANDLE OBJECTS EVERY THREAD HAS TO CREATE ONE HANDLE (THEY CANNOT BE SHARED)
template<class GrowTableData>
class GrowTableHandle
{
private:
    using This_t             = GrowTableHandle<GrowTableData>;
    using Parent_t           = typename GrowTableData::Parent_t;
    using BaseTable_t        = typename GrowTableData::BaseTable_t;
    using WorkerStrat_t      = typename GrowTableData::WorkerStrat_t;
    using ExclusionStrat_t   = typename GrowTableData::ExclusionStrat_t;
    friend GrowTableData;

public:
    using HashPtrRef_t       = typename ExclusionStrat_t::HashPtrRef;
    using value_intern       = typename BaseTable_t::value_intern;

    using key_type           = typename BaseTable_t::key_type;
    using mapped_type        = typename BaseTable_t::mapped_type;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = IteratorGrowT<This_t, false>;//value_intern*;
    using const_iterator     = IteratorGrowT<This_t, true>;//IteratorGrowT<This_t, true>;
    using size_type          = size_t;
    using difference_type    = std::ptrdiff_t;
    using reference          = ReferenceGrowT<This_t, false>;
    using const_reference    = ReferenceGrowT<This_t, true>;
    using mapped_reference       = MappedRefGrowT<This_t, false>;
    using const_mapped_reference = MappedRefGrowT<This_t, true>;
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
    GrowTableHandle() = delete;
    GrowTableHandle(GrowTableData &data);
    GrowTableHandle(Parent_t      &parent);

    GrowTableHandle(const GrowTableHandle& source) = delete;
    GrowTableHandle& operator=(const GrowTableHandle& source) = delete;

    GrowTableHandle(GrowTableHandle&& source);
    GrowTableHandle& operator=(GrowTableHandle&& source);

    ~GrowTableHandle();

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
        { return (*(insert(k, mapped_type()).first)).second; }

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


    size_type element_count_approx() { return gtData.element_count_approx(); }
    //size_type element_count_unsafe();

private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    GrowTableData& gtData;
    size_type      handle_id;
    mutable typename WorkerStrat_t   ::local_data_t local_worker;
    mutable typename ExclusionStrat_t::local_data_t local_exclusion;


    inline void         grow()     const { local_exclusion.grow(); }
    inline void         helpGrow() const { local_exclusion.helpGrow(); }
    inline void         rlsTable() const { local_exclusion.rlsTable(); }
    inline HashPtrRef_t getTable() const { return local_exclusion.getTable(); }

    template<typename Functor, typename ... Types>
    inline typename std::result_of<Functor(HashPtrRef_t, Types&& ...)>::type
    execute (Functor f, Types&& ... param)
    {
        HashPtrRef_t temp = local_exclusion.getTable();
        auto result = std::forward<Functor>(f)
                          (temp, std::forward<Types>(param)...);
        rlsTable();
        return result;
    }

    template<typename Functor, typename ... Types>
    inline typename std::result_of<Functor(HashPtrRef_t, Types&& ...)>::type
    cexecute (Functor f, Types&& ... param) const
    {
        HashPtrRef_t temp = local_exclusion.getTable();
        auto result = std::forward<Functor>(f)
                          (temp, std::forward<Types>(param)...);
        rlsTable();
        return result;
    }

    inline iterator makeIterator(const basetable_iterator& bit, size_t version)
    { return iterator(bit, version, *this); }
    inline insert_return_type makeInsertRet(const basetable_iterator& bit,
                                            size_t version, bool inserted)
    { return std::make_pair(iterator(bit, version, *this), inserted); }
    inline basetable_iterator bend()
    { return basetable_iterator (std::make_pair(key_type(), mapped_type()), nullptr, nullptr);}
    inline basetable_iterator bcend()
    { return basetable_citerator(std::make_pair(key_type(), mapped_type()), nullptr, nullptr);}

    double max_fill_factor;

    // LOCAL COUNTERS FOR SIZE ESTIMATION WITH SOME PADDING FOR
    // REDUCING CACHE EFFECTS
public:
    void update_numbers();

private:
    void inc_inserted(int v);
    void inc_deleted(int v);

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

public:
    using range_iterator       = typename BaseTable_t::range_iterator;
    using const_range_iterator = typename BaseTable_t::const_range_iterator;

    /* size has to divide capacity */
    range_iterator       range (size_t rstart, size_t rend)
    {
        range_iterator result = execute([rstart, rend](HashPtrRef_t tab)
                                        { return tab->range(rstart,rend); });
        return result;
    }
    const_range_iterator crange(size_t rstart, size_t rend)
    {
        const_range_iterator result = cexecute([rstart, rend](HashPtrRef_t tab)
                                        { return tab->crange(rstart,rend); });
        return result;
    }
    range_iterator       range_end ()       { return  bend(); }
    const_range_iterator range_cend() const { return bcend(); }
    size_t               capacity()   const
    {
        size_t cap = cexecute([](HashPtrRef_t tab) { return tab->_capacity; });
        return cap;
    }

};







// CONSTRUCTORS AND ASSIGNMENTS ************************************************

template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(GrowTableData &data)
    : gtData(data), local_worker(data), local_exclusion(data, local_worker),
      max_fill_factor(0.666),
      counts()
{
    //handle_id = gtData.handle_ptr.push_back(this);

    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    local_exclusion.init();
    local_worker   .init(local_exclusion);
}

template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(Parent_t      &parent)
    : gtData(*(parent.gtData)), local_worker(*(parent.gtData)),
      local_exclusion(*(parent.gtData), local_worker),
      max_fill_factor(0.666),
      counts()
{
    //handle_id = gtData.handle_ptr.push_back(this);

    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    local_exclusion.init();
    local_worker   .init(local_exclusion);
}



template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(GrowTableHandle&& source)
    : gtData(source.gtData), handle_id(source.handle_id),
      local_worker(std::move(source.local_worker)),
      local_exclusion(std::move(source.local_exclusion)),
      max_fill_factor(source.max_fill_factor),
      counts(std::move(source.counts))
{
    source.counts = LocalCount();
    //gtData.handle_ptr.update(handle_id, this);
    //source.handle_id = std::numeric_limits<size_t>::max();
}

template<class GrowTableData>
GrowTableHandle<GrowTableData>&
GrowTableHandle<GrowTableData>::operator=(GrowTableHandle&& source)
{
    if (this == &source) return *this;

    this->~GrowTableHandle();
    new (this) GrowTableHandle(std::move(source));
    return *this;
}



template<class GrowTableData>
GrowTableHandle<GrowTableData>::~GrowTableHandle()
{

    // if (handle_id < std::numeric_limits<size_t>::max())
    // {
    //     gtData.handle_ptr.remove(handle_id);
    // }
    if (counts.version >= 0)
    {
        update_numbers();
    }

    local_worker   .deinit();
    local_exclusion.deinit();
}









// MAIN HASH TABLE FUNCTIONALITY ***********************************************

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::insert(const key_type& k, const mapped_type& d)
{
    int v = -1;
    basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);
    std::tie (v, result) = execute([](HashPtrRef_t t, const key_type& k, const mapped_type& d)
                                     ->std::pair<int,basetable_insert_return_type>
                                   {
                                       std::pair<int,basetable_insert_return_type> result =
                                           std::make_pair(t->_version,
                                                          t->insert_intern(k,d));
                                       return result;
                                   }, k,d);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
        return makeInsertRet(result.first, v, true);
    case ReturnCode::UNSUCCESS_ALREADY_USED:
    case ReturnCode::TSX_UNSUCCESS_ALREADY_USED:
        return makeInsertRet(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insert(k,d);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return insert(k,d);
    default:
        return makeInsertRet(bend(), v, false);
    }
}

template<class GrowTableData> template <class F, class ... Types>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::update(const key_type& k, F f, Types&& ... args)
{
    int v = -1;
    basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](HashPtrRef_t t, const key_type& k, F f, Types&& ... args)
        ->std::pair<int,basetable_insert_return_type>
        {
            std::pair<int,basetable_insert_return_type> result =
                std::make_pair(t->_version,
                               t->update_intern(k,f,std::forward<Types>(args)...));
            return result;
        },k,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return makeInsertRet(result.first, v, true);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        //std::cout << "!" << std::flush;
        return makeInsertRet(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();  // usually impossible as this collides with NOT_FOUND
        return update(k,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return update(k,f, std::forward<Types>(args)...);
    default:
        return makeInsertRet(bend(), v, false);
    }
}

template<class GrowTableData> template <class F, class ... Types>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    int v = -1;
    basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](HashPtrRef_t t, const key_type& k, F f, Types&& ... args)
        ->std::pair<int,basetable_insert_return_type>
        {
            std::pair<int,basetable_insert_return_type> result =
                std::make_pair(t->_version,
                               t->update_unsafe_intern(k,f,std::forward<Types>(args)...));
            return result;
        },k,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return makeInsertRet(result.first, v, true);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return makeInsertRet(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return update_unsafe(k,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return update_unsafe(k,f, std::forward<Types>(args)...);
    default:
        return makeInsertRet(bend(), v, false);
    }
}


template<class GrowTableData> template <class F, class ... Types>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    int v = -1;
    basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](HashPtrRef_t t, const key_type& k, const mapped_type& d, F f, Types&& ... args)
        ->std::pair<int,basetable_insert_return_type>
        {
            std::pair<int,basetable_insert_return_type> result =
                std::make_pair(t->_version,
                               t->insertOrUpdate_intern(k,d,f,std::forward<Types>(args)...));
            return result;
        },k,d,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
        return makeInsertRet(result.first, v, true);
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return makeInsertRet(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insertOrUpdate(k,d,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return insertOrUpdate(k,d,f, std::forward<Types>(args)...);
    default:
        return makeInsertRet(bend(), v, false);
    }
}

template<class GrowTableData> template <class F, class ... Types>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::insertOrUpdate_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    int v = -1;
    basetable_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](HashPtrRef_t t, const key_type& k, const mapped_type& d, F f, Types&& ... args)
        ->std::pair<int,basetable_insert_return_type>
        {
            std::pair<int,basetable_insert_return_type> result =
                std::make_pair(t->_version,
                               t->insertOrUpdate_unsafe_intern(k,d,f,std::forward<Types>(args)...));
            return result;
        },k,d,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
        return makeInsertRet(result.first, v, true);
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return makeInsertRet(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insertOrUpdate_unsafe(k,d,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return insertOrUpdate_unsafe(k,d,f, std::forward<Types>(args)...);
    default:
        return makeInsertRet(bend(), v, false);
    }
}

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::iterator
GrowTableHandle<GrowTableData>::find(const key_type& k)
{
    int v = -1;
    basetable_iterator bit = bend();
    std::tie (v, bit) = execute([](HashPtrRef_t t, const key_type & k) -> std::pair<int, basetable_iterator>
                                { return std::make_pair<int, basetable_iterator>(t->_version, t->find(k)); },
                     k);
    return makeIterator(bit, v);
}

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::const_iterator
GrowTableHandle<GrowTableData>::find(const key_type& k) const
{
    int v = -1;
    basetable_citerator bit = bcend();
    std::tie (v, bit) = cexecute([](HashPtrRef_t t, const key_type & k) -> std::pair<int, basetable_citerator>
                                { return std::make_pair<int, basetable_iterator>(t->_version, t->find(k)); },
                     k);
    return makeCIterator(bit, v);
}

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::size_type
GrowTableHandle<GrowTableData>::erase(const key_type& k)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;
    std::tie (v, result) = execute([](HashPtrRef_t t, const key_type& k)
                                     ->std::pair<int,ReturnCode>
                                   {
                                       std::pair<int,ReturnCode> result =
                                           std::make_pair(t->_version,
                                                          t->erase_intern(k));
                                       return result;
                                   },
                                   k);

    switch(result)
    {
    case ReturnCode::SUCCESS_DEL:
        inc_deleted(v);
        return 1;
    case ReturnCode::TSX_SUCCESS_DEL:
        inc_deleted(v);  // TSX DELETION COULD BE USED TO AVOID DUMMIES => dec_inserted()
        return 1;
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return erase(k);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return 0;
    default:
        return 0;
    }
}






// ITERATOR FUNCTIONALITY ******************************************************

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::iterator
GrowTableHandle<GrowTableData>::begin()
{
    return execute([](HashPtrRef_t t, GrowTableHandle& gt)
                   -> iterator
                   {
                       return iterator(t->begin(), t->_version, gt);
                   }, *this);
}
template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::iterator
GrowTableHandle<GrowTableData>::end()
{
    return iterator(basetable_iterator(std::pair<key_type, mapped_type>(key_type(), mapped_type()),
                                       nullptr,nullptr),
                    0, *this);
}

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::const_iterator
GrowTableHandle<GrowTableData>::cbegin() const
{
    // return begin();
    return cexecute([](HashPtrRef_t t, const GrowTableHandle& gt)
                    -> const_iterator
                    {
                      return const_iterator(t->cbegin(), t->_version, gt);
                    }, *this);
}
template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::const_iterator
GrowTableHandle<GrowTableData>::cend()  const
{
    // return end();
    return const_iterator(basetable_citerator(std::pair<key_type, mapped_type>(key_type(), mapped_type()),
                                       nullptr,nullptr), 0, *this);
}





// COUNTING FUNCTIONALITY ******************************************************

template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::update_numbers()
{
    counts.updates  = 0;

    auto table = getTable();
    if (table->_version != size_t(counts.version))
    {
        counts.set(table->_version, 0,0,0);
        rlsTable();
        return;
    }

    gtData.dummies.fetch_add(counts.deleted,std::memory_order_relaxed);

    auto temp       = gtData.elements.fetch_add(counts.inserted, std::memory_order_relaxed);
    temp           += counts.inserted;

    if (temp  > table->_capacity * max_fill_factor)
    {
        rlsTable();
        grow();
    }
    rlsTable();
    counts.set(counts.version, 0,0,0);
}

template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::inc_inserted(int v)
{
    if (counts.version == v)
    {
        ++counts.inserted;
        if (++counts.updates > 64)
        {
            update_numbers();
        }
    }
    else
    {
        counts.set(v,1,1,0);
    }
}


template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::inc_deleted(int v)
{
    if (counts.version == v)
    {
        ++counts.deleted;
        if (++counts.updates > 64)
        {
            update_numbers();
        }
    }
    else
    {
        counts.set(v,1,0,1);
    }
}

// template <typename GrowTableData>
// inline typename GrowTableHandle<GrowTableData>::size_type
// GrowTableHandle<GrowTableData>::element_count_unsafe()
// {
//     int v = getTable()->version;
//     rlsTable();

//     int temp = gtData.elements.load();
//     temp    -= gtData.dummies.load();
//     temp    += gtData.handle_ptr.forall([v](This_t* h, int res)
//                                         {
//                                             if (h->counts.version != v)
//                                             {
//                                                 return res;
//                                             }
//                                             int temp = res;
//                                             temp += h->counts.inserted;
//                                             temp -= h->counts.deleted;
//                                             return temp;
//                                         });
//     return temp;
// }

}
