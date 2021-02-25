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

#include <string>

#include <libcuckoo/cuckoohash_map.hh>

#include "data-structures/hash_table_mods.hpp"
#include "data-structures/returnelement.hpp"
#include "wrapper/stupid_iterator.hpp"

using namespace growt;


template<class Key, class Data, class Hasher, class Alloc>
class cuckoo_wrapper
{
private:
    using HashType = libcuckoo::cuckoohash_map<Key, Data, Hasher>;

    HashType hash;
    size_t capacity;

public:
    static constexpr bool allows_deletions      = true;
    static constexpr bool allows_atomic_updates = false;
    static constexpr bool allows_updates        = true;
    static constexpr bool allows_referential_integrity = false;

    using key_type           = Key;
    using mapped_type        = Data;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<key_type, mapped_type>;
    using insert_return_type = std::pair<iterator, bool>;
    using emplace_return_type = std::pair<bool,bool>;

    cuckoo_wrapper() = default;
    cuckoo_wrapper(size_t capacity_) : hash(capacity_), capacity(capacity_) {}
    cuckoo_wrapper(const cuckoo_wrapper&) = delete;
    cuckoo_wrapper& operator=(const cuckoo_wrapper&) = delete;

    cuckoo_wrapper(cuckoo_wrapper&& rhs) : hash(rhs.capacity), capacity(rhs.capacity) {}
    cuckoo_wrapper& operator=(cuckoo_wrapper&& rhs)
    {
        capacity = rhs.capacity;

        (&hash)->~HashType();
        new (&hash) HashType(capacity);

        return *this;
    }

    using handle_type = cuckoo_wrapper&;
    handle_type get_handle() { return *this; }

    inline iterator find(const key_type& k);

    inline insert_return_type insert(const key_type& k, const mapped_type& d);

    template <class ... Args>
    inline emplace_return_type emplace(Args&& ... args);

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k,
                                     F f, Types&& ... args);
    template<class F, class ... Types>
    inline insert_return_type insert_or_update(const key_type& k,
                                               const mapped_type& d,
                                               F f, Types&& ... args);
    template<class F, class ... Types>
    inline emplace_return_type emplace_or_update(key_type&& k,
                                                mapped_type&& d,
                                                F f, Types&& ... args);
    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k,
                                            F f, Types&& ... args);
    template<class F, class ... Types>
    inline insert_return_type insert_or_update_unsafe(const key_type& k,
                                                      const mapped_type& d,
                                                      F f, Types&& ... args);
    inline size_t erase(const key_type& k);
    inline iterator end();

};

template <class Key, class Data, class HashFct, class Allocator, hmod ... Mods>
class cuckoo_config
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

    static_assert(is_viable, "libcuckoo wrapper does not support the chosen flags");

    using table_type = cuckoo_wrapper<key_type, mapped_type, hash_fct_type, allocator_type>;

    static std::string name() { return "cuckoo"; }
};




template<class K, class D, class HF, class AL>
typename cuckoo_wrapper<K,D,HF,AL>::iterator
cuckoo_wrapper<K,D,HF,AL>::find(const key_type& k)
{
    size_t temp = 0;
    hash.find(k,temp);
    return iterator((temp) ? k : 0, temp);
    //if (temp) return ReturnElement(k,temp);
    //else      return ReturnElement::getEmpty();
}

template<class K, class D, class HF, class AL>
typename cuckoo_wrapper<K,D,HF,AL>::insert_return_type
cuckoo_wrapper<K,D,HF,AL>::insert(const key_type& k, const mapped_type& d)
{
    bool res = hash.insert(k,d);
    return std::make_pair(iterator(k,d), res);
    // return (hash.insert(k,d)) ? ReturnCode::SUCCESS_IN :
    //                             ReturnCode::UNSUCCESS_ALREADY_USED;
}

template<class K, class D, class HF, class AL> template<class ... Args>
typename cuckoo_wrapper<K,D,HF,AL>::emplace_return_type
cuckoo_wrapper<K,D,HF,AL>::emplace(Args&& ... args)
{
    bool res = hash.insert(std::forward<Args>(args)...);
    return std::make_pair(res, res);
    // return (hash.insert(k,d)) ? ReturnCode::SUCCESS_IN :
    //                             ReturnCode::UNSUCCESS_ALREADY_USED;
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename cuckoo_wrapper<K,D,HF,AL>::insert_return_type
cuckoo_wrapper<K,D,HF,AL>::update(const key_type& k, F f, Types&& ... args)
{
    bool res = hash.update_fn(k, [k,&f,&args ...](mapped_type& d){ f(d, std::forward<Types>(args)...); });
    return std::make_pair(iterator(k,mapped_type()), res);
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename cuckoo_wrapper<K,D,HF,AL>::insert_return_type
cuckoo_wrapper<K,D,HF,AL>::insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    // no differentiation between update and insert
    // args1 = std::forward<Types>(args)...
    auto res = hash.upsert(k, [&f, &args...](mapped_type& v){ f(v, std::forward<Types>(args)...); }, d);
    return std::make_pair(iterator(k,d), true);
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename cuckoo_wrapper<K,D,HF,AL>::emplace_return_type
cuckoo_wrapper<K,D,HF,AL>::emplace_or_update(key_type&& k, mapped_type&& d,
                                             F f, Types&& ... args)
{
    // no differentiation between update and insert
    // args1 = std::forward<Types>(args)...
    auto res = hash.upsert(std::move(k),
                           [&f, &args...](mapped_type& v)
                           { f(v, std::forward<Types>(args)...); },
                           std::move(d));
    return std::make_pair(res, res);
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename cuckoo_wrapper<K,D,HF,AL>::insert_return_type
cuckoo_wrapper<K,D,HF,AL>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    return update(k,f, std::forward<Types>(args)...);
}

template<class K, class D, class HF, class AL> template<class F, class ... Types>
typename cuckoo_wrapper<K,D,HF,AL>::insert_return_type
cuckoo_wrapper<K,D,HF,AL>::insert_or_update_unsafe(const key_type& k,
                                                const mapped_type& d,
                                                F f, Types&& ... args)
{
    return insert_or_update(k,d,f, std::forward<Types>(args)...);
}

template<class K, class D, class HF, class AL>
size_t
cuckoo_wrapper<K,D,HF,AL>::erase(const key_type& k)
{ return hash.erase(k); }

template<class K, class D, class HF, class AL>
typename cuckoo_wrapper<K,D,HF,AL>::iterator
cuckoo_wrapper<K,D,HF,AL>::end()
{ return iterator(); }

#endif // CUCKOO_WRAPPER
