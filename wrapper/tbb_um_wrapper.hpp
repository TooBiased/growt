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
#include "data-structures/hash_table_mod.hpp"

using namespace growt;

template<class Key, class Data, class Hasher class Alloc>
class tbb_um_wrapper
{
private:
    using HashType = tbb::concurrent_unordered_map<size_t, size_t, Hasher>;

    HashType hash;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = typename HashType::iterator;
    using const_iterator     = typename HashType::const_iterator;
    using insert_return_type = std::pair<iterator, bool>;

    tbb_um_wrapper() = default;
    tbb_um_wrapper(size_t capacity_) : hash(capacity_) { }
    tbb_um_wrapper(const tbb_um_wrapper&) = delete;
    tbb_um_wrapper& operator=(const tbb_um_wrapper&) = delete;

    tbb_um_wrapper(tbb_um_wrapper&& rhs) = default;
    tbb_um_wrapper& operator=(tbb_um_wrapper&& rhs) = default;

    using Handle = tbb_um_wrapper&;
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

    inline size_t erase(const key_type&);

    inline iterator        end()         { return hash.end();  }
    inline const_iterator  end()   const { return hash.cend(); }
    inline const_iterator cend()   const { return hash.cend(); }

    inline iterator        begin()       { return hash.begin();  }
    inline const_iterator  begin() const { return hash.cbegin(); }
    inline const_iterator cbegin() const { return hash.cbegin(); }
};


template <class Key, class Data, class HashFct, class Allocator, hmod ... Mods>
class tbb_um_config
{
public:
    using key_type = Key;
    using mapped_type = Data;
    using hash_fct_type = HashFct;
    using allocator_type = Allocator;

    using mods = mod_aggregator<Mods ...>;

    // Derived Types
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr is_viable = ! mods::is<hmod::deletion>();

    static_assert(is_viable, "tbb hm wrapper does not support the chosen flags");

    using table_type = tbb_hm_wrapper<key_type, mapped_type,
                                     hash_fct_type, allocator_type>;
};



template<class K, class D, class HF, class AL>
typename tbb_um_wrapper<K,D,HF,AL>::iterator
tbb_um_wrapper<K,D,HF,AL>::find(const key_type& k)
{
    return hash.find(k);
    // auto temp = hash.find(k);
    // if (temp == end(hash)) return ReturnElement::getEmpty();
    // else return ReturnElement(k, temp->second);
}

template<class K, class D, class HF, class AL>
typename tbb_um_wrapper<K,D,HF,AL>::insert_return_type
tbb_um_wrapper<K,D,HF,AL>::insert(const key_type& k, const mapped_type& d)
{
    return hash.insert(std::make_pair(k,d));
    // auto ret = hash.insert(std::make_pair(k,d)).second;
    // return (ret) ? ReturnCode::SUCCESS_IN : ReturnCode::UNSUCCESS_ALREADY_USED;
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename tbb_um_wrapper<K,D,HF,AL>::insert_return_type
tbb_um_wrapper<K,D,HF,AL>::update(const key_type& k, F f, Types&& ... args)
{
    auto result = hash.find(k);
    if (result != hash.end())
    {
        f.atomic(result->second, std::forward<Types>(args)...);
    }
    return std::make_pair(result, result != hash.end());
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename tbb_um_wrapper<K,D,HF,AL>::insert_return_type
tbb_um_wrapper<K,D,HF,AL>::insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    auto ret = hash.insert(std::make_pair(k,d));
    if (! ret.second)
    {
        f.atomic(ret.first->second, std::forward<Types>(args)...);
    }
    return ret;
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename tbb_um_wrapper<K,D,HF,AL>::insert_return_type
tbb_um_wrapper<K,D,HF,AL>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    return update(k,f, std::forward<Types>(args)...);
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename tbb_um_wrapper<K,D,HF,AL>::insert_return_type
tbb_um_wrapper<K,D,HF,AL>::insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    return insert_or_update(k,d,f, std::forward<Types>(args)...);
}

template<class K, class D, class HF, class AL>
size_t
tbb_um_wrapper<K,D,HF,AL>::erase(const key_type&)
{
    //static_assert(false, "tbb_um .remove(key) is not implemented")
    return 0;
}
#endif // TBB_UM_WRAPPER
