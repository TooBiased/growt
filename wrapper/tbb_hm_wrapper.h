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


template<class Hasher>
class TBBHMWrapper
{
private:
    using HashType = tbb::concurrent_hash_map<size_t, size_t, HashCompareClass<Hasher> >;
    using Accessor = typename HashType::accessor;

    HashType hash;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<key_type, mapped_type>;
    using insert_return_type = std::pair<iterator, bool>;

    TBBHMWrapper() = default;
    TBBHMWrapper(size_t capacity_) : hash(capacity_) { }
    TBBHMWrapper(const TBBHMWrapper&) = delete;
    TBBHMWrapper& operator=(const TBBHMWrapper&) = delete;

    TBBHMWrapper(TBBHMWrapper&& rhs) = default;
    TBBHMWrapper& operator=(TBBHMWrapper&& rhs) = default;

    using Handle = TBBHMWrapper&;
    Handle getHandle() { return *this; }


    inline iterator find(const key_type& k)
    {
        Accessor a;
        if (hash.find(a,k)) return iterator(k,a->second);
        return end();
        // if (hash.find(a, k))
        //     return ReturnElement(k, a->second);
        // else
        //     return ReturnElement::getEmpty();
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
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

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k, F f, Types&& ... args)
    {
        Accessor a;
        if (hash.find(a, k))
        {
            auto temp = f(a->second, std::forward<Types>(args)...);
            return std::make_pair(iterator(k, temp),true);//ReturnCode::SUCCESS_UP;
        }
        else return std::make_pair(end(),false);//ReturnCode::UNSUCCESS_NOT_FOUND;
    }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
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

    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k, F f, Types&& ... args)
    {
        return update(k,f, std::forward<Types>(args)...);
    }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        return insertOrUpdate(k,d,f, std::forward<Types>(args)...);
    }

    inline size_t erase(const key_type& k)
    {
        return hash.erase(k);
    }

    inline iterator end()
    {
        return iterator();
    }
};

#endif // TBB_HM_WRAPPER
