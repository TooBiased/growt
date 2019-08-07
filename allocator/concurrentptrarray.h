/*******************************************************************************
 * utils/pushbackarray.h
 *
 * Concurrent array data-structure used to store pointers to threadlocal
 * handles.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef CONCURRENT_PTR_ARRAY_H
#define CONCURRENT_PTR_ARRAY_H

#include <atomic>
#include <algorithm>
#include <limits>

template <typename T, class Alloc = std::allocator<T*> >
class ConcurrentPtrArray
{
private:
    using Element_t   = T;
    using ElementPtr  = T*;
    using ElementAPtr = std::atomic<T*>;
    using Allocator_t = typename Alloc::template rebind<ElementAPtr>::other;

    std::atomic_int    reader;
public:
    std::atomic_size_t size;
    std::atomic_size_t capacity;

    ConcurrentPtrArray(size_t cap) : reader(0), size(0), capacity(cap)
    {
        ElementAPtr* temp = allocator.allocate(cap);
        //std::fill(temp, temp+cap, ElementAPtr(nullptr));
        for (size_t i = 0; i<cap; ++i)
        {
            temp[i].store(nullptr);
        }
        data.store(temp);
    }

    ~ConcurrentPtrArray()
    {
        // locking is unnecessary because there should not be remaining accesses)
        lockWriter();

        ElementAPtr* temp = data.load();
        if (temp)
            allocator.deallocate(temp, capacity.load());
        // no unlock!
    }

    ConcurrentPtrArray(ConcurrentPtrArray&& rhs)
        : reader(0)
    {
        // lock rhs
        // (prob unnecessary because using the old structure will be undefined)
        rhs.lockWriter();

        size.store(rhs.size.load());
        capacity.store(rhs.size.load());
        data.store(rhs.data.load());
        rhs.capacity.store(0);
        rhs.size.store(0);
        rhs.data.store(nullptr);

        rhs.unlockWriter();
    }

    ConcurrentPtrArray& operator=(ConcurrentPtrArray&& rhs)
    {
        //Lock both lhs and rhsstructures

        // lock rhs
        // (prob unnecessary because using it concurrently will be undefined)
        rhs.lockWriter();
        // lock lhs
        // (prob unnecessary because using it concurrently will be undefined)
        lockWriter();

        size_t cap = capacity.load();
        if (cap)
        {
            allocator.deallocate(data, cap);
        }
        size.store(rhs.size.load());
        capacity.store(rhs.capacity.load());
        data.store(rhs.data.load());

        unlockWriter();
        rhs.unlockWriter();
    }


    size_t push_back(Element_t* e)
    {
        lockReader();
        size_t tcap  = capacity.load(std::memory_order_acquire);
        size_t tsize = size.load(std::memory_order_acquire);
        auto   tdata = data.load(std::memory_order_acquire);

        for (size_t i = 0; i < tsize; ++i)
        {
            if (! tdata[i].load())
            {
                if (initCell(tdata[i], e))
                {
                    unlockReader();
                    return i;
                }
            }
        }

        size_t pos = size.fetch_add(1, std::memory_order_acq_rel);
        if      (pos < tcap)
        {
            if (initCell(tdata[pos], e))
            {
                unlockReader();
                return pos;
            }
            else
            {
                unlockReader();
                return push_back(e);
            }
        }
        else if (pos == tcap)
        {
            unlockReader();
            lockWriter();

            // assumption: nobody else has grown in the meantime!
            auto newdata = allocator.allocate(tcap<<1);
            for (size_t i = 0; i < tcap; ++i)
            {
                newdata[i].store(tdata[i].load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            }
            newdata[pos].store(e);
            for (size_t i = tcap+1; i < tcap << 1; ++i)
            {
                newdata[i].store(nullptr,
                                 std::memory_order_relaxed);
            }
            data.store(newdata);
            capacity.store(tcap<<1);

            unlockWriter();
            return pos;
        }
        else
        {
            unlockReader();

            //wait till grown
            while (capacity.load(std::memory_order_acquire) < pos)
            { /*wait*/ }

            //reacquire reader lock
            lockReader();

            if (initCell(data.load(std::memory_order_acquire)[pos], e))
            {
                unlockReader();
                return pos;
            }
            else
            {
                unlockReader();
                return push_back(e);
            }
        }
    }

    void remove(size_t index)
    {
        update(index, nullptr);
    }

    void update(size_t index, Element_t* e)
    {
        lockReader();

        auto tdata = data.load();
        tdata[index].store(e);

        unlockReader();

        // wait until there are no readers once (== rcu grace period)
        //  => afterwards the old value can be deleted
        while (reader.load(std::memory_order_acquire))
        { /* wait */ }

        return;
    }

    // should have a () operator looking like:
    //  =>  size_t operator=(ElementPtr, size_t);
    template < class F >
    int forall(const F& f)
    {
        //acquire reader lock
        while (true)
        {
            while (reader.load() < 0)
            { /*wait till table is not write locked*/ }
            if (reader.fetch_add(1, std::memory_order_acq_rel) >= 0)
                break;
        }
        size_t tsize  = size.load();
        auto   tdata  = data.load();

        int res = 0;

        for (size_t i = 0; i < tsize; ++i)
        {
            ElementPtr curr = tdata[i].load();
            if (! curr) continue;
            res = f(curr, res);
        }


        //release reader lock
        reader.fetch_sub(1, std::memory_order_acq_rel);
        return res;
    }

private:
    Allocator_t allocator;

    std::atomic<ElementAPtr*> data;

    bool initCell(ElementAPtr& atomic, ElementPtr e)
    {
        ElementPtr temp_null = nullptr;
        return atomic.compare_exchange_weak(temp_null, e);
    }

    void lockReader()
    {
        while (true)
        {
            while (reader.load() < 0)
            { /*wait till table is not write locked*/ }
            if (reader.fetch_add(1, std::memory_order_acq_rel) >= 0)
                break;
        }
    }
    void lockWriter()
    {
        while (true)
        {
            while (reader.load())
            { /*wait while old structure is in use (this should never happen)*/ }
            int temp_zero = 0;
            if (reader.compare_exchange_weak(temp_zero, std::numeric_limits<int>::min()))
                break;
        }
    }

    void unlockReader()
    {
        reader.fetch_sub(1, std::memory_order_acq_rel);
    }

    void unlockWriter()
    {
        if (reader.exchange(0, std::memory_order_acq_rel))
        {
            /* unreachable code */
        }
    }
};


#endif // CONCURRENT_PTR_ARRAY_H
