/*******************************************************************************
 * tests/in_test.cpp
 *
 * basic insert and find test for more information see below
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/
#include <iostream>
#include <fstream>
#include <string>
#include <atomic>

#include "tbb/scalable_allocator.h"

#ifdef MALLOC_COUNT
#include "malloc_count.h"
#endif

#include "utils/default_hash.hpp"
#include "utils/thread_coordination.hpp"
#include "utils/pin_thread.hpp"
#include "utils/command_line_parser.hpp"
#include "utils/output.hpp"
#include "utils/debug.hpp"

#include "tests/selection.hpp"

#include "example/update_fcts.hpp"

/*
 * This Test is meant to test the tables performance on uniform random inputs.
 * 0. Creating 2n random keys
 * 1. Inserting n elements (key, index) - the index can be used for validation
 * 2. Looking for n elements - using different keys (likely not finding any)
 * 3. Looking for the n inserted elements (hopefully finding all)
 *    (correctness test using the index)
 */

namespace otm = utils_tm::out_tm;
namespace ttm = utils_tm::thread_tm;
namespace dtm = utils_tm::debug_tm;

using scalable_string = std::basic_string<char,
                                          std::char_traits<char>,
                                          tbb::scalable_allocator<char>>;
using text_config = table_config<scalable_string, size_t,
                                utils_tm::hash_tm::default_hash,allocator_type>;
using table_type = typename text_config::table_type;


alignas(64) static table_type hash_table = table_type(0);
//alignas(64) static std::atomic_size_t errors;
alignas(64) static std::atomic_size_t pos{0};
alignas(64) static std::atomic_size_t number_words{0};
alignas(64) static std::atomic_size_t unique_words{0};


template <class Hash>
int push_file(Hash& hash, std::ifstream& in_stream, [[maybe_unused]]size_t id)
{
    //auto err = 0u;
    static constexpr size_t block_size = 100000;
    auto uniques = 0u;
    auto words   = 0u;
    while (!in_stream.eof())
    {
        long long start = pos.fetch_add(block_size, std::memory_order_acquire);
        long long end   = start + block_size;
        scalable_string word;

        //auto lout = otm::locally_buffered_output<otm::output_type>(otm::out());
        if (start > 0)
        {
            in_stream.seekg(start);
            in_stream >> word;
        }
        else
            in_stream.seekg(0);

        while (!in_stream.eof() && in_stream.tellg() <= end)
        {

            if (in_stream.eof())
            { break; }

            in_stream >> word;
            words++;

            #define VARIANT 1

            if (VARIANT == 1)
            {
                auto result = hash.emplace_or_update(
                    std::move(word), 1, growt::example::Increment(), 1);
                if (result.second) uniques++;
            }
            if (VARIANT == 2)
            {
                auto result = hash.insert_or_update(
                    word, 1, growt::example::Increment(), 1);
                if (result.second) uniques++;
            }
            // if (VARIANT == 3)
            // {
            //     auto result = hash.insert(word, 1);
            //     if (result.second) uniques++;
            //     else result.first->second++;
            // }
            // if (VARIANT == 4)
            // {
            //     auto result = hash.emplace(std::move(word), 1);
            //     if (result.second) uniques++;
            //     else result.first->second++;
            // }
            if (VARIANT == 5)
            {
                auto result = hash.emplace(std::move(word), 1);
                if (result.second) uniques++;
            }
            if (VARIANT == 6)
            {
                auto result = hash.insert(word, 1);
                if (result.second) uniques++;
            }
            if (VARIANT == 7)
            {
                auto result = hash.insert(word, 1);
                if (result.second) uniques++;
                if (!result.second)
                {
                    hash.update(word, growt::example::Increment(),1);
                }
            }
        }
    }
    number_words.fetch_add(words,   std::memory_order_relaxed);
    unique_words.fetch_add(uniques, std::memory_order_relaxed);
    return 0;
}

template <class ThreadType>
struct test_in_stages
{
    static int execute(ThreadType t, size_t cap, size_t it, std::string file)
    {
        utils_tm::pin_to_core(t.id);

        for (size_t i = 0; i < it; ++i)
        {
            // STAGE 0.1
            t.synchronized(
                [cap] (bool m) { if (m) hash_table = table_type(cap); return 0; },
                ThreadType::is_main);

            std::ifstream in_file(file);
            if (! in_file.is_open())
            {
                t.out << "Error: on opening input file" << std::endl;
                return 0;
            }
            t.out << otm::width(5) << i
                  << otm::width(5) << t.p
                  << otm::width(11) << cap << std::flush;

            t.synchronize();



            using handle_type = typename table_type::handle_type;
            handle_type hash = hash_table.get_handle();

            // STAGE2 n Insertions
            {
                auto duration = t.synchronized(push_file<handle_type>, hash, in_file, t.id);

                t.out << otm::width(12) << duration.second/1000000.;
                t.out << otm::width(13)  << number_words.load();
                t.out << otm::width(13)  << unique_words.load() << std::flush;
            }

#ifdef MALLOC_COUNT
            t.out << otm::width(16) << malloc_count_current();
#endif
            t.out << std::endl;
            if constexpr (t.is_main)
            {
                pos         .store(0);
                unique_words.store(0);
                number_words.store(0);
            }

            in_file.close();

            // Some Synchronization
            t.synchronize();
        }

        return 0;
    }
};



int main(int argn, char** argc)
{
    utils_tm::command_line_parser c{argn, argc};
    size_t p   = c.int_arg("-p" , 1);
    size_t cap = c.int_arg("-c" , 50000);
    size_t it  = c.int_arg("-it", 5);
    std::string in_file_name = c.str_arg("-file", "input");
    if (! c.report()) return 1;

    otm::out() << otm::width(5)  << "#i"
               << otm::width(5)  << "p"
               << otm::width(11) << "cap"
               << otm::width(12) << "time"
               << otm::width(9)  << "words"
               << otm::width(9)  << "uniques"
               << "    " << text_config::name()
               << std::endl;

    ttm::start_threads<test_in_stages>(p, cap, it, in_file_name);
    return 0;
}
