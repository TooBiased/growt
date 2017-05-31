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

#include "utils/hashfct.h"
#include "utils/test_coordination.h"
#include "utils/thread_basics.h"
#include "utils/commandline.h"

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
    out("#p"      , 3);
    out("n"       , 9);
    out("cap"     , 9);
    out("t_ins"   , 10);
    out("t_find_-", 10);
    out("t_find_+", 10);
    out("errors"  , 7);
    std::cout << std::endl;
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

    execute_parallel(current_block, n,
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

    execute_parallel(current_block, n,
        [&hash, &err, val](size_t i)
        {
            auto key = keys[i];
            if (! hash.update(key, growt::example::Overwrite(),val).second) ++ err;
        });

    errors.fetch_add(err, std::memory_order_relaxed);
    return 0;
}


template <class Hash>
int insertOrIncrement(Hash& hash, size_t n, size_t p)
{
    auto err = 0u;

    size_t bitmask = 1;
    while (bitmask < p) bitmask <<= 1;
    --bitmask;

    execute_parallel(current_block, n,
        [&hash, &err, bitmask](size_t i)
        {
            auto key = bitmask &
                __builtin_ia32_crc32di(34390210450981235ull,i*12037459812355ull);
            if (hash.insertOrUpdate(key+2, 1, growt::example::Increment(),1).first == hash.end())
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

    execute_parallel(current_block, n>>1,
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

    execute_parallel(current_block, n,
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

    execute_parallel(current_block, n,
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
    out ((errors.load() == exp), 5);
    errors.store(0);
    return;
}


template <class ThreadType>
int test_in_stages(size_t p, size_t id, size_t n, size_t it)
{

    pin_to_core(id);

    using Handle = HASHTYPE::Handle;

    size_t stage = 0;

    if (ThreadType::is_main)
    {
        keys = new uint64_t[n];
    }

    // STAGE 0 Create Random Keys
    {
        if (ThreadType::is_main) current_block.store (0);

        ThreadType::synchronized(generate_random, ++stage, p-1, n);
    }

    for (size_t i = 0; i<it; ++i)
    {
        // STAGE 0.1
        ThreadType::synchronized(set_up_hash, ++stage, p-1, ThreadType::is_main, n);

        // Needed for synchronization (main thread has finished set_up_hash)
        ThreadType::synchronized([]{ return 0; }, ++stage, p-1);

        Handle hash = hash_table.getHandle();
        //Handle hash(hash_table); //= Handle(*ht);//ht->getHandle(glob_registration);

        // STAGE 0.1 still empty
        {
            if (ThreadType::is_main) current_block.store(0);
            if (ThreadType::is_main) errors.store (0);

            ThreadType::synchronized(find<Handle>,
                                     ++stage, p-1, hash, n, 0);

            if (ThreadType::is_main) check_errors(0);
        }

        // STAGE 1   n Insertions successful
        {
            if (ThreadType::is_main) current_block.store(0);
            if (ThreadType::is_main) errors.store (0);

            ThreadType::synchronized(insert<Handle>,
                                     ++stage, p-1, hash, n, 3);
        }

        {
            if (ThreadType::is_main) current_block.store(0);

            ThreadType::synchronized(find<Handle>,
                                     ++stage, p-1, hash, n, 3);

            if (ThreadType::is_main) check_errors(0);
        }

        // STAGE 2   n Insertions unsuccessful
        {
            if (ThreadType::is_main) current_block.store(0);
            if (ThreadType::is_main) errors.store (0);

            ThreadType::synchronized(insert<Handle>,
                                     ++stage, p-1, hash, n, 4);

            if (ThreadType::is_main && errors.load() == n) errors.store(0);
        }

        // STAGE 2.1 validate (find) (secretly update?)
        {
            if (ThreadType::is_main) current_block.store(0);

            ThreadType::synchronized(find<Handle>,
                                     ++stage, p-1, hash, n, 3);

            if (ThreadType::is_main) check_errors(0);
        }

        // STAGE 3   n updates
        {
            if (ThreadType::is_main) current_block.store(0);
            if (ThreadType::is_main) errors.store (0);

            ThreadType::synchronized(update<Handle>,
                                     ++stage, p-1, hash, n, 5);

            if (ThreadType::is_main) check_errors(0);
        }

        // STAGE 3.1 validate
        {
            if (ThreadType::is_main) current_block.store(0);

            ThreadType::synchronized(find<Handle>,
                                     ++stage, p-1, hash, n, 5);

            if (ThreadType::is_main) check_errors(0);
        }

        // STAGE 4   insertOrIncrement
        {
            if (ThreadType::is_main) current_block.store(0);
            if (ThreadType::is_main) errors.store (0);

            ThreadType::synchronized(insertOrIncrement<Handle>,
                                     ++stage, p-1, hash, n, p);
        }

        // STAGE 4.1 validate
        {
            if (ThreadType::is_main) ThreadType::out(val_inc(hash, n, p), 5);

            if (ThreadType::is_main) check_errors(0);
        }

        // STAGE 5   n/2 remove
        {
            //ThreadType::out (5, 3);
            if (ThreadType::is_main) current_block.store(0);

            ThreadType::synchronized(remove<Handle>,
                                     ++stage, p-1, hash, n);
        }

        // STAGE 5.1 validate remove
        {
            //ThreadType::out (5.1, 3);
            if (ThreadType::is_main) current_block.store(0);

            ThreadType::synchronized(val_rem<Handle>,
                                     ++stage, p-1, hash, n);

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


int main(int argn, char** argc)
{
    CommandLine c{argn, argc};
    size_t p  = c.intArg("-p", 4);
    size_t n  = c.intArg("-n", 1000000);
    size_t it = c.intArg("-it", 2);
    if (! c.report()) return 1;

    start_threads(
        test_in_stages<UnTimedMainThread>,
        test_in_stages<UnTimedSubThread>,
        p, n, it);

    return 0;
}
