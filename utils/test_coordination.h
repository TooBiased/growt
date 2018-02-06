/*******************************************************************************
 * utils/test_coordination.h
 *
 * Offers low level functionality for thread synchronization
 * and parallel for loops (used to simplify writing tests/benchmarks)
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef TEST_COORDINATION_H
#define TEST_COORDINATION_H

#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <type_traits>

static std::atomic_size_t        level;
static std::atomic_size_t        wait_end;
static std::atomic_size_t        wait_start;
static std::atomic_size_t        n_output;

static std::chrono::time_point<std::chrono::high_resolution_clock> start;

static thread_local std::chrono::time_point<std::chrono::high_resolution_clock> start_own;

void reset_stages()
{
    level.store(0);
    wait_end.store(0);
    wait_start.store(0);
    n_output.store(0);
}


// MAINTHREAD SYNCHRONIZATION PRIMITIVES
//   TIMED
inline void start_stage_timed(size_t p, size_t lvl)
{
    while(wait_start.load(std::memory_order_acquire) < p);
    wait_start.store(0, std::memory_order_release);

    start = std::chrono::high_resolution_clock::now();
    level.store(lvl, std::memory_order_release);
}

inline size_t wait_for_finished_timed(size_t p)
{
    while (wait_end.load(std::memory_order_acquire) < p);
    wait_end.store(0, std::memory_order_release);
    return std::chrono::duration_cast<std::chrono::nanoseconds>
            (std::chrono::high_resolution_clock::now() - start).count();

}

//   UNTIMED
inline void start_stage(size_t p, size_t lvl)
{
    while(wait_start.load(std::memory_order_acquire) < p);
    wait_start.store(0, std::memory_order_release);

    level.store(lvl, std::memory_order_release);
}

inline size_t wait_for_finished(size_t p)
{
    while (wait_end.load(std::memory_order_acquire) < p);
    wait_end.store(0, std::memory_order_release);

    return 0ull;
}



// SUBTHREAD SYNCHRONIZATION PRIMITIVES
//   TIMED
inline void wait_for_stage_local_timed(size_t lvl)
{
    wait_start.fetch_add(1, std::memory_order_acq_rel);
    while (level.load(std::memory_order_acquire) < lvl);
    start_own = std::chrono::high_resolution_clock::now();
}

inline size_t finished_stage_local_timed()
{
    wait_end.fetch_add(1, std::memory_order_acq_rel);
    return std::chrono::duration_cast<std::chrono::nanoseconds>
            (std::chrono::high_resolution_clock::now() - start_own).count();
}

//   TIMED GLOBAL (only end because start is taken by mainthread)
inline size_t finished_stage_global_timed()
{
    wait_end.fetch_add(1, std::memory_order_acq_rel);
    return std::chrono::duration_cast<std::chrono::nanoseconds>
            (std::chrono::high_resolution_clock::now() - start).count();
}

//   UNTIMED
inline void wait_for_stage(size_t lvl)
{
    wait_start.fetch_add(1, std::memory_order_acq_rel);
    while (level.load(std::memory_order_acquire) < lvl);
}

inline size_t finished_stage()
{
    wait_end.fetch_add(1, std::memory_order_acq_rel);
    return 0;
}



// STARTS P-1 TFunctor THREADS, THEN EXECUTES MFunctor, THEN REJOINS GENERATED THREADS
template<typename MFunctor, typename TFunctor, typename ... Types>
inline int start_threads(MFunctor mf, TFunctor tf, size_t p, Types&& ... param)
{
    std::thread* local_thread = new std::thread[p-1];

    for (size_t i = 0; i < p-1; ++i)
    {
        local_thread[i] = std::thread(std::forward<TFunctor>(tf), p, i+1,
                                      std::ref(std::forward<Types>(param))...);
    }

    int temp = std::forward<MFunctor>(mf)(p, 0, std::forward<Types>(param)...);

    // CLEANUP THREADS
    for (size_t i = 0; i < p-1; ++i)
    {
        local_thread[i].join();
    }

    delete[] local_thread;

    return temp;
}



// MAIN THREAD CLASS
template <void (start) (size_t, size_t), size_t (end) (size_t)>
struct MainThread
{
    template<typename Functor, typename ... Types>
    inline static std::pair<typename std::result_of<Functor(Types&& ...)>::type, size_t>
        synchronized(Functor f, size_t stage, size_t p, Types&& ... param)
    {
        start(p, stage); //start_stage_timed(p, stage);
        auto temp = std::forward<Functor>(f)(std::forward<Types>(param) ... );
        return std::make_pair(std::move(temp), end(p)); //wait_for_finished_timed(p));
    }

    template<typename Functor, typename ... Types>
    inline static typename std::result_of<Functor(Types&& ...)>::type
        only_main(Functor f, Types&& ... param)
    {
        return std::forward<Functor>(f)(std::forward<Types>(param) ... );
    }


    static constexpr bool is_main = true;


    template<typename T>
    inline MainThread& operator<<(T&& t)//T&& t)
    {
        std::cout << std::forward<T>(t);
        return *this;
    }


    using omanip_t = std::ostream& (*) (std::ostream &);
    inline MainThread& operator<<(omanip_t t)
    {
        std::cout << t;
        return *this;
    }

    template<typename T>
    inline static void out(T t, size_t space)
    {
        std::cout.width(space);
        std::cout << t << " " << std::flush;
    }

    template<typename T>
    inline static void outAll(size_t id, size_t p, T t, size_t space)
    {
        auto lvl = n_output.load();

        while(wait_start.load() < id);
        std::cout.width(space);
        std::cout << t << " " << std::flush;
        wait_start.fetch_add(1);
        if (id == p-1)
        {
            n_output.fetch_add(1);
        }
        else while(n_output.load() <= lvl);

        while (wait_end.load() < p-1);
        wait_end.store(0);
    }
};

using   TimedMainThread = MainThread<start_stage_timed, wait_for_finished_timed>;
using UnTimedMainThread = MainThread<start_stage      , wait_for_finished>;



// SUB THREAD CLASS
template <void (start)(size_t), size_t (end)()>
struct SubThread
{
    template<typename Functor, typename ... Types>
    inline static std::pair<typename std::result_of<Functor(Types&& ...)>::type, size_t>
    synchronized(Functor f, size_t stage, size_t /*p unused*/, Types&& ... param)
    {
        start(stage);//wait_for_stage(stage);
        auto temp = std::forward<Functor>(f)(std::forward<Types>(param)...);
        //finished_stage();
        return std::make_pair(temp, end());
    }

    template<typename Functor, typename ... Types>
    inline static typename std::result_of<Functor(Types&& ...)>::type only_main(Functor f, Types&& ... param)
    {
        return typename std::result_of<Functor(Types&& ...)>::type();
    }

    static constexpr bool is_main = false;

    template<typename T>
    inline SubThread& operator<<(T&&)
    {
        return *this;
    }

    using omanip_t = std::ostream& (*) (std::ostream &);
    inline SubThread& operator<<(omanip_t)
    {
        return *this;
    }

    template<typename T>
    inline static void out(T, size_t)
    {
    }

    template<typename T>
    inline static void outAll(size_t id, size_t p, T t, size_t space)
    {
        auto lvl = n_output.load();

        while(wait_start.load() < id);
        std::cout.width(space);
        std::cout << t << " " << std::flush;
        wait_start.fetch_add(1);
        if (id == p-1)
        {
            wait_start.store(0);
            n_output.fetch_add(1);
        }
        else while(n_output.load() <= lvl);
        wait_end.fetch_add(1);
    }
};

//time is measured relative to the global start
using  LocTimedSubThread = SubThread<wait_for_stage_local_timed, finished_stage_local_timed>;
using GlobTimedSubThread = SubThread<wait_for_stage            , finished_stage_global_timed>;
using   UnTimedSubThread = SubThread<wait_for_stage            , finished_stage>;





static const size_t block_size = 4096;



//BLOCKWISE EXECUTION IN PARALLEL
template<typename Functor, typename ... Types>
inline void execute_parallel(std::atomic_size_t& global_counter, size_t e,
                                       Functor f, Types&& ... param)
{
    auto c_s = global_counter.fetch_add(block_size);
    while (c_s < e)
    {
        auto c_e = std::min(c_s+block_size, e);
        for (size_t i = c_s; i < c_e; ++i)
            f(i, std::forward<Types>(param)...);
        c_s = global_counter.fetch_add(block_size);
    }
}

template<typename Functor, typename ... Types>
inline void execute_blockwise_parallel(std::atomic_size_t& global_counter, size_t e,
                                       Functor f, Types&& ... param)
{
    auto c_s = global_counter.fetch_add(block_size);
    while (c_s < e)
    {
        auto c_e = std::min(c_s+block_size, e);
        f(c_s, c_e, std::forward<Types>(param)...);
        c_s = global_counter.fetch_add(block_size);
    }
}

#endif // TEST_COORDINATION_H
