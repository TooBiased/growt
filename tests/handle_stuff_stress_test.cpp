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

#include "utils/default_hash.hpp"
#include "utils/thread_coordination.hpp"
#include "utils/pin_thread.hpp"
#include "utils/command_line_parser.hpp"
#include "utils/output.hpp"

#include <random>
#include <iostream>

using Handle = typename HASHTYPE::Handle;
const static uint64_t range = (1ull << 63) - 1;
namespace otm = utils_tm::out_tm;
namespace ttm = utils_tm::thread_tm;

alignas(64) static HASHTYPE hash_table = HASHTYPE(0);
alignas(64) static std::atomic_size_t current;
alignas(64) static std::atomic_size_t unfinished;


template<class Hash>
size_t prefill(Hash& hash, size_t thread_id, size_t n)
{
    auto err = 0u;

    std::uniform_int_distribution<uint64_t> dis(2,range);
    std::mt19937_64 re(thread_id*10293903128401092ull);

    ttm::execute_parallel(current, n,
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
    Handle h = hash_table.get_handle();
    while (unfinished.load(std::memory_order_acquire))
    {
        Handle g = hash_table.get_handle();
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

    Handle h = hash_table.get_handle();
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
    otm::out() << "press button to stop test!" << std::endl;
    std::cin.ignore();
    unfinished.store(0, std::memory_order_release);
    return 0;
}

template <class ThreadType>
struct test
{
    static int execute(ThreadType t)
    {
        pin_to_core(t.id);

        if (ThreadType::is_main)
        {
            current.store(0);
            unfinished.store(1);
        }

        t.synchronized(
            [] (bool m) { if (m) hash_table = HASHTYPE(10000000); return 0; },
            ThreadType::is_main
            );
        t.synchronize();

        Handle hash = hash_table.get_handle();


        t.out << "begin prefill!" << std::endl;

        t.synchronized(prefill<Handle>, hash, id, 5000000);

        t.out << "start main test!" << std::endl;

        if (t.id == 0)
        {
            close_thread();
        }
        else if (t.id & 1)
        {
            many_moving_handles();
        }
        else
        {
            calling_size_repeatedly();
        }
        t.out << "end of test!" << std::endl;

        return 0;
    }
};

int main(int argn, char** argc)
{
    utils_tm::command_line_parser c{argn, argc};
    size_t p   = c.int_arg("-p" , 4);
    if (! c.report()) return 1;

    ttm::start_threads<test>(p);

    return 0;
}
