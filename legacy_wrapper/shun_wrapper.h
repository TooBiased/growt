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

#include "ndHash.h"
#include "data-structures/simpleelement.h"

#include "wrapper/stupid_iterator.h"

using namespace growt;

template<class Hasher>
class ShunWrapper
{
private:
    using uint64 = u_int64_t;
    using uint = unsigned int;
    using ll = long long;

    template <typename HasherInside>
    struct HashCellWrapper
    {
        using eType = SimpleElement;
        using kType = ll;

        inline eType empty()
        { return SimpleElement::getEmpty(); }

        inline kType getKey(eType v)
        { return v.getKey(); }

        inline uintT hash(kType s)
        { return (HasherInside()(s)); }

        inline int cmp(kType v, kType b)
        { return ((v > b) ? 1 : ((v == b) ? 0 : -1)); }

        //NOTICE: 1 MEANS THAT ALL INSERTIONS ARE ASSIGNS 0 WOULD MAKE ALL ASSIGNS INSERTIONS
        inline bool  replaceQ(eType s, eType s2)
        { return (s.getKey() == s2.getKey()); }
    };

    using HashType = Table<HashCellWrapper<Hasher>, ll>;

    HashType hash;
    size_t capacity;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<size_t, size_t>;
    using insert_return_type = std::pair<iterator, bool>;

    ShunWrapper() = default;
    ShunWrapper(size_t capacity_) : hash(capacity_, HashCellWrapper<Hasher>()), capacity(capacity_) {}
    ShunWrapper(const ShunWrapper&) = delete;
    ShunWrapper& operator=(const ShunWrapper&) = delete;

    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    ShunWrapper(ShunWrapper&& rhs) : hash(rhs.capacity, HashCellWrapper<Hasher>())
    { }

    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    ShunWrapper& operator=(ShunWrapper&& rhs)
    {
        capacity = rhs.capacity;
        (& hash)->~HashType();
        new (& hash) HashType(rhs.capacity, HashCellWrapper<Hasher>());
        return *this;
    }


    using Handle = ShunWrapper&;
    Handle getHandle() { return *this; }


    inline iterator find(const key_type& k)
    {
        auto temp = hash.find(k);
        if (temp.isEmpty()) return end();
        else return iterator(k,temp.data);
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
    {
        bool res  = hash.insert(SimpleElement(k,d));
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

#endif // SHUN_WRAPPER
