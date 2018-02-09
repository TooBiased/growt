/*******************************************************************************
 * wrapper/hopscotch_wrapper.h
 *
 * Wrapper to use the Hopscotch table as implemented by Herlihy et al.
 * BitmapHopscotchHashMap<...> in our benchmarks.
 *
 * THIS TABLE IS A SET DATASTRUCTURE, NOT A TRUE HASH TABLE
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef HOPSCOTCH_WRAPPER
#define HOPSCOTCH_WRAPPER

#include <memory>

#define INTEL64
#include <BitmapHopscotchHashMap.h>
#include <../framework/cpp_framework.h>

#include "wrapper/stupid_iterator.h"

//using namespace growt;

template<class Hasher>
class HopscotchWrapper
{
private:
    using uint64 = u_int64_t;
    using uint = unsigned int;
    using ll = long long;

    template<class HasherForThing>
    class HopscotchHashThing
    {
    public:
        static const uint64 _EMPTY_HASH  =  0;
        static const uint64 _BUSY_HASH   =  1;
        static const ll     _EMPTY_KEY   =  0;
        static const ll     _EMPTY_DATA  =  0;

        inline static uint64 Calc(ll key)
        {
            HasherForThing hasher;
            return hasher(key);
                 //uint64(__builtin_ia32_crc32di(1329235987123598723ull, key));
        }

        inline static bool IsEqual(ll left_key, ll right_key)
        { return left_key == right_key; }

        inline static void relocate_key_reference(ll volatile& left,
                                                  const ll volatile& right)
        { left = right; }

        inline static void relocate_data_reference(ll volatile& left,
                                                   const ll volatile& right)
        { left = right; }
    };

    using HashType = BitmapHopscotchHashMap<ll, ll,
                                            HopscotchHashThing<Hasher>,
                                            CMDR::TTASLock, CMDR::Memory>;
    HashType hash;
    size_t capacity;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<size_t, size_t>;
    using const_iterator     = bool;
    using insert_return_type = std::pair<iterator, bool>;

    HopscotchWrapper() = default;
    HopscotchWrapper(size_t capacity_) : hash(capacity_, 256), capacity(capacity_) {}
    HopscotchWrapper(const HopscotchWrapper&) = delete;
    HopscotchWrapper& operator=(const HopscotchWrapper&) = delete;

    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    HopscotchWrapper(HopscotchWrapper&& rhs) : hash(rhs.capacity*2, 256)
    { }

    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    HopscotchWrapper& operator=(HopscotchWrapper&& rhs)
    {
        capacity = rhs.capacity;
        (& hash)->~HashType();
        new (& hash) HashType(rhs.capacity, 256);
        return *this;
    }


    using Handle = HopscotchWrapper&;
    Handle getHandle() { return *this; }


    inline iterator find(const key_type& k)
    {
        size_t res = hash.containsKey(k); // LEAHASH and HOPSCOTCH Implementations are sets
        return iterator(k,res);
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
    {
        bool res = (hash.putIfAbsent(k,d) == HopscotchHashThing<Hasher>::_EMPTY_DATA);
        return std::make_pair(iterator(k,d),res);
    }

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k, F f, Types&& ... args)
    { return std::make_pair(iterator(),false); }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    { return std::make_pair(iterator(),false); }

    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k, F f, Types&& ... args)
    { return std::make_pair(iterator(),false); }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    { return std::make_pair(iterator(),false); }

    inline size_t erase(const key_type& k)
    {
        return hash.remove(k);
    }

    inline iterator end() { return false; }
};

#endif // HOPSCOTCH_WRAPPER
