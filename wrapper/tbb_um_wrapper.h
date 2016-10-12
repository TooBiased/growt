/*******************************************************************************
 * wrapper/tbb_um_wrapper.h
 *
 * Wrapper to use tbb's tbb::concurrent_unordered_map in our benchmarks
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef TBB_UM_WRAPPER
#define TBB_UM_WRAPPER

#include <tbb/concurrent_unordered_map.h>
#include <atomic>
#include <memory>

#include "data-structures/returnelement.h"

using namespace growt;

template<class Hasher>
class TBBUMWrapper
{
private:
    using HashType = tbb::concurrent_unordered_map<size_t, size_t, Hasher>;

    HashType hash;

public:
    TBBUMWrapper() = default;
    TBBUMWrapper(size_t capacity_) : hash(capacity_) { }
    TBBUMWrapper(const TBBUMWrapper&) = delete;
    TBBUMWrapper& operator=(const TBBUMWrapper&) = delete;

    TBBUMWrapper(TBBUMWrapper&& rhs) = default;
    TBBUMWrapper& operator=(TBBUMWrapper&& rhs) = default;

    using Handle = TBBUMWrapper&;
    Handle getHandle() { return *this; }


    inline ReturnElement find              (const size_t k)
    {
        auto temp = hash.find(k);
        if (temp == end(hash)) return ReturnElement::getEmpty();
        else return ReturnElement(k, temp->second);
    }

    inline ReturnCode insert               (const size_t k, const size_t d)
    {
        auto ret = hash.insert(std::make_pair(k,d)).second;
        return (ret) ? ReturnCode::SUCCESS_IN : ReturnCode::UNSUCCESS_ALREADY_USED;
    }

    template<class F>
    inline ReturnCode update               (const size_t k, const size_t d, F f)
    {
        f.atomic(hash[k], k, d);
        return ReturnCode::SUCCESS_UP;
    }

    template<class F>
    inline ReturnCode insertOrUpdate       (const size_t k, const size_t d, F f)
    {
        auto temp = hash.insert(std::make_pair(k,d));
        if (temp.second) return ReturnCode::SUCCESS_IN;
        else
        {
            f.atomic(temp.first->second, k, d);
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

    inline ReturnCode remove               (const size_t)
    {
        //static_assert(false, "tbb_um .remove(key) is not implemented")
        return ReturnCode::ERROR;
    }
};

#endif // TBB_UM_WRAPPER
