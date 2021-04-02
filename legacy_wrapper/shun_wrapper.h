/*******************************************************************************
 * wrapper/shun_wrapper.h
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

#ifndef SHUN_WRAPPER
#define SHUN_WRAPPER

#ifdef SHUN_ND
#include "ndHash.h"
#else
#define MCX16
#include "deterministicHash.h"
#endif

#include "wrapper/stupid_iterator.hpp"

using namespace growt;

template<class Hasher>
class shun_wrapper
{
private:
    using uint64 = u_int64_t;
    using uint = unsigned int;
    using ll = long long;

    struct hash_cell_wrapper
    {
        using eType = std::pair<uint64, uint64>;
        using kType = uint64;

        inline eType empty()
        { return std::make_pair(0,0); }

        inline kType getKey(eType v)
        { return v.first; }

        inline uintT hash(kType s)
        { return (Hasher()(s)); }

        inline int cmp(kType v, kType b)
        { return ((v > b) ? 1 : ((v == b) ? 0 : -1)); }

        //NOTICE: 1 MEANS THAT ALL INSERTIONS ARE ASSIGNS 0 WOULD MAKE ALL ASSIGNS INSERTIONS
        inline bool  replaceQ(eType s, eType s2)
        { return (s.first == s2.first); }
    };

    using table_type = Table<hash_cell_wrapper, ll>;

    table_type hash;
    size_t capacity;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<size_t, size_t>;
    using insert_return_type = std::pair<iterator, bool>;

    shun_wrapper() = default;
    shun_wrapper(size_t capacity_) : hash(capacity_, hash_cell_wrapper()), capacity(capacity_) {}
    shun_wrapper(const shun_wrapper&) = delete;
    shun_wrapper& operator=(const shun_wrapper&) = delete;

    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    shun_wrapper(shun_wrapper&& rhs) : hash(rhs.capacity, hash_cell_wrapper())
    { }

    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    shun_wrapper& operator=(shun_wrapper&& rhs)
    {
        capacity = rhs.capacity;
        (& hash)->~table_type();
        new (& hash) table_type(rhs.capacity, hash_cell_wrapper());
        return *this;
    }


    using handle_type = shun_wrapper&;
    handle_type get_handle() { return *this; }


    inline iterator find(const key_type& k)
    {
        auto temp = hash.find(k);
        if (temp.first == 0) return end();
        else return iterator(k,temp.second);
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
    {
        bool res  = hash.insert(std::make_pair(k,d));
        return std::make_pair(iterator(k,d), res);
    }

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k, F f, Types&& ... args)
    {
        return std::make_pair(iterator(), false);
    }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        return std::make_pair(iterator(), false);
    }

    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k, F f, Types&& ... args)
    {
        return std::make_pair(iterator(), false);
    }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        return std::make_pair(iterator(), false);
    }

    inline size_t erase(const key_type& k)
    {
        return hash.deleteVal(k);
    }

    inline iterator end() { return iterator(); }
};

template <class Key, class Data, class HashFct, class Allocator, hmod ... Mods>
class shun_config
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

    static_assert(is_viable, "shun wrapper does not support the chosen flags");

    using table_type = shun_wrapper<hash_fct_type>;

    static std::string name()
    {
        #ifdef SHUN_ND
        return "shun_nd";
        #else
        return "shun";
        #endif
    }
};

#endif // SHUN_WRAPPER
