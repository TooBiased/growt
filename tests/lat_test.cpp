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

#include "utils/default_hash.hpp"
#include "utils/thread_coordination.hpp"
#include "utils/pin_thread.hpp"
#include "utils/command_line_parser.hpp"
#include "utils/output.hpp"

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
namespace otm = utils_tm::out_tm;
namespace ttm = utils_tm::thread_tm;

alignas(64) static HASHTYPE hash_table = HASHTYPE(0);
alignas(64) static std::atomic_size_t current_block;
alignas(64) static std::atomic_size_t errors;

template <class Hash>
int fill(Hash& hash, size_t end)
{
    auto err = 0u;

    ttm::execute_parallel(current_block, end,
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

    ttm::execute_parallel(current_block, end,
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

    ttm::execute_parallel(current_block, end,
        [&hash, &err, end](size_t i)
        {
            auto data = hash.find(range & (i*9827345982374782ull));
            if (data == hash.end()) ++err;
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}

template <class ThreadType>
struct test_in_stages
{
    static int execute(ThreadType t, size_t n, size_t cap, size_t it)
    {
        using Handle = typename HASHTYPE::Handle;

        utils_tm::pin_to_core(t.id);

        for (size_t i = 0; i < it; ++i)
        {
            // STAGE 0.1
            t.synchronized(
                [cap] (bool m) { if (m) hash_table = HASHTYPE(cap); return 0; },
                ThreadType::is_main);

            t.out << otm::width(5) << i
                  << otm::width(5) << t.p
                  << otm::width(11) << n
                  << otm::width(11) << cap;

            t.synchronize();

            Handle hash = hash_table.get_handle();

            // STAGE2 n Insertions
            {
                if (ThreadType::is_main) current_block.store(1);

                auto duration = t.synchronized(fill<Handle>, hash, n+1);

                t.out << otm::width(12) << duration.second/1000000.;
            }

            // STAGE3 n Finds Successful
            {
                if (ThreadType::is_main) current_block.store(1);

                auto duration = t.synchronized(find_unsucc<Handle>, hash, n+1);

                t.out << otm::width(12) << duration.second/1000000.;
            }

            // STAGE4 n Finds Successful
            {
                if (ThreadType::is_main) current_block.store(1);

                auto duration = t.synchronized(find_succ<Handle>, hash, n+1);

                t.out << otm::width(12) << duration.second/1000000.;
                t.out << otm::width(9) << errors.load();
            }

            t.out << std::endl;
            if (ThreadType::is_main) errors.store(0);
        }
        return 0;
    }
};



int main(int argn, char** argc)
{
    utils_tm::command_line_parser c{argn, argc};
    size_t n   = c.int_arg("-n" , 10000000);
    size_t p   = c.int_arg("-p" , 4);
    size_t cap = c.int_arg("-c" , n);
    size_t it  = c.int_arg("-it", 5);
    if (! c.report()) return 1;

    otm::out() << otm::width(5)  << "#i"
               << otm::width(5)  << "p"
               << otm::width(11) << "n"
               << otm::width(11) << "cap"
               << otm::width(12) << "t_ins"
               << otm::width(12) << "t_find_-"
               << otm::width(12) << "t_find_+"
               << otm::width(9)  << "errors"
               << std::endl;

    ttm::start_threads<test_in_stages>(p, n, cap, it);
    return 0;
}
