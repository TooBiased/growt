/*******************************************************************************
 * utils/poolallocator.h
 *
 * Pool allocator using the tbb::fixed_pool allocator
 * During first initialization all memory is zeroed to force virtual to physical
 * mapping (preventing some slowdown during growing phases)
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef POOLALLOCATOR_H
#define POOLALLOCATOR_H


#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <algorithm>
#include <atomic>

#define TBB_PREVIEW_MEMORY_POOL 1
#include "tbb/memory_pool.h"
#include "tbb/tbb.h"
//#include "numa.h"
#include <iostream>

#ifdef GROWT_USE_CONFIG
#include "growt_config.h"
#else
#define GROWT_MEMPOOL_SIZE 1024ull*1024ull*1024ull*2
#endif

namespace growt {

namespace BaseAllocator
{
struct Malloc
{
    void* alloc (size_t n)
    {
        char* memory = (char*) malloc(n);
        if (!memory)
        {
            throw std::bad_alloc();
        }
        std::fill(memory, memory+n, 0);
        return memory;
    }

    void dealloc (void* ptr, size_t /*size_hint*/)
    {
        free(ptr);
    }
};


struct HugePageAlloc
{
    void* alloc (size_t n)
    {
        char* memory = (char*)mmap(NULL, n, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
        if (memory == MAP_FAILED)
        {
            throw std::bad_alloc();
        }
        std::fill(memory, memory+n, 0);
        return memory;
    }

    void dealloc (void* ptr, size_t size_hint)
    {
        munmap(ptr, size_hint);
    }
};


}

template<class  T = char, class AS = BaseAllocator::Malloc>
class BasePoolAllocator
{
private:
    static const size_t       default_pool_size;//(1024ull*1024ull*1024ull*16ull); //16GiB TODO: CHANGE BACK
    static std::atomic_size_t initialized;//(0);// 0 uninitialized 1 currently initializing 2 initialized
    static tbb::fixed_pool    *pool        ;//= nullptr;
    static char               *pool_buffer ;//= nullptr;

public:
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    //! C++11 type flag
    using is_always_equal = std::false_type;
    //! C++11 type flag
    using propagate_on_container_move_assignment = std::true_type;


    //! Return allocator for different type.
    template<class U>
    struct rebind { using other = BasePoolAllocator<U,AS>; };

    // Deletes the memory pool, and frees the used memory
    // This does not deconstruct any allocated elements,
    // therefore it should only be used after cleaning all allocations
    static void reset()
    {
        if (initialized.load() >= 2)
        {
            delete pool;
            AS().dealloc(pool_buffer, initialized.load());
            initialized.store( 0 );
        }
    }

    // can be used to explicitly construct the mempool with different sizes.
    static void init(size_t n = 0)
    {
        if (initialized.load() == 0)
        {
            size_t temp = 0;
            if (initialized.compare_exchange_strong(temp, 1))
            {
                size_t tempN = (n) ? n : default_pool_size;
                pool_buffer = (char*) AS().alloc(tempN);
                if (pool_buffer)
                {
                    pool = new tbb::fixed_pool(pool_buffer, tempN);
                    initialized.store(tempN);
                    std::atexit(reset);
                }
            }
        }

    }

    // has to create the memory pool iff it is not initialized yet
    BasePoolAllocator(size_t n = 0)
    {
        auto temp = initialized.load();

        if (temp >= 2) return;
        init(n);
        while (initialized.load() < 2) { }
    }

    BasePoolAllocator(const BasePoolAllocator&)            noexcept = default;
    template<class U>
    BasePoolAllocator(const BasePoolAllocator<U>&)         noexcept {};
    BasePoolAllocator& operator=(const BasePoolAllocator&) noexcept = default;
    ~BasePoolAllocator()                                   noexcept = default;


    //! Allocates memory for n objects of type T
    pointer allocate(size_type n, const void* /* hint */ = nullptr)
    {
        if (n > max_size())
            throw std::bad_alloc();

        void* ptr = pool->malloc(n*sizeof(T));
        return pointer(ptr);
    }

    //! Frees an allocated piece of memory
    void deallocate(pointer p, size_type /* size_hint */ = 0) noexcept
    {   pool->free(p);  }

    //! Returns the address of x.
    pointer address(reference x) const noexcept
    {   return std::addressof(x);   }

    //! Returns the address of x.
    const_pointer address(const_reference x) const noexcept
    {   return std::addressof(x);   }

    //! Maximum size possible to allocate
    size_type max_size() const noexcept
    {   return default_pool_size / sizeof(T);   }

    //! Constructs an element object on the location pointed by p.
    void construct(pointer p, const_reference value)
    {   ::new ((void*)p)T(value);   }

    //! Destroys in-place the object pointed by p.
    void destroy(pointer p) const noexcept
    {   p->~T();   }



    //! Constructs an element object on the location pointed by p.
    template <typename SubType, typename ... Args>
    void construct(SubType* p, Args&& ... args)
    {   ::new ((void*)p)SubType(std::forward<Args>(args) ...);   }

    //! Destroys in-place the object pointed by p.
    template <typename SubType>
    void destroy(SubType* p) const noexcept {
        p->~SubType();
    }

    template<class Other, class OtherAlloc>
    bool operator==(const BasePoolAllocator<Other, OtherAlloc>& other)
    {   return  std::is_same<AS, OtherAlloc>::value;   }

    template<class Other, class OtherAlloc>
    bool operator!=(const BasePoolAllocator<Other, OtherAlloc>& other)
    {   return !std::is_same<AS, OtherAlloc>::value;   }
};

// Initialization of static member variables

template<typename T, typename S>
const size_t       BasePoolAllocator<T,S>::default_pool_size = GROWT_MEMPOOL_SIZE;
template<typename T, typename S>
std::atomic_size_t BasePoolAllocator<T,S>::initialized(0);
template<typename T, typename S>
char*              BasePoolAllocator<T,S>::pool_buffer = nullptr;
template<typename T, typename S>
tbb::fixed_pool*   BasePoolAllocator<T,S>::pool        = nullptr;

template<typename T = char>
using     PoolAllocator = BasePoolAllocator<T,BaseAllocator::Malloc>;
template<typename T = char>
using HTLBPoolAllocator = BasePoolAllocator<T,BaseAllocator::HugePageAlloc>;

}

#endif // POOLALLOCATOR_H
