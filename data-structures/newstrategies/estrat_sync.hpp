/*******************************************************************************
 * data-structures/strategy/estrat_sync.h
 *
 * see below
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/
#pragma once

#include <atomic>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <string>

/*******************************************************************************
 *
 * This is a exclusion strategy for our growtable.
 *
 * Every exclusion strategy has to implement the following
 *  - subclass: global_data_t      (is stored at the growtable object)
 *     - THIS OBJECT STORES THE ACTUAL HASH TABLE (AT LEAST THE POINTER)
 *           (This is important, because the exclusion strategy dictates,
 *            if reference counting is necessary.)
 *  - subclass: local_data_t       (is stored at each handle)
 *     - init()
 *     - deinit()
 *     - get_table()   (gets current table and protects it from destruction)
 *     - rls_table()   (stops protecting the table)
 *     - grow()       (creates a new table and initiates a growing step)
 *     - help_grow()   (called when an operation is unsuccessful,
 *                     because the table is growing)
 *     - migrate()    (called by the worker strategy to execute the migration.
 *                     Done here to ensure the table is not concurrently freed.)
 *
 * This specific strategy uses a synchronized growing approach, where table
 * updates and growing steps cannot coexist to do this some flags are used
 * (they are stored in the blobal data object). They will be set during each
 * operation on the table. Since the growing is synchronized, storing
 * the table is easy (using some atomic pointers).
 *
 ******************************************************************************/

namespace growt {

template<class table_type> size_t blockwise_migrate(table_type source, table_type target);


template <class Parent>
class estrat_sync
{
public:
    static constexpr size_t max_sim_threads = 256;

    using base_table_type   = typename Parent::base_table_type;
    using worker_strat_local_data  = typename Parent::worker_strat::local_data_type;
    using hash_ptr       = std::atomic<base_table_type*>;
    using hash_ptr_reference    = base_table_type*;

    static constexpr size_t migration_block_size = 4096;


    class local_data_type;

    // STORED AT THE GLOBAL OBJECT
    //  - ATOMIC POINTERS TO BOTH CURRENT AND TARGET TABLE
    //  - FLAGS FOR ALL HANDLES (currently in critical section?/growing step?)
    class global_data_type
    {
    public:

        global_data_type(size_t size_);
        global_data_type(const global_data_type& source) = delete;
        global_data_type& operator=(const global_data_type& source) = delete;
        ~global_data_type();
    private:
        friend local_data_type;

        // prevents false sharing between handlespecific flags
        // essential for good performance
        struct alignas(128) HandleFlags
        {
            std::atomic_size_t in_use;
            std::atomic_size_t table_op;
            std::atomic_size_t migrating;

            constexpr HandleFlags() : in_use(0), table_op(0), migrating(0) { }
        };


        // align to cache line
        std::atomic_size_t _currently_growing;
        std::atomic_size_t _handle_id;
        hash_ptr            _g_table_r;
        hash_ptr            _g_table_w;
        alignas(128) HandleFlags _handle_flags[max_sim_threads];

        size_t register_handle();
    };

    // STORED AT EACH HANDLE
    //  - REFERENCE TO THE OWNED FLAGS
    class local_data_type
    {
    public:

        local_data_type(Parent& parent, worker_strat_local_data &wstrat);
        local_data_type(const local_data_type& source) = delete;
        local_data_type& operator=(const local_data_type& source) = delete;

        local_data_type(local_data_type&& source);
        local_data_type& operator=(local_data_type&& source);
        ~local_data_type();

        inline void init() { }
        inline void deinit() { }

    private:
        Parent&                   _parent;
        global_data_type&         _global;
        worker_strat_local_data&  _worker_strat;

        size_t         _id;
        size_t         _epoch;
        typename global_data_type::HandleFlags& _flags;

    public:
        inline hash_ptr_reference get_table();
        inline void rls_table();
        void grow();
        inline void help_grow();
        inline size_t migrate();

    private:
        size_t blockwise_migrate(base_table_type& source,
                                 base_table_type& target);
        template<bool ErrorMsg = true>
        inline bool change_stage(size_t& stage, size_t next);
        inline void wait_for_table_op();
        inline void wait_for_migration();
    };

    static std::string name()
    {
        return "e_sync";
    }
};





template <class P>
estrat_sync<P>::global_data_type::global_data_type(size_t size_)
    : _currently_growing(0), _handle_id(0)
{
    auto temp = new base_table_type(size_);
    _g_table_r.store( temp, std::memory_order_relaxed );
    _g_table_w.store( temp, std::memory_order_relaxed );
}

template <class P>
estrat_sync<P>::global_data_type::~global_data_type()
{
    delete _g_table_w.load();
}

template <class P>
size_t
estrat_sync<P>::global_data_type::register_handle()
{
    for (size_t i = 0; i < max_sim_threads; ++i)
    {
        if (! _handle_flags[i].in_use.load())
        {
            size_t  temp = 0;
            if (_handle_flags[i].in_use.compare_exchange_weak(temp, 1))
            {
                while ( (temp = _handle_id.load()) <= i)
                { _handle_id.compare_exchange_weak(temp, i+1); }
                return i;
            }
            else --i;
        }
    }
    std::length_error("Exceeded predefined maximum of simultaneously registered threads (256)!");
    // unreachable
    return -1;
}


template <class P>
estrat_sync<P>::local_data_type::local_data_type(P& parent,
                                                 worker_strat_local_data &wstrat)
    : _parent(parent), _global(parent._global_exclusion), _worker_strat(wstrat),
      _id(_global.register_handle()), _epoch(0),
      _flags(_global._handle_flags[_id])
{ }

template <class P>
estrat_sync<P>::local_data_type::local_data_type(local_data_type&& source)
    : _parent(source._parent), _global(source._global),
      _worker_strat(source._worker_strat), _id(source._id), _epoch(source._epoch),
      _flags(source._flags)
{
    source._id = std::numeric_limits<size_t>::max();
    if ( _flags.table_op.load()  ||
         _flags.migrating.load()  ) std::cout << "wtf" << std::endl;
}

template <class P>
typename estrat_sync<P>::local_data_type&
estrat_sync<P>::local_data_type::operator=(local_data_type&& source)
{
    if (this == &source) return *this;

    this->~local_data_type();
    new (this) local_data_type(std::move(source));
    return *this;
}

template <class P>
estrat_sync<P>::local_data_type::~local_data_type()
{
    if (_id == std::numeric_limits<size_t>::max()) return;

    _flags.table_op.store(0);
    _flags.migrating.store(0);
    _flags.in_use.store(0);
}

template <class P>
typename estrat_sync<P>::hash_ptr_reference
estrat_sync<P>::local_data_type::get_table()
{
    _flags.table_op.store(1, std::memory_order_release);

    if (_global._currently_growing.load(std::memory_order_acquire))
    {
        rls_table();
        help_grow();
        return get_table();
    }
    auto temp = _global._g_table_r.load(std::memory_order_acquire);
    _epoch = temp->_version;
    return temp;
}

template <class P>
void
estrat_sync<P>::local_data_type::rls_table()
{
    _flags.table_op.store(0, std::memory_order_release);
}

template <class P>
void
estrat_sync<P>::local_data_type::grow()
{
    rls_table();

    size_t stage = 0;

    // STAGE 1 GENERATE TABLE AND SWAP IT SIZE_TO NEXT
    if (! change_stage<false>(stage, 1u)) { help_grow(); return; }

    auto t_cur   = _global._g_table_r.load(std::memory_order_acquire);
    auto t_next  = new base_table_type(
        t_cur->_mapper.resize(_parent._elements.load(std::memory_order_acquire),
                              _parent._dummies.load(std::memory_order_acquire)),
        t_cur->_version+1);

    wait_for_table_op();

    if (! _global._g_table_w.compare_exchange_strong(t_cur,
                                                     t_next,
                                                     std::memory_order_release))
    {
        delete t_next;
        std::logic_error("Next_table already replaced (at beginning of grow)!");
        return;
    }

    // STAGE 2 ALL THREADS CAN ENTER THE MIGRATION
    if (! change_stage(stage, 2u)) return;

    _worker_strat.execute_migration(*this, _epoch);

    //STAGE 3 WAIT FOR ALL THREADS, THEN CHANGE CURRENT TABLE
    if (! change_stage(stage, 3u)) return;

    wait_for_migration();

    auto rem_dummies = _parent._dummies.load();
    _parent._elements.fetch_sub(rem_dummies);
    _parent._dummies.fetch_sub(rem_dummies);

    if (! _global._g_table_r.compare_exchange_strong(t_cur,
                                                     t_next,
                                                     std::memory_order_release))
    {
        std::logic_error("Cur_table already replaced (at end of a grow)!");
        return;
    }

    //STAGE 4ISH THREADS MAY CONTINUE MASTER WILL DELETE THE OLD TABLE
    if (! change_stage(stage, 0)) return;

    delete t_cur;
}

template <class P>
void
estrat_sync<P>::local_data_type::help_grow()
{
    _worker_strat.execute_migration(*this, _epoch);

    //wait till growmaster replaced the current table
    while (_global._currently_growing.load(std::memory_order_acquire)) { }
}

template <class P>
size_t
estrat_sync<P>::local_data_type::migrate()
{
    // enter_migration();
    while (_global._currently_growing.load(std::memory_order_acquire) == 1);
    _flags.migrating.store(2, std::memory_order_release);

    // getCurr()
    auto curr = _global._g_table_r.load(std::memory_order_acquire);
    // getNext()
    auto next = _global._g_table_w.load(std::memory_order_acquire);

    if (curr->_version >= next->_version)
    {
        // leave_migration();
        _flags.migrating.store(0, std::memory_order_release);

        return next->_version;
    }
    //_parent.grow_count.fetch_add(
    blockwise_migrate(*curr, *next);//);//,
    //std::memory_order_release);

    // leave_migration();
    _flags.migrating.store(0, std::memory_order_release);

    return next->_version;
}



template <class P>
size_t
estrat_sync<P>::local_data_type::blockwise_migrate(base_table_type& source,
                                                   base_table_type& target)
{
    size_t n = 0;

    //get block + while block legal migrate and get new block
    size_t temp = source._current_copy_block.fetch_add(migration_block_size);
    while (temp < source._mapper.addressable_slots())
    {
        n += source.migrate(target, temp,
                            std::min(uint(temp+migration_block_size),
                                     uint(source._mapper.addressable_slots())));
        temp = source._current_copy_block.fetch_add(migration_block_size);
    }
    return n;
}

template <class P> template<bool EM>
bool
estrat_sync<P>::local_data_type::change_stage(size_t& stage, size_t next)
{
    auto result = _global._currently_growing
        .compare_exchange_strong(stage, next,
                                 std::memory_order_acq_rel);
    if (result)
    {
        stage = next;
    }
    else if (EM)
    {
        std::logic_error("Unexpected result during changeState!");
    }
    return result;
}

template <class P>
void
estrat_sync<P>::local_data_type::wait_for_table_op()
{
    auto end = _global._handle_id.load(std::memory_order_acquire);
    for (size_t i = 0; i < end; ++i)
    {
        while (_global._handle_flags[i]
               .table_op.load(std::memory_order_acquire));
    }
}

template <class P>
void
estrat_sync<P>::local_data_type::wait_for_migration()
{
    auto end = _global._handle_id.load(std::memory_order_acquire);
    for (size_t i = 0; i < end; ++i)
    {
        while (_global._handle_flags[i].migrating.load(std::memory_order_acquire));
    }
}


}
