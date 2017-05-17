/*******************************************************************************
 * data-structures/growtable.h
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

#ifndef GROWTABLE_H
#define GROWTABLE_H

#include "data-structures/returnelement.h"
#include "utils/concurrentptrarray.h"

#include <atomic>
#include <memory>
#include <iostream>

namespace growt {

static const size_t migration_block_size = 4096;

template<class Table_t>
size_t blockwise_migrate(Table_t source, Table_t target)
{
    size_t n = 0;

    //get block + while block legal migrate and get new block
    size_t temp = source->currentCopyBlock.fetch_add(migration_block_size);
    while (temp < source->size)
    {
        n += source->migrate(*target, temp,
                             std::min(uint(temp+migration_block_size),
                                      uint(source->size)));
        temp = source->currentCopyBlock.fetch_add(migration_block_size);
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
    using SeqTable_t       = HashTable;
    using WorkerStrat_t    = WorkerStrat<GTD_t>;
    using ExclusionStrat_t = ExclusionStrat<GTD_t>;
    friend GTD_t;

    //const double max_fill  = MaxFill/100.;

    std::unique_ptr<GTD_t> gtData;

public:
    using Element          = ReturnElement;
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
    using SeqTable_t       = typename Parent::SeqTable_t;
    using WorkerStrat_t    = typename Parent::WorkerStrat_t;
    using ExclusionStrat_t = typename Parent::ExclusionStrat_t;

    friend WorkerStrat_t;
    friend ExclusionStrat_t;

public:
    using Handle           = typename Parent::Handle;
    using Element          = ReturnElement;
    using Key              = typename Element::Key;
    using Data             = typename Element::Data;

    friend Handle;

    GrowTableData(size_t size_)
        : global_exclusion(size_), global_worker(), handle_ptr(64),
          elements(0), dummies(0), grow_count(0)
    { }

    GrowTableData(const GrowTableData& source) = delete;
    GrowTableData& operator=(const GrowTableData& source) = delete;
    GrowTableData(GrowTableData&&) = delete;
    GrowTableData& operator=(GrowTableData&&) = delete;
    ~GrowTableData() = default;

    size_t element_count_approx();
private:

    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    typename ExclusionStrat_t::global_data_t global_exclusion;
    typename WorkerStrat_t   ::global_data_t global_worker;

    ConcurrentPtrArray<Handle> handle_ptr;

    // APPROXIMATE COUNTS
    alignas(64) std::atomic_int elements;
    alignas(64) std::atomic_int dummies;
    alignas(64) std::atomic_int grow_count;
};

template <typename Parent>
size_t GrowTableData<Parent>::element_count_approx()
{
    return elements.load() - dummies.load();
}

// HANDLE OBJECTS EVERY THREAD HAS TO CREATE ONE HANDLE (THEY CANNOT BE SHARED)
template<class GrowTableData>
class GrowTableHandle
{
private:
    // TYPEDEFS
    using This_t           = GrowTableHandle<GrowTableData>;
    using Parent_t         = typename GrowTableData::Parent_t;
    using WorkerStrat_t    = typename GrowTableData::WorkerStrat_t;
    using ExclusionStrat_t = typename GrowTableData::ExclusionStrat_t;
    using HashPtrRef_t     = typename ExclusionStrat_t::HashPtrRef;
    using InternElement_t  = typename GrowTableData::SeqTable_t::InternElement_t;

    friend GrowTableData;

public:
    using Element          = ReturnElement;
    using Key              = typename GrowTableData::Key;
    using Data             = typename GrowTableData::Data;

    GrowTableHandle() = delete;
    GrowTableHandle(GrowTableData &data);
    GrowTableHandle(Parent_t      &parent);

    GrowTableHandle(const GrowTableHandle& source) = delete;
    GrowTableHandle& operator=(const GrowTableHandle& source) = delete;

    GrowTableHandle(GrowTableHandle&& source);
    GrowTableHandle& operator=(GrowTableHandle&& source);

    ~GrowTableHandle();

    ReturnCode    insert(const Key k, const Data d);
    template <class F>
    ReturnCode    update(const Key k, const Data d, F f);
    template <class F>
    ReturnCode    insertOrUpdate(const Key k, const Data d, F f);
    ReturnElement find(const Key & k);
    ReturnCode    remove(const Key & k);

    template <class F>
    ReturnCode    update_unsafe(const Key k, const Data d, F f);
    template <class F>
    ReturnCode    insertOrUpdate_unsafe(const Key k, const Data d, F f);

    size_t element_count_approx() { return gtData.element_count_approx(); }
    size_t element_count_unsafe();

private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    GrowTableData& gtData;
    size_t handle_id;
    typename WorkerStrat_t   ::local_data_t local_worker;
    typename ExclusionStrat_t::local_data_t local_exclusion;


    inline void         grow()     { local_exclusion.grow(); }
    inline void         helpGrow() { local_exclusion.helpGrow(); }
    inline void         rlsTable() { local_exclusion.rlsTable(); }
    inline HashPtrRef_t getTable() { return local_exclusion.getTable(); }

    template<typename Functor, typename ... Types>
    typename std::result_of<Functor(HashPtrRef_t, Types&& ...)>::type
    execute (Functor f, Types&& ... param)
    {
        HashPtrRef_t temp = local_exclusion.getTable();
        auto result = std::forward<Functor>(f)
                          (temp, std::forward<Types>(param)...);
        rlsTable();
        return result;
    }

    ReturnCode insert(const InternElement_t & e);
    template <class F>
    ReturnCode update(const InternElement_t & e, F f);
    template <class F>
    ReturnCode insertOrUpdate(const InternElement_t & e, F f);
    template <class F>
    ReturnCode update_unsafe(const InternElement_t & e, F f);
    template <class F>
    ReturnCode insertOrUpdate_unsafe(const InternElement_t & e, F f);

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
            : version(rhs.version), updates(rhs.updates), inserted(rhs.inserted), deleted(rhs.deleted)
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


template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(GrowTableData &data)
    : gtData(data), local_worker(data), local_exclusion(data, local_worker),
      max_fill_factor(0.666),
      counts()
{
    handle_id = gtData.handle_ptr.push_back(this);

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
    handle_id = gtData.handle_ptr.push_back(this);

    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    local_exclusion.init();
    local_worker   .init(local_exclusion);
}


template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(GrowTableHandle&& source) :
    gtData(source.gtData), handle_id(source.handle_id),
    local_worker(std::move(source.local_worker)),
    local_exclusion(std::move(source.local_exclusion)),
    max_fill_factor(source.max_fill_factor),
    counts(std::move(source.counts))
{
    gtData.handle_ptr.update(handle_id, this);
    source.handle_id = std::numeric_limits<size_t>::max();
}

template<class GrowTableData>
GrowTableHandle<GrowTableData>& GrowTableHandle<GrowTableData>::operator=(GrowTableHandle&& source)
{
    if (this == &source) return *this;

    this->~GrowTableHandle();
    new (this) GrowTableHandle(std::move(source));
    return *this;
}



template<class GrowTableData>
GrowTableHandle<GrowTableData>::~GrowTableHandle()
{

    if (handle_id < std::numeric_limits<size_t>::max())
    {
        gtData.handle_ptr.remove(handle_id);
    }
    if (counts.version >= 0)
    {
        update_numbers();
    }

    local_worker   .deinit();
    local_exclusion.deinit();
}


template<class GrowTableData>
inline ReturnCode GrowTableHandle<GrowTableData>::insert(const Key k, const Data d){
   return insert(InternElement_t(k,d));  }

template<class GrowTableData>
inline ReturnCode GrowTableHandle<GrowTableData>::insert(const InternElement_t& e)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;
    std::tie (v, result) = execute([](HashPtrRef_t t, const InternElement_t& e)->std::pair<int,ReturnCode>
                                   {
                                       std::pair<int, ReturnCode> result =
                                           std::make_pair(t->version,
                                                          t->insert(e));
                                       return result;
                                   },
                                   e);

    switch(result)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
    case ReturnCode::UNSUCCESS_ALREADY_USED:
    case ReturnCode::TSX_UNSUCCESS_ALREADY_USED:
        return result;
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insert(e);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return insert(e);
    default:
        return ReturnCode::ERROR;
    }
}

template<class GrowTableData> template <class F>
inline ReturnCode GrowTableHandle<GrowTableData>::update(const Key k, const Data d, F f)
{   return update(InternElement_t(k,d), f);  }

template<class GrowTableData> template <class F>
inline ReturnCode GrowTableHandle<GrowTableData>::update(const InternElement_t& e, F f)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;

    std::tie (v, result) = execute([](HashPtrRef_t t, const InternElement_t& e, F f)->std::pair<int,ReturnCode>
                                   {
                                       std::pair<int, ReturnCode> result =
                                           std::make_pair(t->version,
                                                          t->update(e, f));
                                       return result;
                                   },
                                   e, f);

    switch(result)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return result;
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();  // usually impossible as this collides with NOT_FOUND
        return update(e, f);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        return update(e, f);
    default:
        return ReturnCode::ERROR;
    }
}

template<class GrowTableData> template <class F>
inline ReturnCode GrowTableHandle<GrowTableData>::insertOrUpdate(const Key k, const Data d, F f)
{   return insertOrUpdate(InternElement_t(k,d), f);  }

template<class GrowTableData> template <class F>
inline ReturnCode GrowTableHandle<GrowTableData>::insertOrUpdate(const InternElement_t& e, F f)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;

    std::tie (v, result) = execute([](HashPtrRef_t t, const InternElement_t& e, F f)->std::pair<int,ReturnCode>
                                   {
                                       std::pair<int, ReturnCode> result =
                                           std::make_pair(t->version,
                                                          t->insertOrUpdate(e, f));
                                       return result;
                                   },
                                   e, f);

    switch(result)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
        return result;
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return result;
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insertOrUpdate(e, f);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return insertOrUpdate(e, f);
    default:
        return ReturnCode::ERROR;
    }
}

template<class GrowTableData> template <class F>
inline ReturnCode GrowTableHandle<GrowTableData>::update_unsafe(const Key k, const Data d, F f)
{   return update_unsafe(InternElement_t(k,d), f);  }

template<class GrowTableData> template <class F>
inline ReturnCode GrowTableHandle<GrowTableData>::update_unsafe(const InternElement_t& e, F f)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;

    std::tie (v, result) = execute([](HashPtrRef_t t, const InternElement_t& e, F f)->std::pair<int,ReturnCode>
                                   {
                                       std::pair<int, ReturnCode> result =
                                           std::make_pair(t->version,
                                                          t->update_unsafe(e, f));
                                       return result;
                                   },
                                   e, f);

    switch(result)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return result;
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return update_unsafe(e, f);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        return update_unsafe(e, f);
    default:
        return ReturnCode::ERROR;
    }
}

template<class GrowTableData> template <class F>
inline ReturnCode GrowTableHandle<GrowTableData>::insertOrUpdate_unsafe(const Key k, const Data d, F f)
{   return insertOrUpdate_unsafe(InternElement_t(k,d), f);  }

template<class GrowTableData> template <class F>
inline ReturnCode GrowTableHandle<GrowTableData>::insertOrUpdate_unsafe(const InternElement_t& e, F f)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;

    std::tie (v, result) = execute([](HashPtrRef_t t, const InternElement_t& e, F f)->std::pair<int,ReturnCode>
                                   {
                                       std::pair<int, ReturnCode> result =
                                           std::make_pair(t->version,
                                                          t->insertOrUpdate_unsafe(e, f));
                                       return result;
                                   },
                                   e, f);

    switch(result)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
        return result;
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return result;
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insertOrUpdate_unsafe(e, f);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return insertOrUpdate_unsafe(e, f);
    default:
        return ReturnCode::ERROR;
    }
}

template<class GrowTableData>
inline ReturnElement GrowTableHandle<GrowTableData>::
    find(const Key& k)
{
    ReturnElement e = execute([this](HashPtrRef_t t, const Key & k) -> ReturnElement
                                    { return t->find(k); },
                     k);
    return e;
}

template<class GrowTableData>
inline ReturnCode GrowTableHandle<GrowTableData>::remove(const Key& k)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;
    std::tie (v, result) = execute([](HashPtrRef_t t, const Key& k)->std::pair<int,ReturnCode>
                                   {
                                       std::pair<int, ReturnCode> result =
                                           std::make_pair(t->version,
                                                          t->remove(k));
                                       return result;
                                   },
                                   k);

    switch(result)
    {
    case ReturnCode::SUCCESS_DEL:
        inc_deleted(v);
        return result;
    case ReturnCode::TSX_SUCCESS_DEL:
        inc_deleted(v);  // TSX DELETION COULD BE USED TO AVOID DUMMIES => dec_inserted()
        return result;
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        helpGrow();
        return remove(k);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return result;
    default:
        return ReturnCode::ERROR;
    }
}

template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::update_numbers()
{
    counts.updates  = 0;

    auto table = getTable();
    if (table->version != size_t(counts.version))
    {
        counts.set(table->version, 0,0,0);
        rlsTable();
        return;
    }

    gtData.dummies.fetch_add(counts.deleted,std::memory_order_relaxed);

    auto temp       = gtData.elements.fetch_add(counts.inserted, std::memory_order_relaxed);
    temp           += counts.inserted;

    if (temp  > table->size*max_fill_factor)
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

template <typename GrowTableData>
size_t GrowTableHandle<GrowTableData>::element_count_unsafe()
{
    int v = getTable()->version;
    rlsTable();

    int temp = gtData.elements.load();
    temp    -= gtData.dummies.load();
    temp    += gtData.handle_ptr.forall([v](This_t* h, int res)
                                        {
                                            if (h->counts.version != v)
                                            {
                                                return res;
                                            }
                                            int temp = res;
                                            temp += h->counts.inserted;
                                            temp -= h->counts.deleted;
                                            return temp;
                                        });
    return temp;
}

}

#endif // GROWTABLE_H
