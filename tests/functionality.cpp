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
#include <random>

#include "utils/default_hash.hpp"
#include "utils/thread_coordination.hpp"
#include "utils/pin_thread.hpp"
#include "utils/command_line_parser.hpp"
#include "utils/output.hpp"

#include "data-structures/returnelement.hpp"

#include "example/update_fcts.hpp"

#include "tests/selection.hpp"


const static uint64_t range = (1ull << 62) -1;
namespace otm = utils_tm::out_tm;
namespace ttm = utils_tm::thread_tm;

using fun_config_simple  = table_config<size_t, size_t,
                                        utils_tm::hash_tm::default_hash,
                                        allocator_type>;
using fun_config_complex = table_config<std::string, std::atomic_size_t,
                                        utils_tm::hash_tm::default_hash,
                                        allocator_type,
                                        hmod::ref_integrity>;
using simple_table_type  = typename fun_config_simple ::table_type;
using complex_table_type = typename fun_config_complex::table_type;

alignas(64) static  simple_table_type simple_table  = simple_table_type(0);
alignas(64) static complex_table_type complex_table = complex_table_type(0);

alignas(64) static uint64_t* keys;
alignas(64) static std::atomic_size_t current_block;

alignas(64) static std::atomic_size_t errors;


template <class TType, class Func, class ... Args>
void perform_test(TType& t, const std::string& name, const std::string& description, Func&& f, Args&& ... args)
{
    if constexpr (TType::is_main) current_block.store(0, std::memory_order_relaxed);
    if constexpr (TType::is_main) errors.store(0, std::memory_order_relaxed);
    t.out << "Starting test: " << name << std::endl;
    t.out << "    " << otm::color::bblack << description << otm::color::reset << std::endl;
    t.synchronized(f, std::forward<Args>(args)...);
    t.out << "  "  << name  << " DONE"<< std::flush;
    auto err = errors.exchange(0, std::memory_order_relaxed);
    if (!err) t.out << otm::color::green << "-> SUCCESSFUL"
                    << otm::color::reset << std::endl << std::endl;
    else      t.out << otm::color::red   << "-> UNSUCCESSFUL "
                    << otm::color::reset << err << " Errors" << std::endl << std::endl;
}

struct inc_test
{
    using mapped_type = uint64_t;
    mapped_type operator()(mapped_type& mapped, bool& atomic)
    {
        atomic = false;
        return mapped++;
    }
    mapped_type atomic(mapped_type& mapped, bool& atomic)
    {
        atomic = true;
        auto temp = reinterpret_cast<std::atomic<mapped_type>*>(&mapped)->fetch_add(
            1,
            std::memory_order_relaxed);
        return temp+1;
    }
};

struct assign_test
{
    using mapped_type = uint64_t;
    mapped_type operator()(mapped_type& mapped, mapped_type input, bool& atomic)
    {
        atomic = false;
        mapped = input;
        return input;
    }
    mapped_type atomic(mapped_type& mapped, mapped_type input, bool& atomic)
    {
        atomic = true;
        reinterpret_cast<std::atomic<mapped_type>*>(&mapped)->store(
            input,
            std::memory_order_relaxed);
        return input;
    }
};

// INPUT empty
// OUTPUT full 2*n elements
template <class ThreadType, class HashType>
void insert_find_test(ThreadType& t, HashType& hash, size_t n)
{
    t.out << otm::color::bblue << "FIRST GROUP INSERTIONS" << otm::color::reset
          << std::endl;
    // STAGE 1   n Insertions successful
    perform_test(
        t, "+INSERTION", "inserting the first n elements",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, n,
                [&](size_t i)
                {
                    auto ins_ret = hash.insert(keys[i], i);
                    if (! ins_ret.second
                        || (*ins_ret.first).first  != keys[i]
                        || (*ins_ret.first).second != i)
                    {
                        err++;
                    }
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "-INSERTION", "reinserting the first n elements (insertions should fail)",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, n,
                [&](size_t i)
                {
                    auto ins_ret = hash.insert(keys[i], 666);
                    if (ins_ret.second
                        || ins_ret.first == hash.end()
                        || (*ins_ret.first).second != i)
                    {
                        err++;
                    }
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "FIND", "looking for all 2*n elements (find first half unsuccess second half)",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, n,
                [&](size_t i)
                {
                    auto it = hash.find(keys[i]);
                    if (i < n
                        && (it == hash.end()
                            || (*it).second != i))
                    { err++; }
                    if (i >= n
                        && (it != hash.end()))
                    { err++; }
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "GROWING", "insert the second n elements (triggering migration)",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, n,
                [&](size_t i)
                {
                    auto ins_ret = hash.insert(keys[i+n], i+n);
                    if (! ins_ret.second)
                    {
                        err++;
                    }
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "CHECK INS/FIND", "find (old and additional elements)",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    auto it = hash.find(keys[i]);
                    if (it == hash.end()
                        || (*it).second != i)
                    {
                        err++;
                    }
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });
}

// INPUT  full 2*n elements
// OUTPUT half full n first elements
template <class ThreadType, class HashType>
void erase_test(ThreadType& t, HashType& hash, size_t n)
{
    t.out << otm::color::bblue << "ERASE TEST" << otm::color::reset << std::endl;
    perform_test(
        t, "+ERASE", "delete elements n-3/2*n elements)",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, n/2,
                [&](size_t i)
                {
                    auto count = hash.erase(keys[n+i]);
                    if (count != 1)
                    {
                        err++;
                    }
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });


    perform_test(
        t, "-ERASE", "delete the same n/2 elements)",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, n/2,
                [&](size_t i)
                {
                    auto count = hash.erase(keys[n+i]);
                    if (count != 0)
                    {
                        err++;
                    }
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "ERASE_IF", "use erase if to remove only the last n/2 elements)",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    auto count = hash.erase_if(keys[i], (i > n) ? i : n);
                    if (i <  n*3/2 && count != 0)
                        err++;
                    if (i >= n*3/2 && count != 1)
                        err++;
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "CHECK ERASE", "find lookup all keys should find the first n but no others",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    auto it = hash.find(keys[i]);

                    if (i < n && it == hash.end())
                        err++;

                    if (i >=n && it != hash.end())
                        err++;
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });
}

// INPUT  half full n first elements
// OUTPUT half full n first elements (different data)
template <class ThreadType, class HashType>
void iterator_test(ThreadType& t, HashType& hash, [[maybe_unused]]size_t n)
{
    t.out << otm::color::bblue << "ITERATOR TEST" << otm::color::reset << std::endl;
        perform_test(
        t, "ITERATOR ACCESS", "iterate over all elements replacing their data with compare_exchange",
        [&]()
        {
            size_t err = 0;
            size_t cha = 0;
            size_t con = 0;
            size_t ele = 0;
            for (auto it = hash.begin(); it != hash.end(); ++it)
            {
                ele++;
                if ((*it).second < n)
                {
                    if ((*it).compare_exchange(n+t.id)) cha++;
                    else con++;

                }
            }
            t.out << "  encountered ";
            if (ele == n) t.out << otm::color::green;
            else          t.out << otm::color::red;
            t.out << ele << otm::color::reset
                  << " elements (expected " << n << ")" << std::endl;
            t.out << "  changed " << cha << " values" << std::endl;
            t.out << "  encountered contention " << con
                  << " times"<< std::endl;
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });
}

// INPUT  half full n first elements (weird data)
// OUTPUT full 2*n elements (i+1)
template <class ThreadType, class HashType>
void update_test(ThreadType& t, HashType& hash, size_t n)
{
    t.out << otm::color::bblue << "UPDATE TEST" << otm::color::reset << std::endl;

    perform_test(
        t, "UPDATE", "update all 2*n keys using the functor defined above (n successful n unsuccessful)",
        [&]()
        {
            size_t   err    = 0;
            bool     atomic = false;
            for (size_t i = 0; i < 2*n; ++i)
            {
                auto ins_ret = hash.update(keys[i],assign_test(),i,atomic);
                if (i <  n && !ins_ret.second) err++;
                if (i >= n &&  ins_ret.second) err++;
            }
            errors.fetch_add(err, std::memory_order_relaxed);
            t.out << "  used the " << otm::color::blue << std::flush;
            if (!atomic) t.out << "operator()";
            else t.out << "atomic";
            t.out << otm::color::reset << " interface " << std::endl;
            return 0;
        });

    perform_test(
        t, "CHECK UPDATE", "find lookup all keys check data",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    auto it = hash.find(keys[i]);

                    if (i < n && (it == hash.end() || (*it).second != i))
                        err++;

                    if (i >=n && it != hash.end())
                        err++;
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "UPDATE_UNSAFE", "update 2*n elements without contention using assign_test function above",
        [&]()
        {
            size_t   err  = 0;
            bool     atomic = false;
            ttm::execute_parallel(
                current_block, n,
                [&](size_t i)
                {
                    auto ins_ret = hash.update_unsafe(keys[i], inc_test(), atomic);
                    if (i < n  && !ins_ret.second) err++;
                    if (i >= n &&  ins_ret.second) err++;
                });

            errors.fetch_add(err, std::memory_order_relaxed);
            t.out << "  used the " << otm::color::blue << std::flush;
            if (atomic) t.out << "operator()";
            else t.out << "atomic";
            t.out << otm::color::reset << " interface " << std::endl;
            return 0;
        });

    perform_test(
        t, "CHECK UPDATE_UNSAFE", "find lookup all keys check data",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    auto it = hash.find(keys[i]);

                    if (i < n && (it == hash.end() || (*it).second != i+1))
                        err++;

                    if (i >=n && it != hash.end())
                        err++;
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "INSERT OR ASSIGN", "reinsert/update all 1.5*n elements",
        [&]()
        {
            size_t upd = 0;
            size_t ins = 0;
            ttm::execute_parallel(
                current_block, n*3/2,
                [&](size_t i)
                {
                    auto ins_ret = hash.insert_or_assign(keys[i], i);
                    if (ins_ret.second)
                        ins++;
                    else
                        upd++;
                });
            t.out << "  inserted " << ins
                  << " elements"   << std::endl;
            t.out << "  updated "  << upd
                  << " elements"   << std::endl;
            return 0;
        });

    perform_test(
        t, "INSERT OR UPDATE", "reinsert/update the first n elements",
        [&]()
        {
            size_t upd = 0;
            size_t ins = 0;
            bool   atomic = false;
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    auto ins_ret = hash.insert_or_update(keys[i], i+1, inc_test(), atomic);
                    if (ins_ret.second)
                        ins++;
                    else
                        upd++;
                });

            t.out << "  used the " << otm::color::blue << std::flush;
            if (atomic) t.out << "operator()";
            else t.out << "atomic";
            t.out << otm::color::reset << " interface " << std::endl;
            t.out << "  inserted " << ins
                  << " elements"   << std::endl;
            t.out << "  updated "  << upd
                  << " elements"   << std::endl;
            return 0;
        });

    perform_test(
        t, "CHECK INSERT_OR_UPDATE", "find lookup all keys check data",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    auto it = hash.find(keys[i]);
                    if (it == hash.end() || (*it).second != i+1)
                        err++;
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

}

// INPUT  full 2*n elements (i+1)
// OUTPUT full 2*n elements (i)
template <class ThreadType, class HashType>
void operator_test(ThreadType& t, HashType& hash, size_t n)
{
    t.out << otm::color::bblue << "OPERATOR TEST" << otm::color::reset << std::endl;
    perform_test(
        t, "+ERASE", "delete elements n elements)",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, n,
                [&](size_t i)
                {
                    auto count = hash.erase(keys[i]);
                    if (count != 1) err++;
                });
            errors.fetch_add(err, std::memory_order_relaxed);
            return 0;
        });

    perform_test(
        t, "INSERT/ASSIGN[]", "insert or update the second n elements using the []-operator",
        [&]()
        {
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    hash[keys[i]] = i;
                });
            return 0;
        });

    perform_test(
        t, "READ[]", "lookup the all elements using the []-operator",
        [&]()
        {
            size_t err = 0;
            ttm::execute_parallel(
                current_block, 2*n,
                [&](size_t i)
                {
                    auto res = hash[keys[i]];
                    if (res != i) err++;
                });
            return 0;
        });
}

// INPUT  full 2*n elements
// OUTPUT half full n first elements
template <class ThreadType, class HashType>
void range_iterator_test(ThreadType& t, HashType& hash, [[maybe_unused]]size_t n)
{
    t.out << otm::color::bblue << "RANGE ITERATOR TEST" << otm::color::reset << std::endl;
    perform_test(
        t, "RANGE", "access each element through the range iterator interface",
        [&]()
        {
            size_t min = std::numeric_limits<size_t>::max();
            size_t max = 0;
            size_t nblocks= 0;
            ttm::execute_blockwise_parallel(
                current_block, hash.capacity(),
                [&](size_t s, size_t e)
                {
                    size_t lcount = 0;
                    for (auto it = hash.range(s,e);
                         it != hash.range_end(); ++it)
                        lcount++;
                    min = std::min(min, lcount);
                    max = std::max(max, lcount);
                    nblocks++;
                });
            t.out << "  explored " << nblocks << " blocks" << std::endl;
            t.out << "  max " << max << " elements (in one block)" << std::endl;
            t.out << "  min " << min << " elements (in one block)" << std::endl;
            return 0;
        });

}

template <class ThreadType>
struct test_in_stages
{
    static int execute(ThreadType t, size_t n, size_t it)
    {

        utils_tm::pin_to_core(t.id);

        using handle_type = typename simple_table_type::handle_type;

        if (ThreadType::is_main)
        {
            keys = new uint64_t[2*n];
        }

        // STAGE 0 Create Random Keys
        {
            if (ThreadType::is_main) current_block.store (0);

            t.synchronized(
                [](size_t n)
                {
                    ttm::execute_blockwise_parallel(current_block, 2*n,
                        [](size_t s, size_t e)
                        {
                            std::mt19937_64 re(s*1239819123884ull);
                            std::uniform_int_distribution<uint64_t> dis(1,(1ull<<63)-1);
                            for (size_t i = s; i < e; i++)
                            {
                                keys[i] = dis(re);
                            }
                        });
                    return 0;
                }, n);
        }

        for (size_t i = 0; i<it; ++i)
        {
            t.synchronize();
            if constexpr (ThreadType::is_main) simple_table = simple_table_type{n};
            t.synchronize();

            handle_type hash = simple_table.get_handle();

            insert_find_test(t, hash, n);
            erase_test      (t, hash, n);
            iterator_test   (t, hash, n);
            update_test     (t, hash, n);
            operator_test   (t, hash, n);
            range_iterator_test(t, hash, n);

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
    size_t p  = c.int_arg("-p", 4);
    size_t n  = c.int_arg("-n", 100000);
    size_t it = c.int_arg("-it", 2);
    if (! c.report()) return 1;

    otm::out() << "# testing " << simple_table_type::name() << std::endl;

    ttm::start_threads<test_in_stages>(p, n, it);

    return 0;
}
