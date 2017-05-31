/*******************************************************************************
 * tests/del_test.cpp
 *
 * deletion test for more information see below
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include "tests/selection.h"

#include "utils/hashfct.h"
#include "utils/test_coordination.h"
#include "utils/thread_basics.h"
#include "utils/commandline.h"

#include <random>

#ifdef MALLOC_COUNT
#include "malloc_count.h"
#endif


/*
 * This Test is meant to test the tables performance on uniform random inputs.
 * 0. Creating 2n random keys
 * 1. Inserting n elements (key, index) - the index can be used for validation
 * 2. Looking for n elements - using different keys (likely not finding any)
 * 3. Looking for the n inserted elements (hopefully finding all)
 *    (correctness test using the index)
 */

const static uint64_t range = (1ull << 62) -1;

alignas(64) static HASHTYPE hash_table = HASHTYPE(0);
alignas(64) static uint64_t* keys;
alignas(64) static std::atomic_size_t current_block;
alignas(64) static std::atomic_size_t errors;
alignas(64) static std::atomic_size_t unsucc_deletes;
alignas(64) static std::atomic_size_t succ_found;

template<typename T>
inline void out(T t, int space)
{
    std::cout.width(space);
    std::cout << t << " " << std::flush;
}

void print_parameter_list()
{
    out("#i"      , 3);
    out("p"       , 3);
    out("n"       , 9);
    out("cap"     , 9);
    out("w_size"  , 9);
    out("t_del"   , 10);
    out("t_val"   , 10);
    out("unsucc"  , 7);
    out("remain"  , 7);
    out("errors"  , 7);
    std::cout << std::endl;
}

void print_parameters(size_t i, size_t p, size_t n, size_t cap, size_t pre)
{
    out (i        , 3);
    out (p        , 3);
    out (n        , 9);
    out (cap      , 9);
    out (pre      , 9);
}

int generate_keys(size_t end)
{
    std::uniform_int_distribution<uint64_t> key_dis (2,range);

    execute_blockwise_parallel(current_block, end,
        [&key_dis](size_t s, size_t e)
        {
            std::mt19937_64 re(s*10293903128401092ull);

            for (size_t i = s; i < e; i++)
            {
                keys[i] = key_dis(re);
            }
        });

    return 0;
}

template <class Hash>
int prefill(Hash& hash, size_t pre)
{
    auto err = 0u;

    execute_parallel(current_block, pre,
        [&hash, &err](size_t i)
        {
            auto key = keys[i];
            auto temp = hash.insert(key, i+2);
            if (! temp.second )
            { ++err; }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int del_test(Hash& hash, size_t end, size_t size)
{
    auto err         = 0u;
    auto not_deleted = 0u;

    execute_parallel(current_block, end,
        [&hash, &err, &not_deleted, size](size_t i)
        {
            auto key = keys[i];

            auto temp = hash.insert(key, i+2);
            if (! temp.second )
            { ++err; }

            auto key2 = keys[i-size];

            if (! hash.erase(key2))
            { ++not_deleted; }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    unsucc_deletes.fetch_add(not_deleted, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int validate(Hash& hash, size_t end)
{
    auto err   = 0u;
    auto found = 0u;

    execute_parallel(current_block, end,
        [&hash, &err, &found](size_t i)
        {
            auto key  = keys[i];

            if ( hash.find(key) != hash.end() )
            {
                ++found;
            }
        });

    errors.fetch_add(err      , std::memory_order_relaxed);
    succ_found.fetch_add(found, std::memory_order_relaxed);
    return 0;
}


template <class ThreadType>
int test_in_stages(size_t p, size_t id, size_t n, size_t cap, size_t it, size_t ws)
{
    pin_to_core(id);

    using Handle = typename HASHTYPE::Handle;

    size_t stage  = 0;

    if (ThreadType::is_main)
    {
        keys = new uint64_t[ws + n];
    }

    // STAGE0 Create Random Keys for insertions
    {
        if (ThreadType::is_main) current_block.store (0);
        ThreadType::synchronized(generate_keys, ++stage, p-1, ws+n);
    }

    for (size_t i = 0; i < it; ++i)
    {
        // STAGE 0.01
        ThreadType::synchronized([cap](bool m)
                                 { if (m) hash_table = HASHTYPE(cap); return 0; },
                                 ++stage, p-1, ThreadType::is_main);

        if (ThreadType::is_main) print_parameters(i, p, n, cap, ws);
        ThreadType::synchronized([]{ return 0; }, ++stage, p-1);

        Handle hash = hash_table.getHandle();


        // STAGE0.1 prefill table with pre elements
        {
            if (ThreadType::is_main) current_block.store(0);

            ThreadType::synchronized(prefill<Handle>, ++stage, p-1, hash, ws);
        }

        // STAGE1 n Mixed Operations
        {
            if (ThreadType::is_main) current_block.store(ws);

            auto duration = ThreadType::synchronized(del_test<Handle>,
                                                     ++stage, p-1, hash, ws+n, ws);

            ThreadType::out (duration.second/1000000., 10);
        }

        // STAGE2 prefill table with pre elements
        {
            if (ThreadType::is_main) current_block.store(0);

            auto duration = ThreadType::synchronized(validate<Handle>,
                                                     ++stage, p-1, hash, ws+n);

            ThreadType::out (duration.second/1000000., 10);
            ThreadType::out (unsucc_deletes.load(), 7);
            ThreadType::out (succ_found.load(), 7);
            ThreadType::out (errors.load(), 7);
        }


        #ifdef MALLOC_COUNT
        ThreadType::out (malloc_count_current(), 14);
        #endif

        ThreadType() << std::endl;

        if (ThreadType::is_main)
        {
            errors.store(0);
            unsucc_deletes.store(0);
            succ_found.store(0);
        }
    }

    if (ThreadType::is_main)
    {
        delete[] keys;
        reset_stages();
    }

    return 0;
}


int main(int argn, char** argc)
{
    CommandLine c{argn, argc};
    size_t n   = c.intArg("-n" , 10000000);
    size_t p   = c.intArg("-p" , 4);
    size_t ws  = c.intArg("-ws", n/100);
    size_t cap = c.intArg("-c" , ws);
    size_t it  = c.intArg("-it", 5);
    if (! c.report()) return 1;

    print_parameter_list();

    start_threads(test_in_stages<TimedMainThread>,
                  test_in_stages<UnTimedSubThread>,
                  p, n, cap, it, ws);

    return 0;
}
