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
#include "data-structures/grow_iterator.h"
#include "example/update_fcts.h"

#include <atomic>
#include <memory>

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

    std::unique_ptr<GTD_t> _gt_data;

public:
    using Element          = ReturnElement;
    using Handle           = GrowTableHandle<GTD_t>;
    friend Handle;

    GrowTable (size_t size) : _gt_data(std::make_unique<GTD_t>(size)) { }

    GrowTable (const GrowTable& source)            = delete;
    GrowTable& operator= (const GrowTable& source) = delete;

    GrowTable (GrowTable&& source)            = default;
    GrowTable& operator= (GrowTable&& source) = default;

    ~GrowTable() = default;

    Handle get_handle()
    {
        return Handle(*_gt_data);
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
    using Handle           = typename Parent::Handle;
    using Element          = ReturnElement;
    using Key              = typename Element::Key;
    using Data             = typename Element::Data;

    friend Handle;

    GrowTableData(size_t size_)
        : _global_exclusion(size_), _global_worker(),
          _elements(0), _dummies(0)
    { }

    GrowTableData(const GrowTableData& source) = delete;
    GrowTableData& operator=(const GrowTableData& source) = delete;
    ~GrowTableData() = default;

private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    typename ExclusionStrat_t::global_data_t _global_exclusion;
    typename WorkerStrat_t   ::global_data_t _global_worker;

    // APPROXIMATE COUNTS
    alignas(64) std::atomic_int _elements;
    alignas(64) std::atomic_int _dummies;
};




// HANDLE OBJECTS EVERY THREAD HAS TO CREATE ONE HANDLE (THEY CANNOT BE SHARED)
template<class GrowTableData>
class GrowTableHandle
{
private:
    // TYPEDEFS
    using This_t                 = GrowTableHandle<GrowTableData>;
    using Parent_t               = typename GrowTableData::Parent_t;
    using BaseTable_t            = typename GrowTableData::BaseTable_t;
    using WorkerStrat_t          = typename GrowTableData::WorkerStrat_t;
    using ExclusionStrat_t       = typename GrowTableData::ExclusionStrat_t;
    using HashPtrRef_t           = typename ExclusionStrat_t::HashPtrRef;
    using InternElement_t        = typename BaseTable_t::value_intern;

public:
    using key_type               = typename BaseTable_t::key_type;
    using mapped_type            = typename BaseTable_t::mapped_type;
    using value_type             = typename std::pair<const key_type, mapped_type>;
    using iterator               = IteratorGrowT<This_t, false>;
    using const_iterator         = IteratorGrowT<This_t, true>;
    using size_type              = size_t;
    using difference_type        = std::ptrdiff_t;
    using reference              = ReferenceGrowT<This_t, false>;
    using const_reference        = ReferenceGrowT<This_t, true>;
    using mapped_reference       = MappedRefGrowT<This_t, false>;
    using const_mapped_reference = MappedRefGrowT<This_t, true>;
    using insert_return_type     = std::pair<iterator, bool>;

    using local_iterator         = void;
    using const_local_iterator   = void;
    using node_type              = void;

    using value_intern           = InternElement_t;
private:
    friend iterator;
    friend const_iterator;
    friend reference;
    friend const_reference;
    friend mapped_reference;
    friend const_mapped_reference;

    using base_iterator           = typename BaseTable_t::iterator;
    using base_citerator          = typename BaseTable_t::const_iterator;
    using base_insert_return_type = typename BaseTable_t::insert_return_type;
    using base_intern_insert_return_type = typename BaseTable_t::insert_return_intern;

    inline constexpr base_iterator  bend()
    { return base_iterator (std::make_pair(key_type(), mapped_type()), nullptr, nullptr); }
    inline constexpr base_citerator bcend()
    { return base_citerator(std::make_pair(key_type(), mapped_type()), nullptr, nullptr); }

public:
    GrowTableHandle() = delete;
    GrowTableHandle(GrowTableData &data);
    GrowTableHandle(Parent_t      &parent);

    GrowTableHandle(const GrowTableHandle& source) = delete;
    GrowTableHandle& operator=(const GrowTableHandle& source) = delete;

    GrowTableHandle(GrowTableHandle&& source);
    GrowTableHandle& operator=(GrowTableHandle&& source);

    ~GrowTableHandle();

    iterator       begin();
    const_iterator begin()  const { return cbegin(); }
    const_iterator cbegin() const;

    inline constexpr iterator       end()
    { return iterator(bend(), 0, *this); }
    inline constexpr const_iterator cend() const
    { return const_iterator(bcend(), 0, *this); }
    inline constexpr const_iterator end() const
    { return const_iterator(bcend(), 0, *this); }

    insert_return_type insert(const key_type& k, const mapped_type& d);
    iterator           find (const key_type& k);
    const_iterator     find (const key_type& k) const;

    mapped_reference operator[](const key_type& k)
    { return (*insert(k, mapped_type())).second; }
    insert_return_type insert_or_assign(const key_type& k, const mapped_type& d)
    { return insert_or_Update(k, d, example::Overwrite(), d); }

    size_type          erase(const key_type& k);

    template <class F, class ... Types>
    insert_return_type update        (const key_type& k, F f, Types&& ... args);
    template <class F, class ... Types>
    insert_return_type insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args);
    template <class F, class ... Types>
    insert_return_type update_unsafe (const key_type& k, F f, Types&& ...args);
    template <class F, class ... Types>
    insert_return_type insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args);

private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    GrowTableData& _gt_data;
    mutable typename WorkerStrat_t   ::local_data_t _local_worker;
    mutable typename ExclusionStrat_t::local_data_t _local_exclusion;


    inline void         grow()      { _local_exclusion.grow(); }
    inline void         help_grow() { _local_exclusion.help_grow(); }
    inline void         rls_table() { _local_exclusion.rls_table(); }
    inline HashPtrRef_t get_table() { return _local_exclusion.get_table(); }

    template<typename Functor, typename ... Types>
    typename std::result_of<Functor(HashPtrRef_t, Types&& ...)>::type
    execute (Functor f, Types&& ... param)
    {
        HashPtrRef_t temp = _local_exclusion.get_table();
        auto result = std::forward<Functor>(f)
                          (temp, std::forward<Types>(param)...);
        rls_table();
        return result;
    }

    template<typename Functor, typename ... Types>
    inline typename std::result_of<Functor(HashPtrRef_t, Types&& ...)>::type
    cexecute (Functor f, Types&& ... param) const
    {
        HashPtrRef_t temp = _local_exclusion.get_table();
        auto result = std::forward<Functor>(f)
                          (temp, std::forward<Types>(param)...);
        rls_table();
        return result;
    }

    static constexpr double _max_fill_factor = 0.666;

    // LOCAL COUNTERS FOR SIZE ESTIMATION WITH SOME PADDING FOR
    // REDUCING CACHE EFFECTS
    void update_numbers();
    void inc_inserted();
    void inc_deleted();

    alignas(64) int  _updates;
    alignas(64) int  _inserted;
    alignas(64) int  _deleted;
};


template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(GrowTableData &data)
    : _gt_data(data), _local_worker(data), _local_exclusion(data, _local_worker),
      _updates(0),
      _inserted(0), _deleted(0)
{
    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    _local_exclusion.init();
    _local_worker   .init(_local_exclusion);
}

template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(Parent_t      &parent)
    : _gt_data(*(parent._gt_data)), _local_worker(*(parent._gt_data)),
      _local_exclusion(*(parent._gt_data), _local_worker),
      _updates(0),
      _inserted(0), _deleted(0)
{
    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    _local_exclusion.init();
    _local_worker   .init(_local_exclusion);
}


template<class GrowTableData>
GrowTableHandle<GrowTableData>::GrowTableHandle(GrowTableHandle&& source) :
    _gt_data(source._gt_data),
    _local_worker   (std::move(source._local_worker)),
    _local_exclusion(std::move(source._local_exclusion)),
    _updates        (source._updates),
    _inserted       (source._inserted),
    _deleted        (source._deleted)
{

};

template<class GrowTableData>
GrowTableHandle<GrowTableData>& GrowTableHandle<GrowTableData>::operator=(GrowTableHandle&& source)
{
    _gt_data(source._gt_data);
    _local_worker   (std::move(source._local_worker));
    _local_exclusion(std::move(source._local_exclusion));
    _updates         = source._updates;
    _inserted        = source._inserted;
    _deleted         = source._deleted;
    source._inserted = 0;
    source._deleted  = 0;
    return *this;
};



template<class GrowTableData>
GrowTableHandle<GrowTableData>::~GrowTableHandle()
{
    update_numbers();
    _local_worker   .deinit();
    _local_exclusion.deinit();
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
inline typename GrowTableHandle<GrowTableData>::const_iterator
GrowTableHandle<GrowTableData>::cbegin() const
{
    return cexecute([](HashPtrRef_t t, const GrowTableHandle& gt)
                    -> const_iterator
                    {
                      return const_iterator(t->cbegin(), t->_version, gt);
                    }, *this);
}




// HASH TABLE FUNCTIONALITY ****************************************************

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::insert(const key_type& k, const mapped_type& d)
{
    int v = -1;
    base_intern_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);
    std::tie (v, result) = execute([](HashPtrRef_t t, const key_type& k, const mapped_type& d)
                                     ->std::pair<int,base_intern_insert_return_type>
                                   {
                                       std::pair<int,base_intern_insert_return_type> result =
                                           std::make_pair(t->_version,
                                                          t->insert_intern(k,d));
                                       return result;
                                   }, k,d);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted();
        return insert_return_type(iterator(result.first,v,*this), true);
    case ReturnCode::UNSUCCESS_ALREADY_USED:
    case ReturnCode::TSX_UNSUCCESS_ALREADY_USED:
        return insert_return_type(iterator(result.first,v,*this), false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insert(k,d);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return insert(k,d);
    default:
        return insert_return_type(end(), false);
    }
}

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::iterator
GrowTableHandle<GrowTableData>::find(const key_type& k)
{
    base_iterator it = bend();
    size_t        v  = 0;
    std::tie(v, it)  =
        execute([this](HashPtrRef_t t, const key_type& k)
                -> std::pair<size_t, base_iterator>
                { return std::make_pair(t->_version, t->find(k)); },
                      k);
    return iterator(it, v, *this);
}

template<class GrowTableData>
inline typename GrowTableHandle<GrowTableData>::const_iterator
GrowTableHandle<GrowTableData>::find(const key_type& k) const
{
    base_citerator it = bcend();
    size_t         v  = 0;
    std::tie(v, it)   =
        execute([this](HashPtrRef_t t, const key_type& k)
                -> std::pair<size_t, base_citerator>
                { return std::make_pair(t->_version, t->find(k)); },
                      k);
    return const_iterator(it, v, *this);
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
        inc_deleted();
        return 1;
    case ReturnCode::TSX_SUCCESS_DEL:
        inc_deleted(); // TSX DELETION COULD BE USED TO AVOID DUMMIES => dec_inserted()
        return 1;
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return erase(k);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return 0;
    default:
        return 0;
    }
}

template<class GrowTableData> template <class F, class ... Types>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::update(const key_type& k, F f, Types&& ... args)
{
    int v = -1;
    base_intern_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](HashPtrRef_t t, const key_type& k, F f, Types&& ... args)
        ->std::pair<int,base_intern_insert_return_type>
        {
            std::pair<int,base_intern_insert_return_type> result =
                std::make_pair(t->_version,
                               t->update_intern(k,f,std::forward<Types>(args)...));
            return result;
        },k,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return std::make_pair(iterator(result.first,v,*this), true);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return std::make_pair(iterator(result.first,v,*this), false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();  // usually impossible as this collides with NOT_FOUND
        return update(k,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return update(k,f, std::forward<Types>(args)...);
    default:
        return std::make_pair(end(), false);
    }
}


template<class GrowTableData> template <class F, class ... Types>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    int v = -1;
    base_intern_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](HashPtrRef_t t, const key_type& k, const mapped_type& d, F f, Types&& ... args)
        ->std::pair<int,base_intern_insert_return_type>
        {
            std::pair<int,base_intern_insert_return_type> result =
                std::make_pair(t->_version,
                               t->insert_or_update_intern(k,d,f,std::forward<Types>(args)...));
            return result;
        },k,d,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted();
        return std::make_pair(iterator(result.first,v,*this), true);
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return std::make_pair(iterator(result.first,v,*this), false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insert_or_update(k,d,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return insert_or_update(k,d,f, std::forward<Types>(args)...);
    default:
        return std::make_pair(end(), false);
    }
}

template<class GrowTableData> template <class F, class ... Types>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    int v = -1;
    base_intern_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](HashPtrRef_t t, const key_type& k, F f, Types&& ... args)
        ->std::pair<int,base_intern_insert_return_type>
        {
            std::pair<int,base_intern_insert_return_type> result =
                std::make_pair(t->_version,
                               t->update_unsafe_intern(k,f,std::forward<Types>(args)...));
            return result;
        },k,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return std::make_pair(iterator(result.first,v,*this), true);
               // makeInsertRet(result.first, v, true);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        //std::cout << "!" << std::flush;
        return std::make_pair(iterator(result.first,v,*this), false);
               // makeInsertRet(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();  // usually impossible as this collides with NOT_FOUND
        return update_unsafe(k,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return update_unsafe(k,f, std::forward<Types>(args)...);
    default:
        return std::make_pair(end(), false);
    }
}


template<class GrowTableData> template <class F, class ... Types>
inline typename GrowTableHandle<GrowTableData>::insert_return_type
GrowTableHandle<GrowTableData>::insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    int v = -1;
    base_intern_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](HashPtrRef_t t, const key_type& k, const mapped_type& d, F f, Types&& ... args)
        ->std::pair<int,base_intern_insert_return_type>
        {
            std::pair<int,base_intern_insert_return_type> result =
                std::make_pair(t->_version,
                               t->insert_or_update_unsafe_intern(k,d,f,std::forward<Types>(args)...));
            return result;
        },k,d,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted();
        return std::make_pair(iterator(result.first,v,*this), true);
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return std::make_pair(iterator(result.first,v,*this), false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insert_or_update_unsafe(k,d,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return insert_or_update_unsafe(k,d,f, std::forward<Types>(args)...);
    default:
        return std::make_pair(end(), false);
    }
}



// ELEMENT COUNTING STUFF ******************************************************

template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::update_numbers()
{
    _updates = 0;

    _gt_data._dummies.fetch_add(_deleted,std::memory_order_relaxed);
    _deleted   = 0;

    auto temp = _gt_data._elements.fetch_add(_inserted, std::memory_order_relaxed);
    // temp     += _inserted;
    // _inserted  = 0;

    int cap = get_table()->_capacity * _max_fill_factor;
    rls_table();

    if (temp + _inserted > cap)
    {
        //rls_table();
        grow();
    }
    _inserted = 0;
    //rls_table();
}

template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::inc_inserted()
{
    ++_inserted;
    if (++_updates > 64)
    {
        update_numbers();
    }
}

template<class GrowTableData>
inline void GrowTableHandle<GrowTableData>::inc_deleted()
{
    ++_deleted;
    if (++_updates > 64)
    {
        update_numbers();
    }
}

}

#endif // GROWTABLE_H
