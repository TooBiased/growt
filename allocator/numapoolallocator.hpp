#ifndef NUMAPOOLALLOCATOR_H
#define NUMAPOOLALLOCATOR_H

#include "numa.h"
#include "utils/poolallocator.h"

namespace growt
{
namespace BaseAllocator
{

struct NumaAlloc
{
    void* alloc(size_t n)
    {
        char* memory = (char*)numa_alloc_interleaved(n);
        if (!memory) { throw std::bad_alloc(); }
        std::fill(memory, memory + n, 0);
        return memory;
    }

    void dealloc(void* ptr, size_t size_hint) { numa_free(ptr, size_hint); }
};
} // namespace BaseAllocator

template <typename T = char>
using NUMAPoolAllocator = BasePoolAllocator<T, BaseAllocator::NumaAlloc>;
} // namespace growt

#endif //  NUMAPOOLALLOCATOR_H
