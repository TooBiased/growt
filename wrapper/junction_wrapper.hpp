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

#include "data-structures/hash_table_mods.hpp"
#include "data-structures/returnelement.h"
#include "wrapper/stupid_iterator.h"

using namespace growt;

size_t nextPowerOf2(size_t n)
{
    auto temp = 1u;
    while (n > temp) temp <<= 1;
    return temp;
}

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




template <class JunctionType>
class junction_handle
{
private:
    using internal_table_type = JunctionType
    // JUNCTION_TYPE<turf::u64, turf::u64, junction::DefaultKeyTraits<turf::u64>, ValueTraits>;

    internal_table_type& hash;
    int count;
    junction::QSBR::Context qsbrContext;

public:
    //static std::mutex registration_mutex;

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<key_type, mapped_type>;
    using insert_return_type = std::pair<iterator, bool>;

    junction_handle() = delete;
    junction_handle(internal_table_type& hash_table);
    ~junction_handle();

    junction_handle(const junction_handle&) = delete;
    junction_handle& operator=(const junction_handle&) = delete;

    junction_handle(junction_handle&& rhs) = default;
    junction_handle& operator=(junction_handle&& rhs);

    inline iterator           find(const key_type& k)
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



template <class JunctionType>
class junction_wrapper
{
public:
    using internal_table_type = JunctionType;
//
    using handle_type = junction_handle<internal_table_type>;

    junction_wrapper(size_t capacity)
        : hash(new internal_table_type(nextPowerOf2(capacity)<<1)) { }
    ~junction_wrapper() { }
    junction_wrapper(const junction_wrapper&) = delete;
    junction_wrapper& operator=(const junction_wrapper&) = delete;
    junction_wrapper(junction_wrapper&& rhs) = default;
    junction_wrapper& operator=(junction_wrapper&& rhs) = default;

    handle_type get_handle() { return handle_type(*hash); }

private:
    std::unique_ptr<internal_table_type> hash;
    friend handle_type;
};




template <class Key, class Data, class HashFct, class Allocator, hmod ... Mods>
class junction_config
{
public:
    using key_type = Key;
    using mapped_type = Data;
    using hash_fct_type = HashFct;
    using allocator_type = Allocator;

    using mods = mod_aggregator<Mods ...>;

    // Derived Types
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr is_viable = !mods::template is<ref_integrity>()
        && std::is_same<Key , size_t>::value
        && std::is_same<Data, size_t>::value;

    static_assert(is_viable, "folly wrapper does not support the chosen flags");

    using internal_table_type = JUNCTION_TYPE<turf::u64, turf::u64,
                                              junction::DefaultKeyTraits<turf::u64>,
                                              ValueTraits>;

    using table_type = folly_wrapper<key_type, mapped_type,
                                     hash_fct_type, allocator_type>;
};





template <class JT>
junction_handle<JT>::junction_handle(internal_table_type& hash_table)
    : hash(hash_table), count(0)
{
    //std::lock_guard<std::mutex> lock(registration_mutex);
    qsbrContext = junction::DefaultQSBR.createContext();
}

template <class JT>
junction_handle<JT>::~junction_handle()
{
    //std::lock_guard<std::mutex> lock(junction_mutex);
    junction::DefaultQSBR.destroyContext(qsbrContext);
}

template <class JT>
typename junction_handle<JT>&
junction_handle<JT>::operator=(junction_handle&& rhs)
{
    if (&rhs == this) return *this;
    this->~junction_handle();
    new (this) junction_handle(std::move(rhs));
    return *this;
}

template <class JT>
typename junction_handle<JT>::iterator
junction_handle<JT>::find(const key_type& k)
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

template <class JT>
typename junction_handle<JT>::insert_return_type
junction_handle<JT>::insert(const key_type& k, const mapped_type& d)
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

template <class JT> template<class F, class ... Types>
typename junction_handle<JT>::insert_return_type
junction_handle<JT>::update(const key_type& k, F f, Types&& ... args)
{
    //static_assert(F::junction_compatible::value, //TJuncComp<F>::value,
    //              "Used update function is not Junction compatible!");
    if constexpr (! F::junction_compatible::value)
        return insert_return_type(end(), false);

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

template <class JT> template<class F, class ... Types>
typename junction_handle<JT>::insert_return_type
junction_handle<JT>::insert_or_update(const key_type& k,
                                      const mapped_type& d,
                                      F f, Types&& ... args)
{
    //static_assert(F::junction_compatible::value, //TJuncComp<F>::value,
    //              "Used update function is not Junction compatible!");

    if constexpr (! F::junction_compatible::value)
        return insert_return_type(end(), false);

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

template <class JT> template<class F, class ... Types>
typename junction_handle<JT>::insert_return_type
junction_handle<JT>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    return update(k,f,std::forward<Types>(args)...);
}

template <class JT> template<class F, class ... Types>
typename junction_handle<JT>::insert_return_type
junction_handle<JT>::insert_or_update_unsafe(const key_type& k,
                                             const mapped_type& d,
                                             F f, Types&& ... args)
{
    return insert_or_update(k,d,f,std::forward<Types>(args)...);
}

template <class JT>
size_t
junction_handle<JT>::erase(const key_type& k)
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

template <class JT>
typename junction_handle<JT>::iterator
junction_handle<JT>::end() { return iterator(); }


#endif // JUNCTION_WRAPPER
