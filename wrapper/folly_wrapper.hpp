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
#include "data-structures/hash_table_mods.hpp"
#include "data-structures/returnelement.hpp"
#include <algorithm>

using namespace growt;

template<class Key, class Data, class Hasher, class Allocator>
class folly_wrapper
{
private:
    using internal_table_type = folly::AtomicHashMap<Key, Data, Hasher>;

    internal_table_type hash;
    size_t capacity;

public:

    using key_type           = Key;
    using mapped_type        = Data;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = typename internal_table_type::iterator;
    using const_iterator     = typename internal_table_type::const_iterator;
    using insert_return_type = std::pair<iterator, bool>;

    static constexpr size_t next_cap(size_t i)
    {
	    size_t curr = 4096;
	    while (curr<i) curr<<=1;
	    return curr << 1;
    }

    folly_wrapper() = default;
    folly_wrapper(size_t capacity_)
        : hash(next_cap(capacity_)), capacity(next_cap(capacity_)) {}
    folly_wrapper(const folly_wrapper&) = delete;
    folly_wrapper& operator=(const folly_wrapper&) = delete;
    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    folly_wrapper(folly_wrapper&& rhs)
        : hash(rhs.capacity), capacity(rhs.capacity)
    { }
    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    folly_wrapper& operator=(folly_wrapper&& rhs)
    {
        capacity = rhs.capacity;
        (& hash)->~internal_table_type();
        new (& hash) internal_table_type(rhs.capacity);
        return *this;
    }

    using Handle = folly_wrapper&;
    Handle get_handle() { return *this; }

    inline iterator find(const key_type& k);

    inline insert_return_type insert(const key_type& k, const mapped_type& d);

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k,
                                     F f, Types&& ... args);

    template<class F, class ... Types>
    inline insert_return_type insert_or_update(const key_type& k,
                                               const mapped_type& d,
                                               F f, Types&& ... args);
    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k,
                                            F f, Types&& ... args);

    template<class F, class ... Types>
    inline insert_return_type insert_or_update_unsafe(const key_type& k,
                                                      const mapped_type& d,
                                                      F f, Types&& ... args);
    inline size_t erase(const key_type& k);

    inline iterator        end()         { return hash.end();  }
    inline const_iterator  end()   const { return hash.cend(); }
    inline const_iterator cend()   const { return hash.cend(); }

    inline iterator        begin()       { return hash.begin();  }
    inline const_iterator  begin() const { return hash.cbegin(); }
    inline const_iterator cbegin() const { return hash.cbegin(); }
};


template <class Key, class Data, class HashFct, class Allocator, hmod ... Mods>
class folly_config
{
public:
    using key_type = Key;
    using mapped_type = Data;
    using hash_fct_type = HashFct;
    using allocator_type = Allocator;

    using mods = mod_aggregator<Mods ...>;

    // Derived Types
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr bool is_viable = !(mods::template is<hmod::ref_integrity>());

    static_assert(is_viable, "folly wrapper does not support the chosen flags");

    using table_type = folly_wrapper<key_type, mapped_type,
                                     hash_fct_type, allocator_type>;
};





template <class K, class D, class HF, class AL>
typename folly_wrapper<K,D,HF,AL>::iterator
folly_wrapper<K,D,HF,AL>::find(const key_type& k)
{
    return hash.find(k);
    // auto ret = hash.find(k);
    // if (ret == hash.end()) return ReturnElement::getEmpty();
    // else return ReturnElement(k, ret->second);
}

template <class K, class D, class HF, class AL>
typename folly_wrapper<K,D,HF,AL>::insert_return_type
folly_wrapper<K,D,HF,AL>::insert(const key_type& k, const mapped_type& d)
{
    return hash.insert(k,d);
    // auto ret = hash.insert(std::make_pair(k,d));
    // if (ret.second) return ReturnCode::SUCCESS_IN;
    // else return ReturnCode::UNSUCCESS_ALREADY_USED;
}

template <class K, class D, class HF, class AL>
template<class F, class ... Types>
typename folly_wrapper<K,D,HF,AL>::insert_return_type
folly_wrapper<K,D,HF,AL>::update(const key_type& k, F f, Types&& ... args)
{
    auto result = hash.find(k);
    if (result != hash.end())
    {
        f.atomic(result->second, std::forward<Types>(args)...);
    }
    return std::make_pair(result, result != hash.end());
}

template <class K, class D, class HF, class AL>
template<class F, class ... Types>
typename folly_wrapper<K,D,HF,AL>::insert_return_type
folly_wrapper<K,D,HF,AL>::insert_or_update(const key_type& k,
                                           const mapped_type& d,
                                           F f, Types&& ... args)
{
    auto ret = hash.insert(std::make_pair(k,d));
    if (! ret.second)
    {
        f.atomic(ret.first->second, std::forward<Types>(args)...);
    }
    return ret;
}

template <class K, class D, class HF, class AL>
template<class F, class ... Types>
typename folly_wrapper<K,D,HF,AL>::insert_return_type
folly_wrapper<K,D,HF,AL>::update_unsafe(const key_type& k,
                                        F f, Types&& ... args)
{
    return update(k,f, std::forward<Types>(args)...);
}

template <class K, class D, class HF, class AL>
template<class F, class ... Types>
typename folly_wrapper<K,D,HF,AL>::insert_return_type
folly_wrapper<K,D,HF,AL>::insert_or_update_unsafe(const key_type& k,
                                                  const mapped_type& d,
                                                  F f, Types&& ... args)
{
    return insert_or_update(k,d,f, std::forward<Types>(args)...);
}

template <class K, class D, class HF, class AL>
size_t
folly_wrapper<K,D,HF,AL>::erase(const key_type& k)
{
    return hash.erase(k);
}

#endif // FOLLY_WRAPPER
