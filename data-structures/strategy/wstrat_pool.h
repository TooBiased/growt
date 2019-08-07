/*******************************************************************************
 * data-structures/strategy/wstrat_pool.h
 *
 * see below
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef WSTRAT_POOL_H
#define WSTRAT_POOL_H

#include "allocator/counting_wait.h"

#include <atomic>
#include <thread>

/*******************************************************************************
 *
 * This is a worker strategy for our growtable.
 *
 * Every worker strategy has to implement the following
 *  - subclass: global_data_t      (is stored at the growtable object)
 *  - subclass: local_data_t       (is stored at each handle)
 *     - init(...)
 *     - deinit()
 *     - execute_migration(...)
 *
 * This specific strategy uses a thread-pool for growing.
 * Every thread who creates a handle will generate a growing
 * thread, which will help with each migration. The thread will
 * be stopped once the handle is deleted.
 *
 * NOTE: The migration thread will be pinned to the core, it was created from.
 *       This is good if all hardware threads are used and pinned .
 *
 ******************************************************************************/

namespace growt {

template <class Parent>
class WStratPool
{
public:

    // Globaly we have to store two "waiting objects" (futexes).
    // They are used to sleep until the next grow/nongrow phase (+wake up).
    class global_data_t
    {
    public:
        global_data_t() : _grow_wait(0), _user_wait(0) {}
        global_data_t(const global_data_t &) = delete;
        global_data_t & operator = (const global_data_t &) = delete;

        ~global_data_t() = default;

        counting_wait _grow_wait;
        counting_wait _user_wait;
    };


    // This is the function executed by the growing threads
    // wait for wakeup -> check if destroyed
    //                 -> check if growing -> help grow -> repeat
    template <class EStrat>
    static void grow_thread_func(EStrat& estrat,
                                 global_data_t& global,
                                 std::atomic_size_t& finished,
                                 cpu_set_t* aff)
    {
        uint epoch = 0;
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), aff);

        while (true)
        {
            global._grow_wait.wait_if(epoch);
            if (finished) break;

            auto next = estrat.migrate();

            global._user_wait.inc_if(epoch);
            global._user_wait.wake();
            epoch = next;
        }
        finished.store(2, std::memory_order_release);
    }


    // On init the growing thread is created, on deinit it is joined.
    // All migrations are reduced to waiting for the new table version
    // which is automatically created by the thread-pool.
    class local_data_t
    {
    public:
        Parent           &_parent;
        global_data_t    &_global;
        std::thread      _grow_thread;
        std::unique_ptr<std::atomic_size_t> _finished;

        local_data_t(Parent &parent)
            : _parent(parent),
              _global(parent._global_worker),
              _finished(new std::atomic_size_t(0))
        { }
        local_data_t(const local_data_t& source) = delete;
        local_data_t& operator=(const local_data_t& source) = delete;
        local_data_t(local_data_t&& rhs)
            : _parent(rhs._parent), _global(rhs._global),
              _grow_thread(std::move(rhs._grow_thread)),
              _finished(std::move(_finished))
        { }

        local_data_t& operator=(local_data_t&& rhs)
        {
            _parent = rhs._parent;
            _global = rhs._global;
            deinit();
            _grow_thread = std::move(rhs._grow_thread);
            _finished    = std::move(rhs._finished);
        }

        ~local_data_t() { }

        // creates and pins the thread
        template<class EStrat>
        inline void init(EStrat& estrat)
        {
            cpu_set_t cpuset;
            pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

            _grow_thread = std::thread(grow_thread_func<EStrat>, std::ref(estrat),
                                                                 std::ref(_global),
                                                                 std::ref(*_finished),
                                                                 &cpuset);
        }

        // sets a local destroy flag, and wakes up all growing threads
        // only the one local thread will be destroyed though. Since there is no
        // new table, no migration will be executed by the growing threads.
        inline void deinit()
        {
            if (_grow_thread.joinable())
            {
                _finished->store(1, std::memory_order_release);

                while (_finished->load(std::memory_order_acquire) < 2)
                    _global._grow_wait.wake();

                _grow_thread.join();
            }
        }

        template<class ESLocal>
        inline void execute_migration(ESLocal&, size_t epoch)
        {
            // lets instead tell somebody else and ...
            // wait lazily until somebody did this zzzzZZZzz
            if (_global._grow_wait.inc_if(epoch))
                _global._grow_wait.wake();

            _global._user_wait.wait_if(epoch);
        }
    };


};

}

#endif // WSTRAT_POOL_H
