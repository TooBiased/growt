/*******************************************************************************
 * wrapper/folly_wrapper.h
 *
 * Wrapper to use the Hash Tables programmed by Julian Shun as part of his
 * Dissertation with Prof. Guy Blelloch.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef LEAHASH_WRAPPER
#define LEAHASH_WRAPPER

#include <memory>

#define INTEL64
#include <../framework/cpp_framework.h>
#include <ChainedHashMap.h>

#include "wrapper/stupid_iterator.hpp"

//using namespace growt;

template<class Hasher>
class leahash_wrapper
{
private:
    using uint64 = u_int64_t;
    using uint = unsigned int;
    using ll = long long;

    class ChainedHashThing
    {
    public:
        static const uint64 _EMPTY_HASH  =  0;
        static const uint64 _BUSY_HASH   =  1;
        static const uint64 _EMPTY_KEY   =  0;
        static const uint64 _EMPTY_DATA  =  0;

        inline static uint64 Calc(uint64 key)
            { return Hasher()(key); }

        inline static bool IsEqual(uint64 left_key, uint64 right_key)
        { return left_key == right_key; }

        inline static void relocate_key_reference( uint64 volatile& left,
                                                   const uint64 volatile& right)
        { left = right; }

        inline static void relocate_data_reference(uint64 volatile& left,
                                                   const uint64 volatile& right)
        { left = right; }
    };

    using HashType = ChainedHashMap<uint64, uint64,
                                    ChainedHashThing,
                                    CMDR::TTASLock, CMDR::Memory>;

    HashType hash;
    size_t capacity;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<size_t, size_t>;
    using insert_return_type = std::pair<iterator, bool>;

    leahash_wrapper() = default;
    leahash_wrapper(size_t capacity_) : hash(capacity_, 256), capacity(capacity_) {}
    leahash_wrapper(const leahash_wrapper&) = delete;
    leahash_wrapper& operator=(const leahash_wrapper&) = delete;

    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    leahash_wrapper(leahash_wrapper&& rhs) : hash(rhs.capacity, 256)
    { }

    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    leahash_wrapper& operator=(leahash_wrapper&& rhs)
    {
        capacity = rhs.capacity;
        (& hash)->~HashType();
        new (& hash) HashType(rhs.capacity, 256);
        return *this;
    }


    using handle_type = leahash_wrapper&;
    handle_type get_handle() { return *this; }


    inline iterator find(const key_type& k)
    {
        size_t res = hash.containsKey(k); // LEAHASH and HOPSCOTCH Implementations are sets
        if (!res) return iterator();
        return iterator(k,res);
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
    {
        bool res = (hash.putIfAbsent(k,d) == ChainedHashThing::_EMPTY_DATA);
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


template <class Key, class Data, class HashFct, class Allocator, hmod ... Mods>
class leahash_config
{
public:
    using key_type = Key;
    using mapped_type = Data;
    using hash_fct_type = HashFct;
    using allocator_type = Allocator;

    using mods = mod_aggregator<Mods ...>;

    // Derived Types
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr bool is_viable = true;

    static_assert(is_viable, "leahash wrapper does not support the chosen flags");

    using table_type = leahash_wrapper<hash_fct_type>;

    static std::string name() { return "leahash"; }
};

#endif // LEAHASH_WRAPPER
