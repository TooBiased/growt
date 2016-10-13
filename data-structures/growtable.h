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

#include <atomic>
#include <memory>

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
        : global_exclusion(size_), global_worker(),
          elements(0), dummies(0)
    { }

    GrowTableData(const GrowTableData& source) = delete;
    GrowTableData& operator=(const GrowTableData& source) = delete;
    ~GrowTableData() = default;

private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    typename ExclusionStrat_t::global_data_t global_exclusion;
    typename WorkerStrat_t   ::global_data_t global_worker;

    // APPROXIMATE COUNTS
    alignas(64) std::atomic_int elements;
    alignas(64) std::atomic_int dummies;
};




// HANDLE OBJECTS EVERY THREAD HAS TO CREATE ONE HANDLE (THEY CANNOT BE SHARED)
template<class GrowTableData>
class GrowTableHandle
{
private:
    // TYPEDEFS
    using Parent_t         = typename GrowTableData::Parent_t;
    using WorkerStrat_t    = typename GrowTableData::WorkerStrat_t;
    using ExclusionStrat_t = typename GrowTableData::ExclusionStrat_t;
    using HashPtrRef_t     = typename ExclusionStrat_t::HashPtrRef;
    using InternElement_t  = typename GrowTableData::SeqTable_t::InternElement_t;

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

private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    GrowTableData& gtData;
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
    void update_numbers();
    void inc_inserted();
    void inc_deleted();

    class LocalCount
    {
    public:
        alignas(64) int  updates;
        alignas(64) int  inserted;
        alignas(64) int  deleted;
        LocalCount() : updates(0), inserted(0), deleted(0) {}
        LocalCount(LocalCount&& rhs)
            : updates(rhs.updates), inserted(rhs.inserted), deleted(rhs.deleted)
        {
            rhs.updates = 0;
            rhs.inserted = 0;
            rhs.deleted = 0;
        }
        LocalCount& operator=(LocalCount&& rhs)
        {
            updates  = rhs.updates;
            inserted = rhs.inserted;
            deleted  = rhs.deleted;
            rhs.updates  = 0;
            rhs.inserted = 0;
            rhs.deleted  = 0;
            return *this;
        }
        LocalCount(const LocalCount&) = delete;
        LocalCount& operator=(const LocalCount&) = delete;
    };
    std::unique_ptr<LocalCount> counts;
};


template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(GrowTableData &data)
    : gtData(data), local_worker(data), local_exclusion(data, local_worker),
      max_fill_factor(0.666),
      counts(new LocalCount())
{
    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    local_exclusion.init();
    local_worker   .init(local_exclusion);
}

template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(Parent_t      &parent)
    : gtData(*(parent.gtData)), local_worker(*(parent.gtData)),
      local_exclusion(*(parent.gtData), local_worker),
      max_fill_factor(0.666),
      counts(new LocalCount())
{
    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    local_exclusion.init();
    local_worker   .init(local_exclusion);
}


template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(GrowTableHandle&& source) :
    gtData(source.gtData),
    local_worker(std::move(source.local_worker)),
    local_exclusion(std::move(source.local_exclusion)),
    max_fill_factor(source.max_fill_factor),
    counts(std::move(source.counts))
{

};

template<class GrowTableData>
GrowTableHandle<GrowTableData>& GrowTableHandle<GrowTableData>::operator=(GrowTableHandle&& source)
{
    gtData(source.gtData);
    local_worker(std::move(source.local_worker));
    local_exclusion(std::move(source.local_exclusion));
    max_fill_factor(source.max_fill_factor);
    counts(source.counts);
};



template<class GrowTableData>
GrowTableHandle<GrowTableData>::~GrowTableHandle()
{
    update_numbers();
    local_worker   .deinit();
    local_exclusion.deinit();
}


template<class GrowTableData>
inline ReturnCode GrowTableHandle<GrowTableData>::insert(const Key k, const Data d)
{   return insert(InternElement_t(k,d));  }

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
        inc_inserted();
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
        inc_inserted();
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
        inc_inserted();
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
        inc_deleted();
        return result;
    case ReturnCode::TSX_SUCCESS_DEL:
        inc_deleted();  // TSX DELETION COULD BE USED TO AVOID DUMMIES => dec_inserted()
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
    counts->updates = 0;

    gtData.dummies.fetch_add(counts->deleted,std::memory_order_relaxed);
    counts->deleted   = 0;

    auto temp         = gtData.elements.fetch_add(counts->inserted, std::memory_order_relaxed);
    temp             += counts->inserted;
    counts->inserted  = 0;

    if (temp  > getTable()->size*max_fill_factor)
    {
        rlsTable();
        grow();
    }
    rlsTable();
}

template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::inc_inserted()
{
    ++counts->inserted;
    if (++counts->updates > 64)
    {
        update_numbers();
    }
}

template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::inc_deleted()
{
    ++counts->deleted;
    if (++counts->updates > 64)
    {
        update_numbers();
    }
}

}

#endif // GROWTABLE_H
