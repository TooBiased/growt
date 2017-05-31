/*******************************************************************************
 * tests/co_test.cpp
 *
 * basic contention test for more information see below
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include "tests/selection.h"
#include "data-structures/returnelement.h"

#include "utils/hashfct.h"
#include "utils/keygen.h"
#include "utils/test_coordination.h"
#include "utils/thread_basics.h"
#include "utils/commandline.h"

#include "example/update_fcts.h"

#include <random>

/*
 * This Test is meant to test the tables performance on uniform random inputs.
 * 0. Creating n random keys with zipf distribution
 * 1. Inserting n elements [1..n] (key, key)
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
    out("con"     , 5);
    out("t_ins_or", 10);
    out("t_find_c", 10);
    out("t_updt_c", 10);
    out("t_val_up", 10);
    out("errors"  , 9);
    std::cout << std::endl;
}

void print_parameters(size_t i, size_t p, size_t n, size_t cap, double con)
{
    out (i  , 3);
    out (p  , 3);
    out (n  , 9);
    out (cap, 9);
    out (con, 5);
}

int generate_random(size_t n, double con)
{
    std::uniform_real_distribution<double> prob(0.,1.);

    execute_blockwise_parallel(current_block, n,
        [n, con, &prob](size_t s, size_t e)
        {
            std::mt19937_64 re(s*10293903128401092ull);

            for (size_t i = s; i < e; i++)
            {
                keys[i] = zipf(n, con, prob(re))+2;
            }
        });

    return 0;
}

template <class Hash>
int fill(Hash& hash, size_t n)
{
    auto err = 0u;

    execute_parallel(current_block, n,
        [&hash, &err](size_t i)
        {
            if (! hash.insert(i+2, i+2).second) ++err;
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int find_contended(Hash& hash, size_t n)
{
    auto err = 0u;

    execute_parallel(current_block, n,
        [&hash, &err](size_t i)
        {
            auto key = keys[i];

            auto data = hash.find(key);

            if (data == hash.end() || (*data).second != key)
            {
                ++err;
            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);


    return 0;
}

template <class Hash>
int update_contended(Hash& hash, size_t n)
{
    auto err = 0u;

    execute_parallel(current_block, n,
        [&hash, &err](size_t i)
        {
            auto key = keys[i];

            if (! hash.update(key, growt::example::Overwrite(), i+2).second) ++err;
        });

    errors.fetch_add(err, std::memory_order_relaxed);

    return 0;
}


template <class Hash>
int val_update(Hash& hash, size_t n)
{
    auto err = 0u;

    execute_parallel(current_block, n,
        [&hash, &err, n](size_t i)
        {
            auto data = hash.find(i+2);
            if      (data == hash.end())
            {   ++ err;   }
            else
            {
                auto temp = (*data).second;
                if (temp != i+2)
                {
                    if (temp < 2   ||
                        temp > n+1 ||
                        keys[temp-2] != i+2) ++err;
                }
            }
        });

    // std::cout << " " << err << std::flush;
    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class ThreadType>
int test_in_stages(size_t p, size_t id, size_t n, size_t cap, size_t it, double con)
{

    pin_to_core(id);

    using Handle = typename HASHTYPE::Handle;

    size_t stage = 0;

    if (ThreadType::is_main)
    {
        keys = new uint64_t[n];
    }

    // STAGE0 Create Random Keys
    {
        if (ThreadType::is_main) current_block.store (0);
        ThreadType::synchronized(generate_random, ++stage, p-1, n, con);
    }

    for (size_t i = 0; i<it; ++i)
    {
        // STAGE 0.1
        ThreadType::synchronized([cap](bool m)
                                 { if (m) hash_table = HASHTYPE(cap); return 0; },
                                 ++stage, p-1, ThreadType::is_main);

        if (ThreadType::is_main) print_parameters(i, p, n, cap, con);

        // Needed for synchronization (main thread has finished set_up_hash)
        ThreadType::synchronized([]{ return 0; }, ++stage, p-1);

        Handle hash = hash_table.getHandle();


        // STAGE2 n Insertions [2 .. n+1]
        {
            if (ThreadType::is_main) current_block.store(0);

            auto duration = ThreadType::synchronized(fill<Handle>,
                                                     ++stage, p-1, hash, n);

            ThreadType::out (duration.second/1000000., 10);
        }

        // STAGE3 n Cont Random Finds Successful
        {
            if (ThreadType::is_main) current_block.store(0);

            auto duration = ThreadType::synchronized(find_contended<Handle>,
                                                     ++stage, p-1, hash, n);

            ThreadType::out (duration.second/1000000., 10);
        }

        // STAGE4 n Cont Random Updates
        {
            if (ThreadType::is_main) current_block.store(0);

            auto duration = ThreadType::synchronized(update_contended<Handle>,
                                                     ++stage, p-1, hash, n);


            ThreadType::out (duration.second/1000000., 10);
        }

        // STAGE5 Validation of Hash Table Contents
        {
            if (ThreadType::is_main) current_block.store(0);

            auto duration = ThreadType::synchronized(val_update<Handle>,
                                                     ++stage, p-1, hash, n);


            ThreadType::out (duration.second/1000000., 10);
            ThreadType::out (errors.load(), 9);
        }

        ThreadType() << std::endl;
        if (ThreadType::is_main) errors.store(0);
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
    size_t n   = c.intArg("-n", 10000000);
    size_t p   = c.intArg("-p", 4);
    size_t cap = c.intArg("-c", n);
    size_t it  = c.intArg("-it", 5);
    double con = c.doubleArg("-con", 1.0);
    if (! c.report()) return 1;

    print_parameter_list();

    start_threads(test_in_stages<TimedMainThread>,
                  test_in_stages<UnTimedSubThread>,
                  p, n, cap, it, con);

    return 0;
}
