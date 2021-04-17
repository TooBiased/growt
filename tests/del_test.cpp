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

#include <random>

#ifdef MALLOC_COUNT
#include "malloc_count.h"
#endif

#include "utils/default_hash.hpp"
#include "utils/thread_coordination.hpp"
#include "utils/pin_thread.hpp"
#include "utils/command_line_parser.hpp"
#include "utils/output.hpp"
#include "utils/data_structures/circular_buffer.hpp"

#include "tests/selection.hpp"

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

using del_config = table_config<size_t, size_t,
                                utils_tm::hash_tm::default_hash,allocator_type,
                                hmod::deletion>;
using table_type = typename del_config::table_type;


alignas(64) static table_type hash_table = table_type(0);
alignas(64) static uint64_t* keys;
alignas(64) static std::atomic_size_t current_block;
alignas(64) static std::atomic_size_t errors;
alignas(64) static std::atomic_size_t shared_temp;


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
    return 0;
}


template <class Hash>
int del_test(Hash& hash, size_t end, size_t size)
{
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

    shared_temp.fetch_add(found, std::memory_order_relaxed);
    return 0;
}

template <class ThreadType>
struct test_in_stages
{
    static int execute(ThreadType t, size_t n, size_t cap, size_t it, size_t ws)
    {
        utils_tm::pin_to_core(t.id);

        using handle_type = typename table_type::handle_type;

        if (ThreadType::is_main)
        {
            keys = new uint64_t[ws + n];
        }

        // STAGE0 Create Random Keys for insertions
        {
            if (ThreadType::is_main) current_block.store (0);
            t.synchronized(generate_keys, ws + n);
        }

        for (size_t i = 0; i < it; ++i)
        {
            // STAGE 0.01
            t.synchronized([cap](bool m)
                           { if (m) hash_table = table_type(cap); return 0; },
                           ThreadType::is_main);

            t.out << otm::width(5) << i
                  << otm::width(5) << t.p
                  << otm::width(11) << n
                  << otm::width(11) << cap
                  << otm::width(11) << ws;

            t.synchronize();

            handle_type hash = hash_table.get_handle();

            auto last_block_index = 0;
            auto local_buffer_size = ws/t.p + ((t.id < (ws%t.p)) ? 1 : 0);
            auto buffer = circular_buffer<size_t>{local_buffer_size+1};

            // STAGE0.1 prefill table with one block per thread
            {
                if (ThreadType::is_main) current_block.store(0);

                t.synchronized(
                    [&]()
                    {
                        auto temp = current_block.fetch_add(
                            local_buffer_size,
                            std::memory_order_relaxed);

                        auto err = 0;

                        for (size_t i = temp; i < temp + local_buffer_size; ++i)
                        {
                            auto key = keys[i];
                            auto r = hash.insert(key, 666);
                            if (! r.second)
                                err++;
                            buffer.push_back(key);
                        }
                        errors.fetch_add(err, std::memory_order_relaxed);
                        return 0;
                    });
            }

            // STAGE1 n Mixed Operations
            {
               auto duration = t.synchronized(
                    [&]()
                    {
                        auto err = 0;
                        ttm::execute_blockwise_parallel(
                            current_block, ws + n,
                            [&](size_t s, size_t e)
                            {
                                auto d = last_block_index;
                                last_block_index = s;

                                for (size_t i = s; i < e; ++i, ++d)
                                {
                                    auto insert_key = keys[i];
                                    auto r = hash.insert(insert_key, 666);
                                    if (! r.second) err++;

                                    auto erase_key = buffer.pop_front();
                                    if (! erase_key) { err++; continue; }
                                    auto e = hash.erase(erase_key.value());
                                    if (e != 1) err++;

                                    buffer.push_back(insert_key);

                                }
                            });
                        errors.fetch_add(err, std::memory_order_relaxed);
                        return 0;
                    });

                t.out << otm::width(12) << duration.second/1000000.;
            }

            // STAGE2 prefill table with pre elements
            {
                if (ThreadType::is_main) current_block.store(0);

                auto duration = t.synchronized(validate<handle_type>,
                                               hash, ws + n);

                t.out << otm::width(12) << duration.second/1000000.
                      << otm::width(9)  << shared_temp.load()
                      << otm::width(9)  << errors.load();
            }


#ifdef MALLOC_COUNT
            t.out << otm::width(16) << malloc_count_current();
#endif

            t.out << std::endl;

            if (ThreadType::is_main)
            {
                errors.store(0);
                shared_temp.store(0);
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
               << otm::width(9)  << "elem"
               << otm::width(9)  << "errors"
               << "    " << del_config::name()
               << std::endl;

    ttm::start_threads<test_in_stages>(p, n, cap, it, ws);

    return 0;
}
