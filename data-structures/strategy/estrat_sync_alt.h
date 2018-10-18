/*******************************************************************************
 * data-structures/strategy/estrat_sync_alt.h
 *
 * see below
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef ESTRAT_SYNC_NUMA_H
#define ESTRAT_SYNC_NUMA_H

#include <atomic>
#include <stdexcept>
#include <iostream>

// EXPERIMENTAL !!!!!!!!

/*******************************************************************************
 *
 * This is an EXPERIMENTAL exclusion strategy for our growtable.
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

template<class Table_t> size_t blockwise_migrate(Table_t source, Table_t target);

template <class Parent>
class EStratSyncNUMA
{
public:
    static constexpr size_t max_sim_threads = 256;

    using BaseTable_t   = typename Parent::BaseTable_t;
    using HashPtrRef    = BaseTable_t*;

    class local_data_t;

    // STORED AT THE GLOBAL OBJECT
    //  - ATOMIC POINTERS TO BOTH CURRENT AND TARGET TABLE
    //  - POINTERS TO ALL HANDLES (TO FIND THE LOCAL FLAGS)
    class global_data_t
    {
    public:

        global_data_t(size_t size_) : _currently_growing(0), _handle_id(0)
        {
            auto temp = new BaseTable_t(size_);
            _g_table_r.store( temp, std::memory_order_relaxed );
            _g_table_w.store( temp, std::memory_order_relaxed );
            for (size_t i = 0; i<max_sim_threads; ++i) _handle_flags[i].store(nullptr);
        }
        global_data_t(const global_data_t& source) = delete;
        global_data_t& operator=(const global_data_t& source) = delete;
        ~global_data_t()
        {
            // g_table_r == g_table_w since unused
            delete _g_table_w.load();
        }

    private:
        using HashPtr = std::atomic<BaseTable_t*>;
        friend local_data_t;

        struct alignas(64) HandleFlags
        {
            std::atomic_size_t table_op;
            std::atomic_size_t migrating;

            constexpr HandleFlags() : table_op(0), migrating(0) { }
        };


        alignas(64) HashPtr _g_table_r;
        /*same*/    HashPtr _g_table_w;
        /*same*/    std::atomic_size_t _currently_growing;
        /*same*/    std::atomic_size_t _handle_id;
        alignas(64) std::atomic<HandleFlags*> _handle_flags[max_sim_threads];

        size_t registerHandle(HandleFlags* flags)
        {
            for (size_t i = 0; i < max_sim_threads; ++i)
            {
                HandleFlags* temp = nullptr;
                if (! _handle_flags[i].load())
                {
                    if (_handle_flags[i].compare_exchange_weak(temp, flags))
                    {
                        size_t temps = 0;
                        while( (temps = _handle_id.load()) < _handle_id )
                        { _handle_id.compare_exchange_weak(temps, i+1); }
                        return i;
                    }
                    else --i;
                }
            }
            std::length_error("Exceeded predefined maximum of simultaneously registered threads (256)!");
            // unreachable
            return -1;
        }

    };

    class local_data_t
    {
    private:
        using WorkerStratL = typename Parent::WorkerStrat_t::local_data_t;
        using Flag_t       = typename global_data_t::HandleFlags;
    public:
        local_data_t(Parent& parent, WorkerStratL &wstrat)
            : _parent(parent), _global(parent._global_exclusion), _worker_strat(wstrat),
              _id(_global.registerHandle(&(this->_flags))), _epoch(0)
        {   }

        local_data_t(const local_data_t& source) = delete;
        local_data_t& operator=(const local_data_t& source) = delete;


        local_data_t(local_data_t&& rhs)
            : _parent(rhs._parent), _global(rhs._global), _worker_strat(rhs._worker_strat),
              _id(rhs._id), _flags()
        {
            if (_id < max_sim_threads)
            {
                auto temp = &(rhs._flags);
                if (_global._handle_flags[_id].compare_exchange_strong(temp, &_flags))
                {
                    while(_global._currently_growing.load()) { }
                }
                else
                {
                    std::logic_error("Inconsistent state: in move local_data (corrupted flags pointer)!");
                }
                rhs._id = max_sim_threads;
            }
        }

        local_data_t& operator=(local_data_t&& rhs)
        {
            if (this == &rhs) return *this;

            this->~local_data_t();
            new (this) local_data_t(rhs);
            return *this;
        }

        ~local_data_t()
        {
            // was this moved from?!
            if (_id < max_sim_threads)
            {

                Flag_t* temp = &_flags;
                if (_global._handle_flags[_id].compare_exchange_strong(temp, nullptr))
                {
                    while(_global._currently_growing.load()) { }
                }
                else
                {
                    // Since this cannot actually happen, we throw an exception even out of dtor!
                    //std::logic_error("Could not free local_data correctly (flags pointer corrupted)!");
                    std::cerr  << "HashTableHandle is corrupted shutdown program!" << std::endl;
                    std::cerr  << "This happened in destructor, therefore no exception is thrown!" << std::endl;
                    std::cout << "HashTableHandle is corrupted shutdown program!" << std::endl;
                    exit(32);
                }
            }
        }

        inline void init() { }
        inline void deinit() { }

    private:
        Parent&        _parent;
        global_data_t& _global;
        WorkerStratL&  _worker_strat;

        size_t         _id;
        size_t         _epoch;

        Flag_t         _flags;

    public:
        inline HashPtrRef get_table()
        {
            _flags.table_op.store(1);//, std::memory_order_release);

            if (_global._currently_growing.load()) //std::memory_order_acquire))
            {
                rls_table();
                help_grow();
                return get_table();
            }
            auto temp = _global._g_table_r.load(); //std::memory_order_acquire);
            _epoch = temp->_version;
            return temp;
        }

        inline void rls_table()
        {
            //own_flag = 0;
            _flags.table_op.store(0); //, std::memory_order_release);
        }

        void grow()
        {
            rls_table();

            size_t stage = 0;

            // STAGE 1 GENERATE TABLE AND SWAP IT INTO NEXT
            if (! change_stage<false>(stage, 1u)) { help_grow(); return; }

            auto t_cur   = _global._g_table_r.load();//std::memory_order_acquire);
            auto t_next  = new BaseTable_t(//t_cur->size << 1, t_cur->_version+1);
                        BaseTable_t::resize(t_cur->_capacity,
                                           _parent._elements.load(),//std::memory_order_acquire),
                                           _parent._dummies.load()),//std::memory_order_acquire)),
                        t_cur->_version+1);

            wait_for_table_op();

            if (! _global._g_table_w.compare_exchange_strong(t_cur,
                                                           t_next))//,
                //std::memory_order_release))
            {
                delete t_next;
                std::logic_error("Inconsistent state: next_table already replaced (at beginning of grow)!");
                return;
            }
            //parent.elements.store(0);//, std::memory_order_release);
            //parent.dummies.store(0);//, std::memory_order_release);

            // STAGE 2 ALL THREADS CAN ENTER THE MIGRATION
            if (! change_stage(stage, 2u)) return;

            //blockwise_migrate(t_cur, t_next);
            _worker_strat.execute_migration(*this, _epoch);

            //STAGE 3 WAIT FOR ALL THREADS, THEN CHANGE CURRENT TABLE
            if (! change_stage(stage, 3u)) return;

            wait_for_migration();

            auto temp = _parent._dummies.load();
            _parent._dummies .fetch_sub(temp);
            _parent._elements.fetch_sub(temp);

            // _parent._elements.store(_parent._grow_count.load(std::memory_order_acquire),
            //                       std::memory_order_release);
            // _parent._dummies.store(0, std::memory_order_release);
            // _parent._grow_count(0, std::memory_order_release);

            if (! _global._g_table_r.compare_exchange_strong(t_cur,
                                                           t_next))//,
                //std::memory_order_release))
            {
                std::logic_error("Inconsistent state: cur_table already replaced (at end of a grow)!");
                return;
            }

            //STAGE 4ISH THREADS MAY CONTINUE MASTER WILL DELETE THE OLD TABLE
            if (! change_stage(stage, 0)) return;

            delete t_cur;
        }

        inline void help_grow()
        {
            _worker_strat.execute_migration(*this, _epoch);

                //wait till growmaster replaced the current table
            while (_global._currently_growing.load()) { } //std::memory_order_acquire)) { }
        }

        inline size_t migrate()
        {
            // enter migration()
            while (_global._currently_growing.load() == 1) { } //}std::memory_order_acquire) == 1) { }
            _flags.migrating.store(2);//, std::memory_order_release);

            // getCurr() and getNext()
            auto curr = _global._g_table_r.load(std::memory_order_acquire);
            auto next = _global._g_table_w.load(std::memory_order_acquire);

            if (curr->_version >= next->_version)
            {
                _flags.migrating.store(0);//, std::memory_order_release);

                return next->_version;
            }
            //parent.grow_count.fetch_add(
            blockwise_migrate(curr, next);//);//,
            //std::memory_order_release);

            // leave_migration();
            _flags.migrating.store(0);//, std::memory_order_release);

            return next->_version;
        }

    private:
        template<bool ErrorMsg = true>
        inline bool change_stage(size_t& stage, size_t next)
        {
            auto result = _global._currently_growing
                .compare_exchange_strong(stage, next);//,
            //std::memory_order_acq_rel);
            if (result)
            {
                stage = next;
            }
            else if (ErrorMsg)
            {
                std::logic_error("Inconsistent state: found during changeState!");
            }
            return result;
        }

        inline void wait_for_table_op()
        {
            auto end = _global._handle_id.load();//std::memory_order_acquire);
            for (size_t i = 0; i < end; ++i)
            {
                auto flags_ptr = _global._handle_flags[i].load();//std::memory_order_acquire);
                if (flags_ptr)
                {
                    while (flags_ptr->table_op.load()) { } //}std::memory_order_acquire)) { }
                }
            }
        }

        inline void wait_for_migration()
        {
            auto end = _global._handle_id.load();//std::memory_order_acquire);
            for (size_t i = 0; i < end; ++i)
            {
                auto flags_ptr = _global._handle_flags[i].load();//std::memory_order_acquire);
                if (flags_ptr)
                {
                    while (flags_ptr->migrating.load()) { } //}std::memory_order_acquire)) { }
                }
            }
        }

    };
};

}

#endif // ESTRAT_SYNC_ALT_H
