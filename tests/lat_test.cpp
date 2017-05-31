/*******************************************************************************
 * tests/lat_test.cpp
 *
 * basic insert and find test (with different key generation)
 * for more information see below
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

template <class Hash>
int fill(Hash& hash, size_t end)
{
    auto err = 0u;

    execute_parallel(current_block, end,
        [&hash, &err](size_t i)
        {
            if (! hash.insert(range & (i*9827345982374782ull),
                                         i+2).second) ++err;
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int find_unsucc(Hash& hash, size_t end)
{
    auto err = 0u;

    execute_parallel(current_block, end,
        [&hash, &err](size_t i)
        {
            auto data = hash.find(range & (i*29124898243091298ull));
            if (data != hash.end())
            {
                if (i*29124898243091298ull != ((*data).second-2)*9827345982374782ull)
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
            auto data = hash.find(range & (i*9827345982374782ull));
            if (data == hash.end()) ++err;
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
            if (ThreadType::is_main) current_block.store(1);

            auto duration = ThreadType::synchronized(fill<Handle>,
                                                     ++stage, p-1, hash, n+1);

            ThreadType::out (duration.second/1000000., 10);
        }

        // STAGE3 n Finds Successful
        {
            if (ThreadType::is_main) current_block.store(1);

            auto duration = ThreadType::synchronized(find_unsucc<Handle>,
                                                     ++stage, p-1, hash, n+1);

            ThreadType::out (duration.second/1000000., 10);
        }

        // STAGE4 n Finds Successful
        {
            if (ThreadType::is_main) current_block.store(1);

            auto duration = ThreadType::synchronized(find_succ<Handle>,
                                                     ++stage, p-1, hash, n+1);

            ThreadType::out (duration.second/1000000., 10);
            ThreadType::out (errors.load(), 7);
        }

        ThreadType() << std::endl;
        if (ThreadType::is_main) errors.store(0);
    }

    if (ThreadType::is_main)
    {
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
