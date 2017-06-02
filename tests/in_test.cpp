/*******************************************************************************
 * tests/in_test.cpp
 *
 * basic insert and find test for more information see below
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
#include <iostream>

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

template<typename T>
inline void out(T t, size_t space)
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
    out("t_ins"   ,10);
    out("t_find_-",10);
    out("t_find_+",10);
    out("errors"  , 7);
    std::cout << std::endl;
}

void print_parameters(size_t i, size_t p, size_t n, size_t cap)
{
    out (i  , 3);
    out (p  , 3);
    out (n  , 9);
    out (cap, 9);
}

int generate_random(size_t n)
{
    std::uniform_int_distribution<uint64_t> dis(2,range);

    execute_blockwise_parallel(current_block, n,
        [&dis](size_t s, size_t e)
        {
            std::mt19937_64 re(s*10293903128401092ull);

            for (size_t i = s; i < e; i++)
            {
                keys[i] = dis(re);
            }
        });

    return 0;
}

template <class Hash>
int fill(Hash& hash, size_t end)
{
    auto err = 0u;

    execute_parallel(current_block, end,
        [&hash, &err](size_t i)
        {
            auto key = keys[i];
            if (! hash.insert(key, i+2).second )
            {
                // Insertion failed? Possibly already inserted.
                ++err;

            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int find_unsucc(Hash& hash, size_t begin, size_t end)
{
    auto err = 0u;

    execute_parallel(current_block, end,
        [&hash, &err, begin](size_t i)
        {
            auto key = keys[i];

            auto data = hash.find(key);

            if (data != hash.end())
            {
                // Random key found (unexpected)
                ++err;
            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}

template <class Hash>
int find_succ(Hash& hash, size_t end)
{
    auto err = 0u;

    execute_parallel(current_block, end,
        [&hash, &err, end](size_t i)
        {
            auto key = keys[i];

            auto data = hash.find(key);

            if (data == hash.end()) // || (*data).second != i+2)
            {
                ++err;
            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}

template <class Hash>
int bla(Hash& hash, size_t end)
{
    auto err = 0u;

    execute_parallel(current_block, end,
        [&hash, &err, end](size_t i)
        {
            auto key = keys[i];

            auto data = hash.find(key);

            if (std::pair<size_t,size_t>(*data).second != 666666666)
            {
                ++err;
            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}

template <class ThreadType>
int test_in_stages(size_t p, size_t id, size_t n, size_t cap, size_t it)
{
    using Handle = typename HASHTYPE::Handle;

    pin_to_core(id);
    size_t stage = 0;

    if (ThreadType::is_main)
    {
        keys = new uint64_t[2*n];
    }

    // STAGE0 Create Random Keys
    {
        if (ThreadType::is_main) current_block.store (0);
        ThreadType::synchronized(generate_random, ++stage, p-1, 2*n);
    }

    for (size_t i = 0; i < it; ++i)
    {
        // STAGE 0.1
        ThreadType::synchronized(
            [cap] (bool m) { if (m) hash_table = HASHTYPE(cap); return 0; },
            ++stage, p-1, ThreadType::is_main);

        if (ThreadType::is_main) print_parameters(i, p, n, cap);

        ThreadType::synchronized([]{ return 0; }, ++stage, p-1);

        Handle hash = hash_table.getHandle();


        // STAGE2 n Insertions
        {
            if (ThreadType::is_main) current_block.store(0);

            auto duration = ThreadType::synchronized(fill<Handle>,
                                                     ++stage, p-1, hash, n);

            ThreadType::out (duration.second/1000000., 10);
        }

        // STAGE3 n Finds Unsuccessful
        {
            if (ThreadType::is_main) current_block.store(n);

            auto duration = ThreadType::synchronized(find_unsucc<Handle>,
                                                     ++stage, p-1, hash, n, 2*n);

            ThreadType::out (duration.second/1000000., 10);
        }

        // STAGE4 n Finds Successful
        {
            if (ThreadType::is_main) current_block.store(0);

            auto duration = ThreadType::synchronized(find_succ<Handle>,
                                                     ++stage, p-1, hash, n);

            ThreadType::out (duration.second/1000000., 10);
            ThreadType::out (errors.load(), 7);
        }

        // if (ThreadType::is_main)
        // {
        //     //const
        //         Handle& chash = hash;
        //     size_t count = 0;
        //     for (auto it = chash.begin(); it != hash.end(); it++)
        //     {
        //         (*it) = 666666666;
        //         count += std::pair<size_t, size_t>(*it).second;
        //     }
        //     ThreadType::out (count                      , 10);
        //     ThreadType::out (hash.element_count_unsafe(), 10);
        //     ThreadType::out (hash.element_count_approx(), 10);
        // }

        // // STAGE4 n Finds Successful
        // {
        //     if (ThreadType::is_main) current_block.store(0);

        //     ThreadType::synchronized(bla<Handle>,
        //                              ++stage, p-1, hash, n);

        //     ThreadType::out (errors.load(), 6);
        // }

        #ifdef MALLOC_COUNT
        ThreadType::out (malloc_count_current(), 14);
        #endif

        ThreadType() << std::endl;
        if (ThreadType::is_main) errors.store(0);

        // Some Synchronization
        ThreadType::synchronized([]{ return 0; }, ++stage, p-1);
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
    size_t cap = c.intArg("-c" , n);
    size_t it  = c.intArg("-it", 5);
    if (! c.report()) return 1;

    print_parameter_list();

    start_threads(test_in_stages<TimedMainThread>,
                  test_in_stages<UnTimedSubThread>,
                  p, n, cap, it);
    return 0;
}
