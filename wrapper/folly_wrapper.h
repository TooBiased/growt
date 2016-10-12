/*******************************************************************************
 * wrapper/folly_wrapper.h
 *
 * Wrapper to use folly's folly::AtomicHashMap in our benchmarks
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef FOLLY_WRAPPER
#define FOLLY_WRAPPER

//#include "/home/maier/PHD/HashTables/Implementation/Competitors/folly/install_4.9.2/include/folly/AtomicHashMap.h"
#include "folly/AtomicHashMap.h"
#include "data-structures/returnelement.h"
#include <algorithm>

using namespace growt;

template<class Hasher>
class FollyWrapper
{
private:
    using HashType = folly::AtomicHashMap<size_t, size_t, Hasher>;

    HashType hash;
    size_t capacity;

public:
    FollyWrapper() = default;
    FollyWrapper(size_t capacity_) : hash(capacity_), capacity(capacity_) {}
    FollyWrapper(const FollyWrapper&) = delete;
    FollyWrapper& operator=(const FollyWrapper&) = delete;

    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    FollyWrapper(FollyWrapper&& rhs) : hash(rhs.capacity), capacity(rhs.capacity)
    { }

    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    FollyWrapper& operator=(FollyWrapper&& rhs)
    {
        capacity = rhs.capacity;
        (& hash)->~HashType();
        new (& hash) HashType(rhs.capacity);
        return *this;
    }


    using Handle = FollyWrapper&;
    Handle getHandle() { return *this; }


    inline ReturnElement find              (const size_t k)
    {
        auto ret = hash.find(k);
        if (ret == hash.end()) return ReturnElement::getEmpty();
        else return ReturnElement(k, ret->second);
    }

    inline ReturnCode insert               (const size_t k, const size_t d)
    {
        auto ret = hash.insert(std::make_pair(k,d));
        if (ret.second) return ReturnCode::SUCCESS_IN;
        else return ReturnCode::UNSUCCESS_ALREADY_USED;
    }

    template<class F>
    inline ReturnCode update               (const size_t k, const size_t d, F f)
    {
        auto ret = hash.insert(std::make_pair(k,0));
        if (! ret.second)
        {
            f.atomic(ret.first->second, k, d);
            return ReturnCode::SUCCESS_UP;
        }
        else return ReturnCode::UNSUCCESS_NOT_FOUND;
    }

    template<class F>
    inline ReturnCode insertOrUpdate       (const size_t k, const size_t d, F f)
    {
        auto ret = hash.insert(std::make_pair(k,d));
        if (! ret.second)
        {
            f.atomic(ret.first->second, k, d);
            return ReturnCode::SUCCESS_UP;
        }
        else return ReturnCode::SUCCESS_IN;
    }

    template<class F>
    inline ReturnCode update_unsafe        (const size_t k, const size_t d, F f)
    {
        return update(k,d,f);
    }

    template<class F>
    inline ReturnCode insertOrUpdate_unsafe(const size_t k, const size_t d, F f)
    {
        return insertOrUpdate(k,d,f);
    }

    inline ReturnCode remove               (const size_t k)
    {
        if (hash.erase(k) == 1) return ReturnCode::SUCCESS_DEL;
        else return ReturnCode::UNSUCCESS_NOT_FOUND;
    }
};

#endif // FOLLY_WRAPPER
