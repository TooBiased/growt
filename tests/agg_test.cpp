/*******************************************************************************
 * tests/agg_test.cpp
 *
 * aggregation test for more information see below
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include "tests/selection.h"
#include "data-structures/returnelement.h"

#include "utils/default_hash.hpp"
#include "utils/zipf_keygen.hpp"
#include "utils/thread_coordination.hpp"
#include "utils/pin_thread.hpp"
#include "utils/command_line_parser.hpp"
#include "utils/output.hpp"

#include "example/update_fcts.h"

#include <random>

#ifdef MALLOC_COUNT
#include "malloc_count.h"
#endif

/*
 * This Test is meant to test the tables performance on uniform random inputs.
 * 0. Creating n random keys with zipf distribution between [2..n+1]
 * 1. Inserting n random keys using the insert_or_update Function
 *    [1..n]
 * 2. Validating the end result looking for each key and accumulating the results
 */

namespace otm = utils_tm::out_tm;
namespace ttm = utils_tm::thread_tm;


alignas(64) static HASHTYPE hash_table = HASHTYPE(0);
alignas(64) static uint64_t* keys;
alignas(64) static std::atomic_size_t       current_block;
alignas(64) static std::atomic_size_t       errors;
alignas(64) static std::atomic_size_t       valsum;
alignas(64) static utils_tm::zipf_generator zipf_gen;

int generate_random(size_t n)
{
    ttm::execute_blockwise_parallel(current_block, n,
        [](size_t s, size_t e)
        {
            std::mt19937_64 re(s*10293903128401092ull);

            zipf_gen.generate(re, &keys[s], e-s);
        });

    return 0;
}

template <class Hash>
int aggregate(Hash& hash, size_t n)
{
    auto err = 0u;
    ttm::execute_parallel(current_block, n,
                     [&hash, &err](size_t i)
        {
            auto key = keys[i];
            if (hash.insert_or_update(key, 1,
                                      growt::example::Increment(), 1)
                    .first == hash.end()) ++err;
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}

template <class Hash>
int validate_aggregate(Hash& hash, size_t n)
{
    auto sum = 0u;

    ttm::execute_parallel(current_block, n,
        [&hash, &sum](size_t i)
        {
            auto data = hash.find(i+2);
            if (data != hash.end()) { sum += (*data).second; };
        });

    valsum.fetch_add(sum, std::memory_order_relaxed);
    return 0;
}

template<class ThreadType>
struct test_in_stages
{
    static int execute(ThreadType t, size_t n, size_t cap, size_t it, double con)
    {

        utils_tm::pin_to_core(t.id);

        using Handle = typename HASHTYPE::Handle;

        if (ThreadType::is_main)
        {
            keys = new uint64_t[n];
        }

        // STAGE0 Create Random Keys
        {
            if (ThreadType::is_main) current_block.store (0);
            t.synchronized(generate_random, n);
        }

        for (size_t i = 0; i < it; ++i)
        {

            // STAGE 0.1
            t.synchronized([cap](bool m)
                           { if (m) hash_table = HASHTYPE(cap); return 0; },
                           ThreadType::is_main);

            t.out << otm::width(5) << i
                  << otm::width(5) << t.p
                  << otm::width(11) << n
                  << otm::width(11) << cap
                  << otm::width(7) << con;

            t.synchronize();

            Handle hash = hash_table.get_handle();


            // STAGE2 n Insertions [2 .. n+1]
            {
                if (ThreadType::is_main) current_block.store(0);

                auto duration = t.synchronized(aggregate<Handle>, hash, n);

                t.out << otm::width(12) << duration.second/1000000.;
            }

            // STAGE3 n Cont Random Finds Successful
            {
                if (ThreadType::is_main) current_block.store(0);

                auto duration = t.synchronized(validate_aggregate<Handle>, hash, n);

                t.out << otm::width(12) << duration.second/1000000.
                      << otm::width(9)  << errors.load();
            }

#ifdef MALLOC_COUNT
            t.out << otm::width(16) << malloc_count_current();
#endif

            if (n - valsum.load())
                t.out << "SUM_ERROR " << n - valsum.load() << std::flush;

            t.out << std::endl;
            if (ThreadType::is_main)
            {
                valsum.store(0);
                errors.store(0);
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
    size_t cap = c.int_arg("-c" , n);
    size_t it  = c.int_arg("-it", 5);
    double con = c.double_arg("-con", 1.0);
    if (! c.report()) return 1;

    zipf_gen.initialize(n,con);

    otm::out() << otm::width(5)  << "#i"
               << otm::width(5)  << "p"
               << otm::width(11) << "n"
               << otm::width(11) << "cap"
               << otm::width(7)  << "con"
               << otm::width(12) << "t_aggreg"
               << otm::width(12) << "t_val_ag"
               << otm::width(9)  << "errors"
               << std::endl;

    ttm::start_threads<test_in_stages>(p, n, cap, it, con);


    return 0;
}
