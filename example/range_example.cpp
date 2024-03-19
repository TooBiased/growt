#include <atomic>
#include <cmath>
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



static std::atomic_size_t aggregator_static{0};
static std::atomic_size_t aggregator_dynamic{0};


void insertions(table_type& table, size_t id, size_t n)
{
    // obtain a handle
    auto handle = table.get_handle();

    auto start = 1 + (id * n); // do not insert 0!
    auto end   = (1 + id) * n;

    for (size_t i = start; i <= end; ++i)
    {
        if (!handle.insert(i, i).second)
        {
            std::cout << "unsuccessful insert on key " << i << std::endl;
        }
    }
}


// parallely computes sum over all elements (statically distributed workload)
void static_load(table_type& table, size_t id, size_t p)
{
    // obtain a handle
    auto handle = table.get_handle();

    size_t work_load = std::ceil(double(handle.capacity()) / double(p));
    // the last range might be smaller (iff p does not divide capacity evenly)
    auto static_range_it = handle.range(id * work_load, (id + 1) * work_load);

    size_t temp = 0;

    for (; static_range_it != handle.range_end(); ++static_range_it)
    {
        temp += (*static_range_it).second;
    }

    aggregator_static.fetch_add(temp);
}


// parallely computes sum over all elements (dynamically distributed workload)
void dynamic_blockwise_load(table_type& table, size_t block_size)
{
    // obtain a handle
    auto handle = table.get_handle();

    size_t capacity = handle.capacity();

    static std::atomic_size_t work_counter{0};
    size_t                    curr_block = work_counter.fetch_add(block_size);

    size_t temp = 0;
    while (curr_block < capacity)
    {
        auto block_range_it = handle.range(curr_block, curr_block + block_size);
        // this is save, even if curr_block + block_size > capacity

        for (; block_range_it != handle.range_end(); ++block_range_it)
        {
            temp += (*block_range_it).second;
        }
        curr_block = work_counter.fetch_add(block_size);
    }

    aggregator_dynamic.fetch_add(temp);
}





int main(int, char**)
{
    size_t     n   = 1000000;
    size_t     cap = 100000;
    table_type table(cap);

    size_t temp = 0;
    for (size_t i = 1; i <= n * 4; ++i) temp += i;

    std::cout << "expected result        - " << temp << std::endl;

    std::cout << "insertions             - " << std::flush;
    std::thread t0(insertions, std::ref(table), 0, n);
    std::thread t1(insertions, std::ref(table), 1, n);
    std::thread t2(insertions, std::ref(table), 2, n);
    std::thread t3(insertions, std::ref(table), 3, n);
    t0.join();
    t1.join();
    t2.join();
    t3.join();
    std::cout << "done" << std::endl;


    std::cout << "static_load            - " << std::flush;
    std::thread s0(static_load, std::ref(table), 0, 4);
    std::thread s1(static_load, std::ref(table), 1, 4);
    std::thread s2(static_load, std::ref(table), 2, 4);
    std::thread s3(static_load, std::ref(table), 3, 4);
    s0.join();
    s1.join();
    s2.join();
    s3.join();
    std::cout << aggregator_static.load() << std::endl;


    std::cout << "dynamic_blockwise_load - " << std::flush;
    std::thread d0(dynamic_blockwise_load, std::ref(table), 4096);
    std::thread d1(dynamic_blockwise_load, std::ref(table), 4096);
    std::thread d2(dynamic_blockwise_load, std::ref(table), 4096);
    std::thread d3(dynamic_blockwise_load, std::ref(table), 4096);
    d0.join();
    d1.join();
    d2.join();
    d3.join();
    std::cout << aggregator_dynamic.load() << std::endl;

    return 0;
}
