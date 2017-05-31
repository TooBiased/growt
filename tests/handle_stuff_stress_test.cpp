/*******************************************************************************
 * example/stress_test_weird.cpp
 *
 * This test stress tests some uncommon and more critical functions like
 * move constructors and element_counts.
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

using Handle = typename HASHTYPE::Handle;
const static uint64_t range = (1ull << 63) - 1;

alignas(64) static HASHTYPE hash_table = HASHTYPE(0);
alignas(64) static std::atomic_size_t current;
alignas(64) static std::atomic_size_t unfinished;

template<typename T>
inline void out(T t, size_t space)
{
    std::cout.width(space);
    std::cout << t << " " << std::flush;
}

template<class Hash>
size_t prefill(Hash& hash, size_t thread_id, size_t n)
{
    auto err = 0u;

    std::uniform_int_distribution<uint64_t> dis(2,range);
    std::mt19937_64 re(thread_id*10293903128401092ull);

    execute_parallel(current, n,
        [&hash, &err, &dis, &re](size_t)
        {
            auto key = dis(re);
            if (! hash.insert(key, 3).second)
            {
                // Insertion failed? Possibly already inserted.
                ++err;
            }
        });

    return err;
}


size_t many_moving_handles()
{
    std::uniform_int_distribution<uint64_t> dis(2, range);
    std::mt19937_64 re(rand());

    size_t j = 0;
    Handle h = hash_table.getHandle();
    while (unfinished.load(std::memory_order_acquire))
    {
        Handle g = hash_table.getHandle();
        for (size_t i=0; i<32; ++i)
        {
            g.insert(dis(re), 5);
        }
        h = std::move(g);
        ++j;
    }
    return j;
}

size_t calling_size_repeatedly()
{
    std::uniform_int_distribution<uint64_t> dis(2, range);
    std::mt19937_64 re(rand());

    Handle h = hash_table.getHandle();
    size_t j = 0;
    while (unfinished.load(std::memory_order_acquire))
    {
        for(size_t i=0; i<32; ++i)
        {
            h.insert(dis(re), 6);
        }

        ++j;
    }
    return j;
}

size_t close_thread()
{
    std::cout << "press button to stop test!" << std::endl;
    std::cin.ignore();
    unfinished.store(0, std::memory_order_release);
    return 0;
}

template <class ThreadType>
int test(size_t p, size_t id)
{
    pin_to_core(id);
    size_t stage = 0;

    if (ThreadType::is_main)
    {
        current.store(0);
        unfinished.store(1);
    }

    ThreadType::synchronized(
        [] (bool m) { if (m) hash_table = HASHTYPE(10000000); return 0; },
        ++stage, p-1, ThreadType::is_main
        );
    ThreadType::synchronized([](){ return 0; }, ++stage, p-1);

    Handle hash = hash_table.getHandle();


    ThreadType() << "begin prefill!" << std::endl;

    ThreadType::synchronized(prefill<Handle>,
                             ++stage, p-1, hash, id, 5000000);

    ThreadType() << "start main test!" << std::endl;

    if (id == 0)
    {
        close_thread();
    }
    else if (id & 1)
    {
        many_moving_handles();
    }
    else
    {
        calling_size_repeatedly();
    }
    ThreadType() << "end of test!" << std::endl;

    return 0;
}

int main(int argn, char** argc)
{
    CommandLine c{argn, argc};
    //size_t n   = c.intArg("-n" , 10000000);
    size_t p   = c.intArg("-p" , 4);
    //size_t cap = c.intArg("-c" , n);
    //size_t it  = c.intArg("-it", 5);
    if (! c.report()) return 1;

    start_threads(test<TimedMainThread>,
                  test<UnTimedSubThread>,
                  p);
    return 0;
}
