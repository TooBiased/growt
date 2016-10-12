/*******************************************************************************
 * wrapper/cuckoo_wrapper.h
 *
 * Wrapper to use libcuckoo cuckoohash_map in our benchmarks
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef CUCKOO_WRAPPER
#define CUCKOO_WRAPPER

#include <libcuckoo/cuckoohash_map.hh>
#include "data-structures/returnelement.h"

using namespace growt;

template<class Hasher>
class CuckooWrapper
{
private:
    using HashType = cuckoohash_map<size_t, size_t, Hasher>;

    HashType hash;
    size_t capacity;

public:
    CuckooWrapper() = default;
    CuckooWrapper(size_t capacity_) : hash(capacity_), capacity(capacity_) {}
    CuckooWrapper(const CuckooWrapper&) = delete;
    CuckooWrapper& operator=(const CuckooWrapper&) = delete;

    CuckooWrapper(CuckooWrapper&& rhs) : hash(rhs.capacity), capacity(rhs.capacity) {}
    CuckooWrapper& operator=(CuckooWrapper&& rhs)
    {
        capacity = rhs.capacity;

        (&hash)->~HashType();
        new (&hash) HashType(capacity);

        return *this;
    }


    using Handle = CuckooWrapper&;
    Handle getHandle() { return *this; }


    inline ReturnElement find              (const size_t k)
    {
        size_t temp = 0;
        hash.find(k,temp);
        if (temp) return ReturnElement(k,temp);
        else      return ReturnElement::getEmpty();
    }

    inline ReturnCode insert               (const size_t k, const size_t d)
    {
        return (hash.insert(k,d)) ? ReturnCode::SUCCESS_IN :
                                    ReturnCode::UNSUCCESS_ALREADY_USED;
    }

    template<class F>
    inline ReturnCode update               (const size_t k, const size_t d, F f)
    {
        bool res = hash.update_fn(k, [k,d,&f](size_t & v){ f(v, k, d); });
        return (res) ? ReturnCode::SUCCESS_UP :
                       ReturnCode::UNSUCCESS_NOT_FOUND;
    }

    template<class F>
    inline ReturnCode insertOrUpdate       (const size_t k, const size_t d, F f)
    {
        // no differentiation between update and insert
        hash.upsert(k, [k,d,&f](size_t & v){ f(v, k, d); }, d);
        return ReturnCode::SUCCESS_IN;
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

#endif // CUCKOO_WRAPPER
