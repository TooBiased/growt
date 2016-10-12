/*******************************************************************************
 * wrapper/junction_wrapper.h
 *
 * Wrapper to use junction's junction::ConcurrentMap_... in our benchmarks
 * use #define JUNCTION_TYPE junction::ConcurrentMap_Leapfrog to chose the table
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef JUNCTION_WRAPPER
#define JUNCTION_WRAPPER

#include <junction/Core.h>
#include <junction/ConcurrentMap_Linear.h>
#include <junction/ConcurrentMap_Grampa.h>
#include <junction/ConcurrentMap_Leapfrog.h>
#include <junction/QSBR.h>
#include <mutex>
#include <memory>

#include "data-structures/returnelement.h"

using namespace growt;

struct ValueTraits
{
    using T = turf::u64;
    using IntType = typename turf::util::BestFit<turf::u64>::Unsigned;

    static const IntType NullValue = 0;
    static const IntType Redirect  = 1ull << 63;
};


// TJuncComp<F>::value <==> F is JunctionCompatible
template <typename TFunctor>
class TJuncComp
{
    typedef char one;
    typedef long two;

    template <typename C> static one test( decltype(&C::junction) ) ;
    template <typename C> static two test(...);

public:
    enum { value = sizeof(test<TFunctor>(0)) == sizeof(char) };
};



class JunctionHandle
{
private:
    using HashType = JUNCTION_TYPE<turf::u64, turf::u64, junction::DefaultKeyTraits<turf::u64>, ValueTraits>;

    HashType& hash;
    int count;
    junction::QSBR::Context qsbrContext;

public:
    //static std::mutex registration_mutex;

    JunctionHandle() = default;
    JunctionHandle(HashType& hash_table) : hash(hash_table), count(0)
    {
        //std::lock_guard<std::mutex> lock(registration_mutex);
        qsbrContext = junction::DefaultQSBR.createContext();
    }

    ~JunctionHandle()
    {
        //std::lock_guard<std::mutex> lock(junction_mutex);
        junction::DefaultQSBR.destroyContext(qsbrContext);
    }

    JunctionHandle(const JunctionHandle&) = delete;
    JunctionHandle& operator=(const JunctionHandle&) = delete;

    JunctionHandle(JunctionHandle&& rhs) = default;
    JunctionHandle& operator=(JunctionHandle&& rhs) = default;

    inline ReturnElement find              (const size_t k)
    {
        size_t r = 0;
        {
            r = hash.find(k).getValue();
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        if (r) return ReturnElement(k,r);
        else   return ReturnElement::getEmpty();
    }

    inline ReturnCode insert               (const size_t k, const size_t d)
    {
        ReturnCode r = ReturnCode::UNSUCCESS_ALREADY_USED;
        {
            auto mutator = hash.insertOrFind(k);

            if (! mutator.getValue())
            {
                mutator.exchangeValue(d);
                r = ReturnCode::SUCCESS_IN;
            }
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        return r;
    }

    template<class F>
    inline ReturnCode update               (const size_t k, const size_t d, F f)
    {
        //static_assert(F::junction_compatible::value, //TJuncComp<F>::value,
        //              "Used update function is not Junction compatible!");
        if (! F::junction_compatible::value) return ReturnCode::ERROR;

        ReturnCode r = ReturnCode::UNSUCCESS_NOT_FOUND;
        {
            auto mutator = hash.find(k);
            uint64_t temp    = mutator.getValue();
            if (temp)
            {
                f(temp, k, d);
                mutator.exchangeValue(temp);
                r = ReturnCode::SUCCESS_UP;
            }
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        return r;
    }

    template<class F>
    inline ReturnCode insertOrUpdate       (const size_t k, const size_t d, F f)
    {
        //static_assert(F::junction_compatible::value, //TJuncComp<F>::value,
        //              "Used update function is not Junction compatible!");

        if (! F::junction_compatible::value) return ReturnCode::ERROR;

        ReturnCode r = ReturnCode::ERROR;
        {
            auto mutator = hash.insertOrFind(k);
            uint64_t temp = mutator.getValue();
            if (temp)
            {
                f(temp, k, d);
                mutator.exchangeValue(temp);
                r = ReturnCode::SUCCESS_UP;
            }
            else
            {
                mutator.exchangeValue(d);
                r = ReturnCode::SUCCESS_IN;
            }
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        return r;
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
        bool r = false;
        {
            auto mutator = hash.find(k);
            r = mutator.eraseValue();
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        return (r) ? ReturnCode::SUCCESS_DEL : ReturnCode::UNSUCCESS_NOT_FOUND;
    }
};

//std::mutex JunctionHandle::registration_mutex;

size_t nextPowerOf2(size_t n)
{
    auto temp = 1u;
    while (n > temp) temp <<= 1;
    return temp;
}


class JunctionWrapper
{
public:
    using HashType = JUNCTION_TYPE<turf::u64, turf::u64, junction::DefaultKeyTraits<turf::u64>, ValueTraits>;
    using Handle = JunctionHandle;

    JunctionWrapper(size_t capacity) : hash(new HashType(nextPowerOf2(capacity)<<1)) { }
    ~JunctionWrapper() { }
    JunctionWrapper(const JunctionWrapper&) = delete;
    JunctionWrapper& operator=(const JunctionWrapper&) = delete;
    JunctionWrapper(JunctionWrapper&& rhs) = default;
    JunctionWrapper& operator=(JunctionWrapper&& rhs) = default;
    Handle getHandle() {   return Handle(*hash);   }

private:
    std::unique_ptr<HashType> hash;
    friend Handle;
};


#endif // JUNCTION_WRAPPER
