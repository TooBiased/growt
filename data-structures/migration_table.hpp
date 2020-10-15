/*******************************************************************************
 * data-structures/grow_table.h
 *
 * Defines the growtable architecture:
 *   grow_table       - the global facade of our table
 *   grow_table_data   - the actual global object (immovable core)
 *   grow_table_handle - local handles on the global object (thread specific)
 * The behavior of grow_table can be specified using the Worker- and Exclusion-
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

#include "data-structures/returnelement.hpp"
#include "data-structures/grow_iterator.hpp"
#include "example/update_fcts.hpp"

namespace growt {

static const size_t migration_block_size = 4096;

template<class table_type>
size_t blockwise_migrate(table_type source, table_type target)
{
    size_t n = 0;

    //get block + while block legal migrate and get new block
    size_t temp = source->_current_copy_block.fetch_add(migration_block_size);
    while (temp < source->capacity())
    {
        n += source->migrate(*target, temp,
                             std::min(uint(temp+migration_block_size),
                                      uint(source->capacity())));
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
template<class> class migration_table_handle;
// AND THE STATIONARY DATA OBJECT (GLOBAL OBJECT ON HEAP)
template<class> class migration_table_data;

template<class                  HashTable,
         template <class> class WorkerStrat,
         template <class> class ExclusionStrat>
class migration_table
{
private:
    using this_type                 = migration_table<HashTable,
                                                      WorkerStrat,
                                                      ExclusionStrat>;
    using migration_table_data_type = migration_table_data<this_type>;
    using base_table_type           = HashTable;
    using worker_strat              = WorkerStrat<migration_table_data_type>;
    using exclusion_strat           = ExclusionStrat<migration_table_data_type>;
    friend migration_table_data_type;

    std::unique_ptr<migration_table_data_type> _mt_data;

public:
    using handle_type               = migration_table_handle<migration_table_data_type>;
    friend handle_type;

    migration_table (size_t size)
        : _mt_data(new migration_table_data_type(size)) { }

    migration_table (const migration_table& source)            = delete;
    migration_table& operator= (const migration_table& source) = delete;

    migration_table (migration_table&& source)            = default;
    migration_table& operator= (migration_table&& source) = default;

    ~migration_table() = default;

    handle_type get_handle() { return handle_type(*_mt_data); }
};





// GLOBAL TABLE OBJECT (THIS CANNOT BE USED WITHOUT CREATING A HANDLE)
template<typename Parent>
class migration_table_data
{
private:
    // TYPEDEFS
    using parent_type     = Parent;
    using base_table_type = typename Parent::base_table_type;
    using worker_strat    = typename Parent::worker_strat;
    using exclusion_strat = typename Parent::exclusion_strat;

    friend worker_strat;
    friend exclusion_strat;

public:
    using size_type       = size_t;
    using handle_type     = typename Parent::handle_type;
    friend handle_type;


    migration_table_data(size_type size_)
        : _global_exclusion(size_), _global_worker(), // handle_ptr(64),
          _elements(0), _dummies(0), _grow_count(0)
    { }

    migration_table_data(const migration_table_data& source) =delete;
    migration_table_data& operator=(const migration_table_data& source) =delete;
    migration_table_data(migration_table_data&&) =delete;
    migration_table_data& operator=(migration_table_data&&) =delete;
    ~migration_table_data() = default;

    size_type element_count_approx() { return _elements.load()-_dummies.load(); }

private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    mutable typename exclusion_strat::global_data_type _global_exclusion;
    mutable typename worker_strat::global_data_type    _global_worker;

    // APPROXIMATE COUNTS
    alignas(64) std::atomic_int _elements;
    alignas(64) std::atomic_int _dummies;
    alignas(64) std::atomic_int _grow_count;
};




// HANDLE OBJECTS EVERY THREAD HAS TO CREATE ONE HANDLE (THEY CANNOT BE SHARED)
template<class migration_table_data>
class migration_table_handle
{
private:
    using this_type          = migration_table_handle<migration_table_data>;
    using parent_type        = typename migration_table_data::parent_type;
    using base_table_type    = typename migration_table_data::base_table_type;
    using worker_strat       = typename migration_table_data::worker_strat;
    using exclusion_strat    = typename migration_table_data::exclusion_strat;
    friend migration_table_data;

public:
    using hash_ptr_reference       = typename exclusion_strat::hash_ptr_reference;
    using slot_config       = typename base_table_type::slot_config;

    using key_type           = typename base_table_type::key_type;
    using mapped_type        = typename base_table_type::mapped_type;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = growt_iterator<this_type, false>;//value_intern*;
    using const_iterator     = growt_iterator<this_type, true>;//growt_iterator<this_type, true>;
    using size_type          = size_t;
    using difference_type    = std::ptrdiff_t;
    using reference          = growt_reference<this_type, false>;
    using const_reference    = growt_reference<this_type, true>;
    using mapped_reference   = growt_mapped_reference<this_type, false>;
    using const_mapped_reference = growt_mapped_reference<this_type, true>;
    using insert_return_type = std::pair<iterator, bool>;

    using local_iterator       = void;
    using const_local_iterator = void;
    using node_type            = void;

private:
    using base_table_iterator   = typename base_table_type::iterator;
    using base_table_insert_return_type = typename base_table_type::insert_return_intern;
    using base_table_citerator  = typename base_table_type::const_iterator;

    friend iterator;
    friend reference;
    friend const_iterator;
    friend const_reference;
    friend mapped_reference;
    friend const_mapped_reference;

public:
    migration_table_handle() = delete;
    migration_table_handle(migration_table_data &data);
    migration_table_handle(parent_type      &parent);

    migration_table_handle(const migration_table_handle& source) = delete;
    migration_table_handle& operator=(const migration_table_handle& source) = delete;

    migration_table_handle(migration_table_handle&& source) noexcept;
    migration_table_handle& operator=(migration_table_handle&& source) noexcept;

    ~migration_table_handle();

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
    { return insert_or_update(k, d, example::Overwrite(), d); }

    mapped_reference operator[](const key_type& k)
        { return (*(insert(k, mapped_type()).first)).second; }

    template <class F, class ... Types>
    insert_return_type update
    (const key_type& k, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_type update_unsafe
    (const key_type& k, F f, Types&& ... args);


    template <class F, class ... Types>
    insert_return_type insert_or_update
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_type insert_or_update_unsafe
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);

    size_type          erase_if (const key_type& k, const mapped_type& d);

    size_type element_count_approx() { return _mt_data.element_count_approx(); }
private:
    // DATA+FUNCTIONS FOR MIGRATION STRATEGIES
    migration_table_data& _mt_data;
    size_type             _handle_id;
    mutable typename worker_strat::local_data_type    _local_worker;
    mutable typename exclusion_strat::local_data_type _local_exclusion;


    inline void         grow()      const { _local_exclusion.grow(); }
    inline void         help_grow() const { _local_exclusion.help_grow(); }
    inline void         rls_table() const { _local_exclusion.rls_table(); }
    inline hash_ptr_reference get_table() const { return _local_exclusion.get_table(); }

    template<typename Functor, typename ... Types>
    inline typename std::result_of<Functor(hash_ptr_reference, Types&& ...)>::type
    execute (Functor f, Types&& ... param)
    {
        hash_ptr_reference temp = _local_exclusion.get_table();
        auto result = std::forward<Functor>(f)
                          (temp, std::forward<Types>(param)...);
        rls_table();
        return result;
    }

    template<typename Functor, typename ... Types>
    inline typename std::result_of<Functor(hash_ptr_reference, Types&& ...)>::type
    cexecute (Functor f, Types&& ... param) const
    {
        hash_ptr_reference temp = _local_exclusion.get_table();
        auto result = std::forward<Functor>(f)
                          (temp, std::forward<Types>(param)...);
        rls_table();
        return result;
    }

    inline iterator make_iterator(const base_table_iterator& bit, size_t version)
    { return iterator(bit, version, *this); }
    inline iterator make_citerator(const base_table_citerator& bcit, size_t version)
    { return const_iterator(bcit, version, *this); }

    inline insert_return_type make_insert_ret(const base_table_iterator& bit,
                                            size_t version, bool inserted)
    { return std::make_pair(iterator(bit, version, *this), inserted); }
    inline base_table_iterator bend()
    { return base_table_iterator (std::make_pair(key_type(), mapped_type()), nullptr, nullptr);}
    inline base_table_iterator bcend()
    { return base_table_citerator(std::make_pair(key_type(), mapped_type()), nullptr, nullptr);}

    static constexpr double _max_fill_factor = 0.666;

    // LOCAL COUNTERS FOR SIZE ESTIMATION WITH SOME PADDING FOR
    // REDUCING CACHE EFFECTS
public:
    void update_numbers();

private:
    void inc_inserted(int v);
    void inc_deleted(int v);

    class alignas(64) local_count
    {
    public:
        int  _version;
        int  _updates;
        int  _inserted;
        int  _deleted;
        local_count() : _version(-1), _updates(0), _inserted(0), _deleted(0)
        {  }

        local_count(local_count&& rhs)
            : _version(rhs._version), _updates(rhs._updates),
              _inserted(rhs._inserted), _deleted(rhs._deleted)
        {
            rhs._version = 0;
        }

        local_count& operator=(local_count&& rhs)
        {
            _version   = rhs._version;
            rhs._version  = 0;
            _updates  = rhs._updates;
            _inserted = rhs._inserted;
            _deleted  = rhs._deleted;
            return *this;
        }

        void set(int ver, int upd, int in, int del)
        {
            _updates  = upd;
            _inserted = in;
            _deleted  = del;
            _version  = ver;
        }

        local_count(const local_count&) = delete;
        local_count& operator=(const local_count&) = delete;
    };
    local_count _counts;

public:
    using range_iterator       = typename base_table_type::range_iterator;
    using const_range_iterator = typename base_table_type::const_range_iterator;

    /* size has to divide capacity */
    range_iterator       range (size_t rstart, size_t rend)
    {
        range_iterator result = execute([rstart, rend](hash_ptr_reference tab)
                                        { return tab->range(rstart,rend); });
        return result;
    }
    const_range_iterator crange(size_t rstart, size_t rend)
    {
        const_range_iterator result = cexecute([rstart, rend](hash_ptr_reference tab)
                                        { return tab->crange(rstart,rend); });
        return result;
    }
    range_iterator       range_end ()       { return  bend(); }
    const_range_iterator range_cend() const { return bcend(); }
    size_t               capacity()   const
    {
        size_t cap = cexecute([](hash_ptr_reference tab) { return tab->capacity(); });
        return cap;
    }

};







// CONSTRUCTORS AND ASSIGNMENTS ************************************************

template<class migration_table_data>
migration_table_handle<migration_table_data>::migration_table_handle(migration_table_data &data)
    : _mt_data(data), _local_worker(data), _local_exclusion(data, _local_worker),
      _counts()
{
    //handle_id = _mt_data.handle_ptr.push_back(this);

    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    _local_exclusion.init();
    _local_worker   .init(_local_exclusion);
}

template<class migration_table_data>
migration_table_handle<migration_table_data>::migration_table_handle(parent_type      &parent)
    : _mt_data(*(parent._mt_data)), _local_worker(*(parent._mt_data)),
      _local_exclusion(*(parent._mt_data), _local_worker),
      _counts()
{
    //handle_id = _mt_data.handle_ptr.push_back(this);

    //INITIALIZE STRATEGY DEPENDENT DATA MEMBERS
    _local_exclusion.init();
    _local_worker   .init(_local_exclusion);
}



template<class migration_table_data>
migration_table_handle<migration_table_data>::migration_table_handle(migration_table_handle&& source) noexcept
    : _mt_data(source._mt_data), _handle_id(source._handle_id),
      _local_worker(std::move(source._local_worker)),
      _local_exclusion(std::move(source._local_exclusion)),
      _counts(std::move(source._counts))
{
    source._counts = local_count();
    //_mt_data.handle_ptr.update(_handle_id, this);
    //source._handle_id = std::numeric_limits<size_t>::max();
}

template<class migration_table_data>
migration_table_handle<migration_table_data>&
migration_table_handle<migration_table_data>::operator=(migration_table_handle&& source) noexcept
{
    if (this == &source) return *this;

    this->~migration_table_handle();
    new (this) migration_table_handle(std::move(source));
    return *this;
}



template<class migration_table_data>
migration_table_handle<migration_table_data>::~migration_table_handle()
{

    // if (_handle_id < std::numeric_limits<size_t>::max())
    // {
    //     _mt_data.handle_ptr.remove(_handle_id);
    // }
    if (_counts._version >= 0)
    {
        update_numbers();
    }

    _local_worker   .deinit();
    _local_exclusion.deinit();
}









// MAIN HASH TABLE FUNCTIONALITY ***********************************************

template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::insert_return_type
migration_table_handle<migration_table_data>::insert(const key_type& k, const mapped_type& d)
{
    int v = -1;
    base_table_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);
    std::tie (v, result) = execute([](hash_ptr_reference t, const key_type& k, const mapped_type& d)
                                     ->std::pair<int,base_table_insert_return_type>
                                   {
                                       std::pair<int,base_table_insert_return_type> result =
                                           std::make_pair(t->_version,
                                                          t->insert_intern(k,d));
                                       return result;
                                   }, k,d);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
        return make_insert_ret(result.first, v, true);
    case ReturnCode::UNSUCCESS_ALREADY_USED:
    case ReturnCode::TSX_UNSUCCESS_ALREADY_USED:
        return make_insert_ret(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insert(k,d);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return insert(k,d);
    default:
        return make_insert_ret(bend(), v, false);
    }
}

template<class migration_table_data> template <class F, class ... Types>
inline typename migration_table_handle<migration_table_data>::insert_return_type
migration_table_handle<migration_table_data>::update(const key_type& k, F f, Types&& ... args)
{
    int v = -1;
    base_table_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](hash_ptr_reference t, const key_type& k, F f, Types&& ... args)
        ->std::pair<int,base_table_insert_return_type>
        {
            std::pair<int,base_table_insert_return_type> result =
                std::make_pair(t->_version,
                               t->update_intern(k,f,std::forward<Types>(args)...));
            return result;
        },k,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return make_insert_ret(result.first, v, true);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        //std::cout << "!" << std::flush;
        return make_insert_ret(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();  // usually impossible as this collides with NOT_FOUND
        return update(k,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return update(k,f, std::forward<Types>(args)...);
    default:
        return make_insert_ret(bend(), v, false);
    }
}

template<class migration_table_data> template <class F, class ... Types>
inline typename migration_table_handle<migration_table_data>::insert_return_type
migration_table_handle<migration_table_data>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    int v = -1;
    base_table_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](hash_ptr_reference t, const key_type& k, F f, Types&& ... args)
        ->std::pair<int,base_table_insert_return_type>
        {
            std::pair<int,base_table_insert_return_type> result =
                std::make_pair(t->_version,
                               t->update_unsafe_intern(k,f,std::forward<Types>(args)...));
            return result;
        },k,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return make_insert_ret(result.first, v, true);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return make_insert_ret(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return update_unsafe(k,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return update_unsafe(k,f, std::forward<Types>(args)...);
    default:
        return make_insert_ret(bend(), v, false);
    }
}


template<class migration_table_data> template <class F, class ... Types>
inline typename migration_table_handle<migration_table_data>::insert_return_type
migration_table_handle<migration_table_data>::insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    int v = -1;
    base_table_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](hash_ptr_reference t, const key_type& k, const mapped_type& d, F f, Types&& ... args)
        ->std::pair<int,base_table_insert_return_type>
        {
            std::pair<int,base_table_insert_return_type> result =
                std::make_pair(t->_version,
                               t->insert_or_update_intern(k,d,f,std::forward<Types>(args)...));
            return result;
        },k,d,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
        return make_insert_ret(result.first, v, true);
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return make_insert_ret(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insert_or_update(k,d,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return insert_or_update(k,d,f, std::forward<Types>(args)...);
    default:
        return make_insert_ret(bend(), v, false);
    }
}

template<class migration_table_data> template <class F, class ... Types>
inline typename migration_table_handle<migration_table_data>::insert_return_type
migration_table_handle<migration_table_data>::insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    int v = -1;
    base_table_insert_return_type result = std::make_pair(bend(), ReturnCode::ERROR);

    std::tie (v, result) = execute(
        [](hash_ptr_reference t, const key_type& k, const mapped_type& d, F f, Types&& ... args)
        ->std::pair<int,base_table_insert_return_type>
        {
            std::pair<int,base_table_insert_return_type> result =
                std::make_pair(t->_version,
                               t->insert_or_update_unsafe_intern(k,d,f,std::forward<Types>(args)...));
            return result;
        },k,d,f,std::forward<Types>(args)...);

    switch(result.second)
    {
    case ReturnCode::SUCCESS_IN:
    case ReturnCode::TSX_SUCCESS_IN:
        inc_inserted(v);
        return make_insert_ret(result.first, v, true);
    case ReturnCode::SUCCESS_UP:
    case ReturnCode::TSX_SUCCESS_UP:
        return make_insert_ret(result.first, v, false);
    case ReturnCode::UNSUCCESS_FULL:
    case ReturnCode::TSX_UNSUCCESS_FULL:
        grow();
        return insert_or_update_unsafe(k,d,f, std::forward<Types>(args)...);
    case ReturnCode::UNSUCCESS_INVALID:
    case ReturnCode::TSX_UNSUCCESS_INVALID:
        help_grow();
        return insert_or_update_unsafe(k,d,f, std::forward<Types>(args)...);
    default:
        return make_insert_ret(bend(), v, false);
    }
}

template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::iterator
migration_table_handle<migration_table_data>::find(const key_type& k)
{
    int v = -1;
    base_table_iterator bit = bend();
    std::tie (v, bit) = execute([](hash_ptr_reference t, const key_type & k) -> std::pair<int, base_table_iterator>
                                { return std::make_pair<int, base_table_iterator>(t->_version, t->find(k)); },
                     k);
    return make_iterator(bit, v);
}

template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::const_iterator
migration_table_handle<migration_table_data>::find(const key_type& k) const
{
    int v = -1;
    base_table_citerator bit = bcend();
    std::tie (v, bit) = cexecute([](hash_ptr_reference t, const key_type & k) -> std::pair<int, base_table_citerator>
                                { return std::make_pair<int, base_table_iterator>(t->_version, t->find(k)); },
                     k);
    return make_citerator(bit, v);
}

template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::size_type
migration_table_handle<migration_table_data>::erase(const key_type& k)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;
    std::tie (v, result) = execute([](hash_ptr_reference t, const key_type& k)
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
        help_grow();
        return erase(k);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return 0;
    default:
        return 0;
    }
}

template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::size_type
migration_table_handle<migration_table_data>::erase_if(const key_type& k, const mapped_type& d)
{
    int v = -1;
    ReturnCode result = ReturnCode::ERROR;
    std::tie (v, result) = execute([](hash_ptr_reference t,
                                      const key_type& k,
                                      const mapped_type& d)
                                     ->std::pair<int,ReturnCode>
                                   {
                                       std::pair<int,ReturnCode> result =
                                           std::make_pair(t->_version,
                                                          t->erase_if_intern(k,d));
                                       return result;
                                   },k,d);

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
        help_grow();
        return erase_if(k,d);
    case ReturnCode::UNSUCCESS_NOT_FOUND:
    case ReturnCode::TSX_UNSUCCESS_NOT_FOUND:
        return 0;
    default:
        return 0;
    }
}






// ITERATOR FUNCTIONALITY ******************************************************

template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::iterator
migration_table_handle<migration_table_data>::begin()
{
    return execute([](hash_ptr_reference t, migration_table_handle& gt)
                   -> iterator
                   {
                       return iterator(t->begin(), t->_version, gt);
                   }, *this);
}
template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::iterator
migration_table_handle<migration_table_data>::end()
{
    return iterator(base_table_iterator(std::pair<key_type, mapped_type>(key_type(), mapped_type()),
                                       nullptr,nullptr),
                    0, *this);
}

template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::const_iterator
migration_table_handle<migration_table_data>::cbegin() const
{
    // return begin();
    return cexecute([](hash_ptr_reference t, const migration_table_handle& gt)
                    -> const_iterator
                    {
                      return const_iterator(t->cbegin(), t->_version, gt);
                    }, *this);
}
template<class migration_table_data>
inline typename migration_table_handle<migration_table_data>::const_iterator
migration_table_handle<migration_table_data>::cend()  const
{
    // return end();
    return const_iterator(base_table_citerator(std::pair<key_type, mapped_type>(key_type(), mapped_type()),
                                       nullptr,nullptr), 0, *this);
}





// COUNTING FUNCTIONALITY ******************************************************

template<class migration_table_data>
inline void migration_table_handle<migration_table_data>::update_numbers()
{
    _counts._updates  = 0;

    auto table = get_table();
    if (table->_version != size_t(_counts._version))
    {
        _counts.set(table->_version, 0,0,0);
        rls_table();
        return;
    }

    _mt_data._dummies.fetch_add(_counts._deleted,std::memory_order_relaxed);

    auto temp       = _mt_data._elements.fetch_add(_counts._inserted, std::memory_order_relaxed);
    temp           += _counts._inserted;

    if (temp  > table->_capacity * _max_fill_factor)
    {
        rls_table();
        grow();
    }
    rls_table();
    _counts.set(_counts._version, 0,0,0);
}

template<class migration_table_data>
inline void migration_table_handle<migration_table_data>::inc_inserted(int v)
{
    if (_counts._version == v)
    {
        ++_counts._inserted;
        if (++_counts._updates > 64)
        {
            update_numbers();
        }
    }
    else
    {
        _counts.set(v,1,1,0);
    }
}


template<class migration_table_data>
inline void migration_table_handle<migration_table_data>::inc_deleted(int v)
{
    if (_counts._version == v)
    {
        ++_counts._deleted;
        if (++_counts._updates > 64)
        {
            update_numbers();
        }
    }
    else
    {
        _counts.set(v,1,0,1);
    }
}

// template <typename migration_table_data>
// inline typename migration_table_handle<migration_table_data>::size_type
// migration_table_handle<migration_table_data>::element_count_unsafe()
// {
//     int v = get_table()->_version;
//     rls_table();

//     int temp = _mt_data._elements.load();
//     temp    -= _mt_data._dummies.load();
//     temp    += _mt_data.handle_ptr.forall([v](this_type* h, int res)
//                                         {
//                                             if (h->_counts._version != v)
//                                             {
//                                                 return res;
//                                             }
//                                             int temp = res;
//                                             temp += h->_counts._inserted;
//                                             temp -= h->_counts._deleted;
//                                             return temp;
//                                         });
//     return temp;
// }

}
