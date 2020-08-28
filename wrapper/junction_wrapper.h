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
#include "wrapper/stupid_iterator.h"

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

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<key_type, mapped_type>;
    using insert_return_type = std::pair<iterator, bool>;

    JunctionHandle() = delete;
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
    JunctionHandle& operator=(JunctionHandle&& rhs)
    {
        if (&rhs == this) return *this;
        this->~JunctionHandle();
        new (this) JunctionHandle(std::move(rhs));
        return *this;
    }

    inline iterator find              (const key_type& k)
    {
        mapped_type r = mapped_type();
        {
            r = hash.find(k).getValue();
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        if (r) return iterator(k,r);
        else   return end();
    }

    inline insert_return_type insert               (const key_type& k, const mapped_type& d)
    {
        auto inserted = false;
        auto temp = mapped_type();
        {
            auto mutator = hash.insertOrFind(k);

            temp = mutator.getValue();

            if (! temp)
            {
                mutator.exchangeValue(d);
                temp = d;
                inserted = true;
            }
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        return insert_return_type(iterator(k,temp), inserted);
    }

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k, F f, Types&& ... args)
    {
        //static_assert(F::junction_compatible::value, //TJuncComp<F>::value,
        //              "Used update function is not Junction compatible!");
        if (! F::junction_compatible::value) return insert_return_type(end(), false);

        bool changed     = false;
        auto temp        = mapped_type();
        {
            auto mutator = hash.find(k);
            temp = mutator.getValue();
            if (temp)
            {
                f(temp, std::forward<Types>(args)...);
                mutator.exchangeValue(temp);
                changed = true;
            }
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        return insert_return_type(iterator(changed ? k: 0, temp), changed);
    }

    template<class F, class ... Types>
    inline insert_return_type insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        //static_assert(F::junction_compatible::value, //TJuncComp<F>::value,
        //              "Used update function is not Junction compatible!");

        if (! F::junction_compatible::value) return insert_return_type(end(), false);

        bool inserted = false;
        auto temp     = mapped_type();
        {
            auto mutator = hash.insertOrFind(k);
            temp = mutator.getValue();
            if (temp)
            {
                f(temp, std::forward<Types>(args)...);
                mutator.exchangeValue(temp);
            }
            else
            {
                mutator.exchangeValue(d);
                inserted = true;
                temp = d;
            }
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        return insert_return_type(iterator(k, temp), inserted);
    }

    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k, F f, Types&& ... args)
    {
        return update(k,f,std::forward<Types>(args)...);
    }

    template<class F, class ... Types>
    inline insert_return_type insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        return insert_or_update(k,d,f,std::forward<Types>(args)...);
    }

    inline size_t erase(const key_type& k)
    {
        size_t r = 0;
        {
            auto mutator = hash.find(k);
            r = mutator.eraseValue();
        }
        if (++count > 64)
        {
            count = 0;
            junction::DefaultQSBR.update(qsbrContext);
        }
        return r;
    }

    inline iterator end() { return iterator(); }
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

    Handle get_handle() {   return Handle(*hash);   }

private:
    std::unique_ptr<HashType> hash;
    friend Handle;
};


#endif // JUNCTION_WRAPPER
