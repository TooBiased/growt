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
#include "data-structures/returnelement.h"
#include <algorithm>

using namespace growt;

template<class Hasher>
class FollyWrapper
{
private:
    using HashType = folly::AtomicHashMap<size_t, size_t, Hasher>;

    HashType hash;
    size_t capacity;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = typename HashType::iterator;
    using const_iterator     = typename HashType::const_iterator;
    using insert_return_type = std::pair<iterator, bool>;

    static constexpr size_t next_cap(size_t i)
    {
	    size_t curr = 4096;
	    while (curr<i) curr<<=1;
	    return curr << 1;
    }
    
    FollyWrapper() = default;
    FollyWrapper(size_t capacity_) 
	    : hash(next_cap(capacity_)), capacity(next_cap(capacity_)) {}
    FollyWrapper(const FollyWrapper&) = delete;
    FollyWrapper& operator=(const FollyWrapper&) = delete;

    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    FollyWrapper(FollyWrapper&& rhs) : hash(rhs.capacity), capacity(rhs.capacity)
    { }

    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    FollyWrapper& operator=(FollyWrapper&& rhs)
    {
        capacity = rhs.capacity;
        (& hash)->~HashType();
        new (& hash) HashType(rhs.capacity);
        return *this;
    }


    using Handle = FollyWrapper&;
    Handle get_handle() { return *this; }


    inline iterator find(const key_type& k)
    {
        return hash.find(k);
        // auto ret = hash.find(k);
        // if (ret == hash.end()) return ReturnElement::getEmpty();
        // else return ReturnElement(k, ret->second);
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
    {
        return hash.insert(k,d);
        // auto ret = hash.insert(std::make_pair(k,d));
        // if (ret.second) return ReturnCode::SUCCESS_IN;
        // else return ReturnCode::UNSUCCESS_ALREADY_USED;
    }

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k, F f, Types&& ... args)
    {
        auto result = hash.find(k);
        if (result != hash.end())
        {
            f.atomic(result->second, std::forward<Types>(args)...);
        }
        return std::make_pair(result, result != hash.end());
    }

    template<class F, class ... Types>
    inline insert_return_type insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        auto ret = hash.insert(std::make_pair(k,d));
        if (! ret.second)
        {
            f.atomic(ret.first->second, std::forward<Types>(args)...);
        }
        return ret;
    }

    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k, F f, Types&& ... args)
    {
        return update(k,f, std::forward<Types>(args)...);
    }

    template<class F, class ... Types>
    inline insert_return_type insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        return insert_or_update(k,d,f, std::forward<Types>(args)...);
    }

    inline size_t erase(const key_type& k)
    {
        return hash.erase(k);
    }

    inline iterator        end()         { return hash.end();  }
    inline const_iterator  end()   const { return hash.cend(); }
    inline const_iterator cend()   const { return hash.cend(); }

    inline iterator        begin()       { return hash.begin();  }
    inline const_iterator  begin() const { return hash.cbegin(); }
    inline const_iterator cbegin() const { return hash.cbegin(); }
};

#endif // FOLLY_WRAPPER
