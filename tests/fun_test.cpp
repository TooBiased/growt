/*******************************************************************************
 * tests/fun_test.cpp
 *
 * functionality test for more information see below
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
#include "utils/thread_coordination.hpp"
#include "utils/pin_thread.hpp"
#include "utils/command_line_parser.hpp"
#include "utils/output.hpp"

#include "example/update_fcts.h"

#include <random>

/*
 * This Test is meant to test the functionality of each table
 * 0.  Creating n random keys
 * 0.1 Test if generated table is empty (with n random keys)
 * 1.  Inserting n elements (key, 3)
 * 1.1 Test that each inserted key has value 3
 * 2.  Try to reinsert n keys with value 4 (should be unsuccessful)
 * 2.1 Check that no value has changed to 4
 * 3.  Update all n elements to value 5
 * 3.1 Check, that each value is now 5
 * 4.  insert or increment random keys between 2 and p+1
 * 4.1 sum all values of keys between 2 and p+1 to make sure no increment got lost
 * 5.  remove n/2 elements (every second element)
 * 5.1 make sure, that removed elements cannot be found
 *
 * the expected output is a row of ones (no 0)
 */

const static uint64_t range = (1ull << 62) -1;
namespace otm = utils_tm::out_tm;
namespace ttm = utils_tm::thread_tm;

alignas(64) static HASHTYPE hash_table = HASHTYPE(0);
alignas(64) static uint64_t* keys;
alignas(64) static std::atomic_size_t current_block;
alignas(64) static std::atomic_size_t errors;

int generate_random(size_t n)
{
    std::uniform_int_distribution<uint64_t> dis(2,range);

    ttm::execute_blockwise_parallel(current_block, n,
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

int set_up_hash(bool main, size_t n)
{
    if (main)
    {
        hash_table = HASHTYPE(n);
    }

    return 0;
}

template <class Hash>
int insert(Hash& hash, size_t n, size_t val)
{
    auto err = 0u;

    ttm::execute_parallel(current_block, n,
        [&hash, &err, val](size_t i)
        {
            auto key = keys[i];
            if (hash.insert(key, val).second)
            {
                ++ err;
            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}



template <class Hash>
int update(Hash& hash, size_t n, size_t val)
{
    auto err = 0u;

    ttm::execute_parallel(current_block, n,
        [&hash, &err, val](size_t i)
        {
            auto key = keys[i];
            if (! hash.update(key, growt::example::Overwrite(),val).second) ++ err;
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int insert_or_increment(Hash& hash, size_t n, size_t p)
{
    auto err = 0u;

    size_t bitmask = 1;
    while (bitmask < p) bitmask <<= 1;
    --bitmask;

    ttm::execute_parallel(current_block, n,
        [&hash, &err, bitmask](size_t i)
        {
            auto key = bitmask &
                __builtin_ia32_crc32di(34390210450981235ull,i*12037459812355ull);
            if (hash.insert_or_update(key+2, 1, growt::example::Increment(),1).first == hash.end())
            {
                ++ err;
            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
bool val_inc(Hash& hash, size_t n, size_t p)
{
    size_t bitmask = 1;
    while (bitmask < p) bitmask <<= 1;
    --bitmask;

    size_t sum = 0;

    for (size_t i = 2; i <= bitmask+2; ++i)
    {
        sum += (*hash.find(i)).second;
    }

    return sum == n;
}



template <class Hash>
int remove(Hash& hash, size_t n)
{
    auto err = 0u;

    ttm::execute_parallel(current_block, n>>1,
        [&hash, &err](size_t i)
        {
            auto key = keys[i<<1];
            if (! hash.erase(key) )
            {
                ++err;
            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}

template <class Hash>
int val_rem(Hash& hash, size_t n)
{
    auto err = 0u;

    ttm::execute_parallel(current_block, n,
        [&hash, &err](size_t i)
        {
            auto key  = keys[i];
            auto data = hash.find(key);
            if ( !(i & 1) && (data != hash.end()) ) ++err;
            if (  (i & 1) && (data == hash.end()) ) ++err;
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int find(Hash& hash, size_t n, size_t val)
{
    auto err = 0u;

    ttm::execute_parallel(current_block, n,
        [&hash, &err, val](size_t i)
        {
            auto key = keys[i];

            if ((*hash.find(key)).second != val)
            {
                ++err;
            }
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}

void check_errors(size_t exp)
{
    otm::out() << otm::width(7) << (errors.load() == exp);
    errors.store(0);
    return;
}

template <class ThreadType>
struct test_in_stages
{
    static int test_in_stages(ThreadType t, size_t n, size_t it)
    {

        utils_tm::pin_to_core(id);

        using Handle = HASHTYPE::Handle;

        if (ThreadType::is_main)
        {
            keys = new uint64_t[n];
        }

        // STAGE 0 Create Random Keys
        {
            if (ThreadType::is_main) current_block.store (0);

            t.synchronized(generate_random, ++stage, p-1, n);
        }

        for (size_t i = 0; i<it; ++i)
        {
            // STAGE 0.1
            t.synchronized(set_up_hash, ++stage, p-1, ThreadType::is_main, n);

            // Needed for synchronization (main thread has finished set_up_hash)
            t.synchronize();

            Handle hash = hash_table.get_handle();
            //Handle hash(hash_table); //= Handle(*ht);//ht->getHandle(glob_registration);

            // STAGE 0.1 still empty
            {
                if (ThreadType::is_main) current_block.store(0);
                if (ThreadType::is_main) errors.store (0);

                t.synchronized(find<Handle>, hash, n, 0);

                if (ThreadType::is_main) check_errors(0);
            }

            // STAGE 1   n Insertions successful
            {
                if (ThreadType::is_main) current_block.store(0);
                if (ThreadType::is_main) errors.store (0);

                t.synchronized(insert<Handle>, hash, n, 3);
            }

            {
                if (ThreadType::is_main) current_block.store(0);

                t.synchronized(find<Handle>, hash, n, 3);

                if (ThreadType::is_main) check_errors(0);
            }

            // STAGE 2   n Insertions unsuccessful
            {
                if (ThreadType::is_main) current_block.store(0);
                if (ThreadType::is_main) errors.store (0);

                t.synchronized(insert<Handle>, hash, n, 4);

                if (ThreadType::is_main && errors.load() == n) errors.store(0);
            }

            // STAGE 2.1 validate (find) (secretly update?)
            {
                if (ThreadType::is_main) current_block.store(0);

                t.synchronized(find<Handle>,
                                         hash, n, 3);

                if (ThreadType::is_main) check_errors(0);
            }

            // STAGE 3   n updates
            {
                if (ThreadType::is_main) current_block.store(0);
                if (ThreadType::is_main) errors.store (0);

                t.synchronized(update<Handle>, hash, n, 5);

                if (ThreadType::is_main) check_errors(0);
            }

            // STAGE 3.1 validate
            {
                if (ThreadType::is_main) current_block.store(0);

                t.synchronized(find<Handle>, hash, n, 5);

                if (ThreadType::is_main) check_errors(0);
            }

            // STAGE 4   insert_or_increment
            {
                if (ThreadType::is_main) current_block.store(0);
                if (ThreadType::is_main) errors.store (0);

                t.synchronized(insert_or_increment<Handle>, hash, n, p);
            }

            // STAGE 4.1 validate
            {
                if (ThreadType::is_main) t.out << otm::width(7)
                                               << val_inc(hash, n, p);

                if (ThreadType::is_main) check_errors(0);
            }

            // STAGE 5   n/2 remove
            {
                //ThreadType::out (5, 3);
                if (ThreadType::is_main) current_block.store(0);

                t.synchronized(remove<Handle>, hash, n);
            }

            // STAGE 5.1 validate remove
            {
                //ThreadType::out (5.1, 3);
                if (ThreadType::is_main) current_block.store(0);

                t.synchronized(val_rem<Handle>, hash, n);

                if (ThreadType::is_main) check_errors(0);
            }

            ThreadType() << std::endl;
        }

        if (ThreadType::is_main)
        {
            delete[] keys;
            reset_stages();
        }

        return 0;
    }

};

int main(int argn, char** argc)
{
    utils_tm::command_line_parser c{argn, argc};
    size_t p  = c.int_arg("-p", 4);
    size_t n  = c.int_arg("-n", 1000000);
    size_t it = c.int_arg("-it", 2);
    if (! c.report()) return 1;

    ttm::start_threads<test_in_stages>(p, n, it);

    return 0;
}
