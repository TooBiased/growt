#include <atomic>
#include <random>
#include <thread>

#include "../allocator/alignedallocator.hpp"
#include "../data-structures/hash_table_mods.hpp"
#include "../utils/hash/murmur2_hash.hpp"

using hasher_type    = utils_tm::hash_tm::murmur2_hash;
using allocator_type = growt::AlignedAllocator<>;

//////////////////////////////////////////////////////////////
// USING definitions.h (possibly slower compilation)
#include "../data-structures/table_config.hpp"

using table_type =
    typename growt::table_config<size_t, size_t, hasher_type, allocator_type,
                                 hmod::growable, hmod::deletion>::table_type;
// check hash_table_mods.hpp for other possiblen hash table modificators
// e.g. hmod::circular_map     this config choses the appropriate hash table
// according to your needs.

// insert all keys between 1 and n into table with <key=i, data=i>
// print message if insert is not successful
void insertions(table_type& table, size_t n)
{
    // obtain a handle
    auto handle = table.get_handle();

    for (size_t i = 1; i <= n; ++i)
    {
        if (!handle.insert(i, i).second)
        {
            std::cout << "unsuccessful insert on key " << i << std::endl;
        }
    }
}

// repeatedly checks "table", if n was inserted (as a key) prints n and stops
void wait_for_k(table_type& table, size_t k)
{
    // obtain a handle (alternative)
    table_type::handle_type handle{table};

    while (handle.find(k) == handle.end()) { ; }

    std::cout << "found key " << k << std::endl;
}

// randomly looks for keys between 1 and n
// after n tries it prints the success rate and the average data height
void search_n_and_mean(table_type& table, size_t n)
{
    auto handle = table.get_handle();

    size_t      count = 0;
    size_t      sum   = 0;
    hasher_type randomizer{};

    for (size_t i = 0; i < n; ++i)
    {
        size_t key  = (randomizer(i) % n) + 1;
        auto   temp = handle.find(key);

        if (temp != handle.end())
        {
            ++count;
            sum += (*temp).second;
        }
    }

    std::cout << "found " << count << " elements "
              << "with a mean of " << (double(sum) / double(count))
              << std::endl;
}

// increases every second inserted number by 42
// this keeps repeating updates until one was successful (waits till key was
// inserted)
void update_every_scnd(table_type& table, size_t n)
{
    auto handle = table.get_handle();

    size_t unsuccessful_updates = 0;
    for (size_t i = 1; i < n; i += 2)
    {
        auto ret = handle.update(
            i, [](size_t& lhs, size_t rhs) { return lhs = lhs + rhs; }, 42);
        if (!ret.second)
        {
            ++unsuccessful_updates;
            i -= 2;
        }
    }
}

// check update
void check_update(table_type& table, size_t n)
{
    auto handle = table.get_handle();

    for (size_t i = 1; i < n; ++i)
    {
        auto temp = handle.find(i);
        if (temp != handle.end())
        {
            if (i & 1) // was updated!
            {
                if ((*temp).second != i + 42)
                {
                    std::cout << "Unexpected data at key " << i << " found "
                              << (*temp).second << std::endl;
                }
            }
            else // not updated!
            {
                if ((*temp).second != i)
                {
                    std::cout << "Unexpected data at key " << i << " found "
                              << (*temp).second << std::endl;
                }
            }
        }
        else
        {
            std::cout << "Key " << i << " not found!" << std::endl;
        }
    }
}

void check_function_compile(table_type& table)
{
    auto handle = table.get_handle();
    if (handle[11] != 53)
        std::cout << "[] operator returns " << handle[11] << "expected 53!"
                  << std::endl;
    // else std::cout << "[] operator works" << std::endl;

    /*
      begin
      end
      cbegin
      cend

      erase

      update(_unsafe)
      insert_or_update(_unsafe)

      element_count_approx

      range
      crange
      range_end
      range_cend

      capacity
     */
}

int main(int, char**)
{

    // set the capacity such that the table has to grow by a factor of 10
    size_t     n   = 1000000;
    size_t     cap = 100000;
    table_type hashTable(cap);

    std::thread in_thread(insertions, std::ref(hashTable), n);
    std::thread wait_thread(wait_for_k, std::ref(hashTable), n);
    std::thread up_thread(update_every_scnd, std::ref(hashTable), n);
    std::thread find_thread(search_n_and_mean, std::ref(hashTable), n);
    in_thread.join();
    wait_thread.join();
    up_thread.join();
    find_thread.join();
    std::cout << "First Round Done!" << std::endl;


    std::thread rfind_thread(search_n_and_mean, std::ref(hashTable), n);
    std::thread check_thread(check_update, std::ref(hashTable), n);
    rfind_thread.join();
    check_thread.join();

    check_function_compile(hashTable);
    std::cout << "Second Round Done|" << std::endl;

    return 0;
}
