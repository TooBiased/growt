/*******************************************************************************
 * wrapper/tbb_hm_wrapper.h
 *
 * Wrapper to use tbb's tbb::concurrent_hash_map in our benchmarks
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef TBB_HM_WRAPPER
#define TBB_HM_WRAPPER

#include <tbb/concurrent_hash_map.h>
#include <atomic>
#include <memory>

#include "data-structures/returnelement.h"

using namespace growt;

template<class Hasher>
class HashCompareClass
{
private:
    Hasher fct;
public:

    inline bool equal(const size_t& k0, const size_t& k1) const
    {
        return k0 == k1;
    }

    inline size_t hash(const size_t& k) const
    {
        return fct(k);
    }
};


template<class Hasher>
class TBBHMWrapper
{
private:
    using HashType = tbb::concurrent_hash_map<size_t, size_t, HashCompareClass<Hasher> >;
    using Accessor = typename HashType::accessor;

    HashType hash;

public:
    TBBHMWrapper() = default;
    TBBHMWrapper(size_t capacity_) : hash(capacity_) { }
    TBBHMWrapper(const TBBHMWrapper&) = delete;
    TBBHMWrapper& operator=(const TBBHMWrapper&) = delete;

    TBBHMWrapper(TBBHMWrapper&& rhs) = default;
    TBBHMWrapper& operator=(TBBHMWrapper&& rhs) = default;

    using Handle = TBBHMWrapper&;
    Handle getHandle() { return *this; }


    inline ReturnElement find              (const size_t k)
    {
        Accessor a;
        if (hash.find(a, k))
            return ReturnElement(k, a->second);
        else
            return ReturnElement::getEmpty();
    }

    inline ReturnCode insert               (const size_t k, const size_t d)
    {
        Accessor a;
        if(hash.insert(a,k))
        {
            a->second = d;
            return ReturnCode::SUCCESS_IN;
        }
        else
        {
            return ReturnCode::UNSUCCESS_ALREADY_USED;
        }
    }

    template<class F>
    inline ReturnCode update               (const size_t k, const size_t d, F f)
    {
        Accessor a;
        if (hash.find(a, k))
        {
            f(a->second, k, d);
            return ReturnCode::SUCCESS_UP;
        }
        else return ReturnCode::UNSUCCESS_NOT_FOUND;
    }

    template<class F>
    inline ReturnCode insertOrUpdate       (const size_t k, const size_t d, F f)
    {
        Accessor a;
        if (hash.insert(a,k))
        {
            a->second = d;
            return ReturnCode::SUCCESS_IN;
        }
        else
        {
            f(a->second, k, d);
            return ReturnCode::SUCCESS_UP;
        }

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
        return ( hash.erase(k) ) ? ReturnCode::SUCCESS_DEL : ReturnCode::UNSUCCESS_NOT_FOUND;
    }
};

#endif // TBB_HM_WRAPPER
