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

#include "utils/default_hash.hpp"
#include "utils/thread_coordination.hpp"
#include "utils/pin_thread.hpp"
#include "utils/command_line_parser.hpp"
#include "utils/output.hpp"

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
namespace otm = utils_tm::out_tm;
namespace ttm = utils_tm::thread_tm;

alignas(64) static HASHTYPE hash_table = HASHTYPE(0);
alignas(64) static uint64_t* keys;
alignas(64) static std::atomic_size_t current_block;
alignas(64) static std::atomic_size_t errors;
alignas(64) static std::atomic_size_t unsucc_deletes;
alignas(64) static std::atomic_size_t succ_found;


int generate_keys(size_t end)
{
    std::uniform_int_distribution<uint64_t> key_dis (2,range);

    ttm::execute_blockwise_parallel(current_block, end,
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

    ttm::execute_parallel(current_block, pre,
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

    ttm::execute_parallel(current_block, end,
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
    auto found = 0u;

    ttm::execute_parallel(current_block, end,
        [&hash, &found](size_t i)
        {
            auto key  = keys[i];

            if ( hash.find(key) != hash.end() )
            {
                ++found;
            }
        });

    succ_found.fetch_add(found, std::memory_order_relaxed);
    return 0;
}

template <class ThreadType>
struct test_in_stages
{
    static int execute(ThreadType t, size_t n, size_t cap, size_t it, size_t ws)
    {
        utils_tm::pin_to_core(t.id);

        using Handle = typename HASHTYPE::Handle;

        if (ThreadType::is_main)
        {
            keys = new uint64_t[ws + n];
        }

        // STAGE0 Create Random Keys for insertions
        {
            if (ThreadType::is_main) current_block.store (0);
            t.synchronized(generate_keys, ws+n);
        }

        for (size_t i = 0; i < it; ++i)
        {
            // STAGE 0.01
            t.synchronized([cap](bool m)
                           { if (m) hash_table = HASHTYPE(cap); return 0; },
                           ThreadType::is_main);

            t.out << otm::width(5) << i
                  << otm::width(5) << t.p
                  << otm::width(11) << n
                  << otm::width(11) << cap
                  << otm::width(11) << ws;

            t.synchronize();

            Handle hash = hash_table.get_handle();


            // STAGE0.1 prefill table with pre elements
            {
                if (ThreadType::is_main) current_block.store(0);

                t.synchronized(prefill<Handle>, hash, ws);
            }

            // STAGE1 n Mixed Operations
            {
                if (ThreadType::is_main) current_block.store(ws);

                auto duration = t.synchronized(del_test<Handle>,
                                               hash, ws+n, ws);

                t.out << otm::width(12) << duration.second/1000000.;
            }

            // STAGE2 prefill table with pre elements
            {
                if (ThreadType::is_main) current_block.store(0);

                auto duration = t.synchronized(validate<Handle>,
                                               hash, ws+n);

                t.out << otm::width(12) << duration.second/1000000.
                      << otm::width(9)  << unsucc_deletes.load()
                      << otm::width(9)  << succ_found.load()
                      << otm::width(9)  << errors.load();
            }


#ifdef MALLOC_COUNT
            t.out << otm::width(16) << malloc_count_current();
#endif

            t.out << std::endl;

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
        }

        return 0;
    }

};


int main(int argn, char** argc)
{
    utils_tm::command_line_parser c{argn, argc};
    size_t n   = c.int_arg("-n" , 10000000);
    size_t p   = c.int_arg("-p" , 4);
    size_t ws  = c.int_arg("-ws", n/100);
    size_t cap = c.int_arg("-c" , ws);
    size_t it  = c.int_arg("-it", 5);
    if (! c.report()) return 1;

    otm::out() << otm::width(5)  << "#i"
               << otm::width(5)  << "p"
               << otm::width(11) << "n"
               << otm::width(11) << "cap"
               << otm::width(11) << "w_size"
               << otm::width(12) << "t_del"
               << otm::width(12) << "t_val"
               << otm::width(9)  << "unsucc"
               << otm::width(9 ) << "remain"
               << otm::width(9)  << "errors"
               << std::endl;

    ttm::start_threads<test_in_stages>(p, n, cap, it, ws);

    return 0;
}
