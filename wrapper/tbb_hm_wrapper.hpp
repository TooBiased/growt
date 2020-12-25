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
#include "data-structures/hash_table_mods.hpp"
#include "wrapper/stupid_iterator.h"

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


template<class Key, class Data, class Hasher, class Allocator>
class tbb_hm_wrapper
{
private:
    using HashType = tbb::concurrent_hash_map<Key, Data, HashCompareClass<Hasher> >;
    using Accessor = typename HashType::accessor;

    HashType hash;

public:

    using key_type           = Key;
    using mapped_type        = Data;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<key_type, mapped_type>;
    using insert_return_type = std::pair<iterator, bool>;

    tbb_hm_wrapper() = default;
    tbb_hm_wrapper(size_t capacity_) : hash(capacity_) { }
    tbb_hm_wrapper(const tbb_hm_wrapper&) = delete;
    tbb_hm_wrapper& operator=(const tbb_hm_wrapper&) = delete;

    tbb_hm_wrapper(tbb_hm_wrapper&& rhs) = default;
    tbb_hm_wrapper& operator=(tbb_hm_wrapper&& rhs) = default;

    using Handle = tbb_hm_wrapper&;
    Handle get_handle() { return *this; }

    inline iterator find(const key_type& k);
    inline insert_return_type insert(const key_type& k, const mapped_type& d);

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k, F f, Types&& ... args);
    template<class F, class ... Types>
    inline insert_return_type insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args);
    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k, F f, Types&& ... args);
    template<class F, class ... Types>
    inline insert_return_type insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args);
    inline size_t erase(const key_type& k);
    inline iterator end();
};


template <class Key, class Data, class HashFct, class Allocator, hmod ... Mods>
class tbb_hm_config
{
public:
    using key_type = Key;
    using mapped_type = Data;
    using hash_fct_type = HashFct;
    using allocator_type = Allocator;

    using mods = mod_aggregator<Mods ...>;

    // Derived Types
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr is_viable = !(mods::template is<ref_integrity>());

    static_assert(is_viable, "tbb hm wrapper does not support the chosen flags");

    using table_type = tbb_hm_wrapper<key_type, mapped_type,
                                     hash_fct_type, allocator_type>;
};




template<class K, class D, class HF, class AL>
typename tbb_hm_wrapper<K,D,HF,AL>::iterator
tbb_hm_wrapper<K,D,HF,AL>::find(const key_type& k)
{
    Accessor a;
    if (hash.find(a,k)) return iterator(k,a->second);
    return end();
    // if (hash.find(a, k))
    //     return ReturnElement(k, a->second);
    // else
    //     return ReturnElement::getEmpty();
}

template<class K, class D, class HF, class AL>
typename tbb_hm_wrapper<K,D,HF,AL>::insert_return_type
tbb_hm_wrapper<K,D,HF,AL>::insert(const key_type& k, const mapped_type& d)
{
    Accessor a;
    if(hash.insert(a,k))
    {
        a->second = d;
        return std::make_pair(iterator(k,d),true);
        // return ReturnCode::SUCCESS_IN;
    }
    else
    {
        return std::make_pair(iterator(k,a->second),false);
        // return ReturnCode::UNSUCCESS_ALREADY_USED;
    }
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename tbb_hm_wrapper<K,D,HF,AL>::insert_return_type
tbb_hm_wrapper<K,D,HF,AL>::update(const key_type& k, F f, Types&& ... args)
{
    Accessor a;
    if (hash.find(a, k))
    {
        auto temp = f(a->second, std::forward<Types>(args)...);
        return std::make_pair(iterator(k, temp),true);//ReturnCode::SUCCESS_UP;
    }
    else return std::make_pair(end(),false);//ReturnCode::UNSUCCESS_NOT_FOUND;
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename tbb_hm_wrapper<K,D,HF,AL>::insert_return_type
tbb_hm_wrapper<K,D,HF,AL>::insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    Accessor a;
    if (hash.insert(a,k))
    {
        a->second = d;
        return std::make_pair(iterator(k,d),true);//ReturnCode::SUCCESS_IN;
    }
    else
    {
        auto temp = f(a->second, std::forward<Types>(args)...);
        return std::make_pair(iterator(k,temp),false);//ReturnCode::SUCCESS_UP;
    }
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename tbb_hm_wrapper<K,D,HF,AL>::insert_return_type
tbb_hm_wrapper<K,D,HF,AL>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    return update(k,f, std::forward<Types>(args)...);
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename tbb_hm_wrapper<K,D,HF,AL>::insert_return_type
tbb_hm_wrapper<K,D,HF,AL>::insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    return insert_or_update(k,d,f, std::forward<Types>(args)...);
}

template<class K, class D, class HF, class AL>
size_t
tbb_hm_wrapper<K,D,HF,AL>::erase(const key_type& k)
{
    return hash.erase(k);
}

template<class K, class D, class HF, class AL>
typename tbb_hm_wrapper<K,D,HF,AL>::iterator
tbb_hm_wrapper<K,D,HF,AL>::end()
{
    return iterator();
}
#endif // TBB_HM_WRAPPER
