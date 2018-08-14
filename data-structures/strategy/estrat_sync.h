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

#ifndef ESTRAT_SYNC_H
#define ESTRAT_SYNC_H

#include <atomic>
#include <stdexcept>
#include <iostream>

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
 *     - getTable()   (gets current table and protects it from destruction)
 *     - rlsTable()   (stops protecting the table)
 *     - grow()       (creates a new table and initiates a growing step)
 *     - helpGrow()   (called when an operation is unsuccessful,
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
class EStratSync
{
public:
    using BaseTable_t   = typename Parent::BaseTable_t;
    using WorkerStratL  = typename Parent::WorkerStrat_t::local_data_t;
    using HashPtr       = std::atomic<BaseTable_t*>;
    using HashPtrRef    = BaseTable_t*;


    class local_data_t;

    // STORED AT THE GLOBAL OBJECT
    //  - ATOMIC POINTERS TO BOTH CURRENT AND TARGET TABLE
    //  - FLAGS FOR ALL HANDLES (currently in critical section?/growing step?)
    class global_data_t
    {
    public:
        #define max_sim_threads 256

        global_data_t(size_t size_) : currently_growing(0), handle_id(0)
        {
            auto temp = new BaseTable_t(size_);
            g_table_r.store( temp, std::memory_order_relaxed );
            g_table_w.store( temp, std::memory_order_relaxed );
            //for (size_t i = 0; i<max_sim_threads; ++i)
            //    handle_flags[i]();
        }
        global_data_t(const global_data_t& source) = delete;
        global_data_t& operator=(const global_data_t& source) = delete;
        ~global_data_t()
        {
            // g_table_r == g_table_w since unused
            delete g_table_w.load();
        }
    private:
        friend local_data_t;

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
        std::atomic_size_t currently_growing;
        std::atomic_size_t handle_id;
        HashPtr g_table_r;
        HashPtr g_table_w;
        alignas(128) HandleFlags handle_flags[max_sim_threads];

        size_t registerHandle()
        {
            for (size_t i = 0; i < max_sim_threads; ++i)
            {
                if (! handle_flags[i].in_use.load())
                {
                    size_t  temp = 0;
                    if (handle_flags[i].in_use.compare_exchange_weak(temp, 1))
                    {
                        while ( (temp = handle_id.load()) <= i)
                        { handle_id.compare_exchange_weak(temp, i+1); }
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

    // STORED AT EACH HANDLE
    //  - REFERENCE TO THE OWNED FLAGS
    class local_data_t
    {
    public:

        local_data_t(Parent& parent, WorkerStratL &wstrat)
            : parent(parent), global(parent.global_exclusion), worker_strat(wstrat),
              id(global.registerHandle()), epoch(0),
              flags(global.handle_flags[id])
              //own_flag(global.writing[id<<4]), mig_flag(global.writing[(id<<4)+1])
        { }
        local_data_t(const local_data_t& source) = delete;
        local_data_t& operator=(const local_data_t& source) = delete;

        local_data_t(local_data_t&& source)
            : parent(source.parent), global(source.global),
              worker_strat(source.worker_strat), id(source.id), epoch(source.epoch),
              flags(source.flags)
        {
            source.id = std::numeric_limits<size_t>::max();
            if ( flags.table_op.load()  ||
                 flags.migrating.load()  ) std::cout << "wtf" << std::endl;
        }
        //=default;

        local_data_t& operator=(local_data_t&& source)
        {
            if (this == &source) return *this;

            this->~local_data_t();
            new (this) local_data_t(std::move(source));
            return *this;
        }
        //= default;

        ~local_data_t()
        {
            if (id == std::numeric_limits<size_t>::max()) return;

            flags.table_op.store(0);
            flags.migrating.store(0);
            flags.in_use.store(0);
        }

        inline void init() { }
        inline void deinit() { }

    private:
        Parent& parent;
        global_data_t& global;
        WorkerStratL&  worker_strat;

        size_t           id;
        size_t           epoch;

        typename global_data_t::HandleFlags& flags;
        //std::atomic_size_t& own_flag;
        //std::atomic_size_t& mig_flag;

    public:
        inline HashPtrRef getTable()
        {
            //own_flag = 1;
            flags.table_op.store(1, std::memory_order_release);

            if (global.currently_growing.load(std::memory_order_acquire))
            {
                rlsTable();
                helpGrow();
                return getTable();
            }
            auto temp = global.g_table_r.load(std::memory_order_acquire);
            epoch = temp->_version;
            return temp;
        }

        inline void rlsTable()
        {
            flags.table_op.store(0, std::memory_order_release);
        }

        void grow()
        {
            rlsTable();

            size_t stage = 0;

            // STAGE 1 GENERATE TABLE AND SWAP IT SIZE_TO NEXT
            if (! changeStage<false>(stage, 1u)) { helpGrow(); return; }

            auto t_cur   = global.g_table_r.load(std::memory_order_acquire);
            auto t_next  = new BaseTable_t(//t_cur->size << 1, t_cur->_version+1);
                        BaseTable_t::resize(t_cur->_capacity,
                            parent.elements.load(std::memory_order_acquire),
                            parent.dummies.load(std::memory_order_acquire)),
                        t_cur->_version+1);

            waitForTableOp();

            if (! global.g_table_w.compare_exchange_strong(t_cur,
                                                           t_next,
                                                           std::memory_order_release))
            {
                delete t_next;
                std::logic_error("Next_table already replaced (at beginning of grow)!");
                return;
            }
            //parent.elements.store(0, std::memory_order_release);
            //parent.dummies.store(0, std::memory_order_release);

            // STAGE 2 ALL THREADS CAN ENTER THE MIGRATION
            if (! changeStage(stage, 2u)) return;

            worker_strat.execute_migration(*this, epoch);//, t_cur, t_next);

            //STAGE 3 WAIT FOR ALL THREADS, THEN CHANGE CURRENT TABLE
            if (! changeStage(stage, 3u)) return;

            waitForMigration();

            //parent.elements.store(parent.grow_count.load(std::memory_order_acquire),
            //                      std::memory_order_release);
            auto rem_dummies = parent.dummies.load();
            parent.elements.fetch_sub(rem_dummies);
            parent.dummies.fetch_sub(rem_dummies);
            //parent.grow_count.store(0, std::memory_order_release);

            if (! global.g_table_r.compare_exchange_strong(t_cur,
                                                           t_next,
                                                           std::memory_order_release))
            {
                std::logic_error("Cur_table already replaced (at end of a grow)!");
                return;
            }

            //STAGE 4ISH THREADS MAY CONTINUE MASTER WILL DELETE THE OLD TABLE
            if (! changeStage(stage, 0)) return;

            delete t_cur;
        }


        inline void helpGrow()
        {
            worker_strat.execute_migration(*this, epoch);

                //wait till growmaster replaced the current table
            while (global.currently_growing.load(std::memory_order_acquire)) { }
        }


        inline size_t migrate()
        {
            // enter_migration();
            while (global.currently_growing.load(std::memory_order_acquire) == 1);
            flags.migrating.store(2, std::memory_order_release);

            // getCurr()
            auto curr = global.g_table_r.load(std::memory_order_acquire);
            // getNext()
            auto next = global.g_table_w.load(std::memory_order_acquire);

            if (curr->_version >= next->_version)
            {
                // leave_migration();
                flags.migrating.store(0, std::memory_order_release);

                return next->_version;
            }
            //parent.grow_count.fetch_add(
                blockwise_migrate(curr, next);//);//,
            //std::memory_order_release);

            // leave_migration();
            flags.migrating.store(0, std::memory_order_release);

            return next->_version;
        }

    private:
        template<bool ErrorMsg = true>
        inline bool changeStage(size_t& stage, size_t next)
        {
            auto result = global.currently_growing
                    .compare_exchange_strong(stage, next,
                                             std::memory_order_acq_rel);
            if (result)
            {
                stage = next;
            }
            else if (ErrorMsg)
            {
                std::logic_error("Unexpected result during changeState!");
            }
            return result;
        }

        inline void waitForTableOp()
        {
            auto end = global.handle_id.load(std::memory_order_acquire);
            for (size_t i = 0; i < end; ++i)
            {
                while (global.handle_flags[i]
                             .table_op.load(std::memory_order_acquire));
            }
        }

        inline void waitForMigration()
        {
            auto end = global.handle_id.load(std::memory_order_acquire);
            for (size_t i = 0; i < end; ++i)
            {
                while (global.handle_flags[i].migrating.load(std::memory_order_acquire));
            }
        }

    };
};

}

#endif // ESTRAT_SYNC_H
