/*******************************************************************************
 * tests/mix_test.cpp
 *
 * mixed inserts and finds test for more information see below
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

const static uint64_t range     = (1ull << 62) -1;
const static uint64_t read_flag = (1ull << 63);
namespace otm = utils_tm::out_tm;
namespace ttm = utils_tm::thread_tm;

alignas(64) static HASHTYPE  hash_table = HASHTYPE(0);
alignas(64) static uint64_t* keys;
alignas(64) static std::atomic_size_t current_block;
alignas(64) static std::atomic_size_t errors;
alignas(64) static std::atomic_size_t unsucc_finds;


int generate_insertions(size_t pre, size_t n, double wperc)
{
    std::uniform_real_distribution<double>  write_dis(0, 1.0);
    std::uniform_int_distribution<uint64_t> key_dis  (2,range);

    ttm::execute_blockwise_parallel(current_block, pre+n,
        [pre, wperc, &write_dis, &key_dis](size_t s, size_t e)
        {
            std::mt19937_64 re(s*10293903128401092ull);

            for (size_t i = s; i < e; i++)
            {
                if ( i < pre || write_dis(re) < wperc )
                {
                    keys[i] = key_dis(re);
                }
                else
                {
                    keys[i] = read_flag;
                }
            }
        });

    return 0;
}

int generate_reads(size_t pre, size_t n, size_t window)
{
    ttm::execute_blockwise_parallel(current_block, pre+n,
                               [pre, window](size_t s, size_t e)
        {
            std::mt19937_64 re(s*10293903128401092ull);

            for (size_t i = s; i < e; i++)
            {
                if ( keys[i] & read_flag )
                {
                    auto right_bound = std::max(pre, i-window);
                    std::uniform_int_distribution<size_t> write_dis(0,right_bound);

                    size_t tries = 0;
                    uint64_t key = 0;
                    do
                    {
                        ++tries;
                        key = keys[write_dis(re)];
                        if (tries > 100)
                        {
                            std::uniform_int_distribution<size_t> safe(0,pre-1);
                            key = keys[safe(re)];
                        }
                    } while ( key & read_flag );

                    keys[i] |= key;
                }
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
            if (! temp.second)
            { ++err; }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int mixed_test(Hash& hash, size_t end)
{
    auto err       = 0u;
    auto not_found = 0u;

    ttm::execute_parallel(current_block, end,
        [&hash, &err, &not_found](size_t i)
        {
            auto key = keys[i];
            if (key & read_flag)
            {
                auto data = hash.find( key ^ read_flag );
                if      (data == hash.end()) { ++not_found; }
                else if ((*data).second > i+2) { ++err; }
            }
            else if (! hash.insert(key, i+2).second )
            { ++err;}


        });

    errors.fetch_add(err, std::memory_order_relaxed);
    unsucc_finds.fetch_add(not_found, std::memory_order_relaxed);
    return 0;
}



template <class ThreadType>
struct test_in_stages
{
    static int execute(ThreadType t, size_t n, size_t cap, size_t it,
                       size_t pre, size_t win, double wperc)
    {
        utils_tm::pin_to_core(t.id);

        using Handle = typename HASHTYPE::Handle;

        if (ThreadType::is_main)
        {
            keys = new uint64_t[pre+n];
        }

        // STAGE0 Create Random Keys for insertions
        {
            if (ThreadType::is_main) current_block.store (0);
            t.synchronized(generate_insertions, pre ,n, wperc);
        }

        // STAGE0.1 Create Random Keys for reads (previously inserted keys
        {
            if (ThreadType::is_main) current_block.store (pre);

            t.synchronized(generate_reads, pre ,n, win);
        }

        for (size_t i = 0; i < it; ++i)
        {

            // STAGE 0.011
            t.synchronized([cap](bool m)
                           { if (m) hash_table = HASHTYPE(cap); return 0; },
                           ThreadType::is_main);

            t.out << otm::width(5)  << i
                  << otm::width(5)  << t.p
                  << otm::width(11) << n
                  << otm::width(11) << cap
                  << otm::width(8)  << wperc;

            t.synchronize();

            Handle hash = hash_table.get_handle();

            // STAGE0.2 prefill table with pre elements
            {
                if (ThreadType::is_main) current_block.store(0);

                t.synchronized(prefill<Handle>, hash, pre);
            }

            // STAGE1 n Mixed Operations
            {
                if (ThreadType::is_main) current_block.store(pre);

                auto duration = t.synchronized(mixed_test<Handle>, hash, pre+n);

                t.out << otm::width(12) << duration.second/1000000.
                      << otm::width(9) << unsucc_finds.load()
                      << otm::width(9) << errors.load();
            }

            if (ThreadType::is_main)
            {
                errors.store(0);
                unsucc_finds.store(0);
            }

            t.out << std::endl;
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
    size_t n     = c.int_arg("-n"  , 10000000);
    size_t p     = c.int_arg("-p"  , 4);
    size_t it    = c.int_arg("-it" , 5);
    size_t pre   = c.int_arg("-pre", p*ttm::block_size);
    size_t win   = c.int_arg("-win", pre);
    double wperc = c.double_arg("-wperc", 0.5);

    size_t cap   = c.int_arg("-c"  , pre+n*wperc);
    if (! c.report()) return 1;

    otm::out() << otm::width(5)  << "#i"
               << otm::width(5)  << "p"
               << otm::width(11) << "n"
               << otm::width(11) << "cap"
               << otm::width(8)  << "w_per"
               << otm::width(12) << "t_mix"
               << otm::width(9)  << "unfound"
               << otm::width(9)  << "errors"
               << std::endl;


    ttm::start_threads<test_in_stages>(p, n, cap, it, pre, win, wperc);

    return 0;
}
