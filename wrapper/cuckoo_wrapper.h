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

#include <libcuckoo/cuckoohash_map.hh>
#include "data-structures/returnelement.h"
#include "wrapper/stupid_iterator.h"

using namespace growt;

template<class Hasher>
class CuckooWrapper
{
private:
    using HashType = cuckoohash_map<size_t, size_t, Hasher>;

    HashType hash;
    size_t capacity;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<key_type, mapped_type>;
    using insert_return_type = std::pair<iterator, bool>;

    CuckooWrapper() = default;
    CuckooWrapper(size_t capacity_) : hash(capacity_), capacity(capacity_) {}
    CuckooWrapper(const CuckooWrapper&) = delete;
    CuckooWrapper& operator=(const CuckooWrapper&) = delete;

    CuckooWrapper(CuckooWrapper&& rhs) : hash(rhs.capacity), capacity(rhs.capacity) {}
    CuckooWrapper& operator=(CuckooWrapper&& rhs)
    {
        capacity = rhs.capacity;

        (&hash)->~HashType();
        new (&hash) HashType(capacity);

        return *this;
    }


    using Handle = CuckooWrapper&;
    Handle getHandle() { return *this; }


    inline iterator find(const key_type& k)
    {
        size_t temp = 0;
        hash.find(k,temp);
        return iterator((temp) ? k : 0, temp);
        //if (temp) return ReturnElement(k,temp);
        //else      return ReturnElement::getEmpty();
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
    {
        bool res = hash.insert(k,d);
        return std::make_pair(iterator(k,d), res);
        // return (hash.insert(k,d)) ? ReturnCode::SUCCESS_IN :
        //                             ReturnCode::UNSUCCESS_ALREADY_USED;
    }

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k, F f, Types&& ... args)
    {
        bool res = hash.update_fn(k, [k,&f,&args ...](mapped_type& d){ f(d, std::forward<Types>(args)...); });
        return std::make_pair(iterator(k,mapped_type()), res);
    }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        // no differentiation between update and insert
        // args1 = std::forward<Types>(args)...
        hash.upsert(k, [k,&f, &args...](mapped_type& v){ f(v, std::forward<Types>(args)...); }, d);
        return std::make_pair(iterator(k,d), true);
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

    inline iterator end() { return iterator(); }

};

#endif // CUCKOO_WRAPPER
