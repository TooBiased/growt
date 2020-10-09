#pragma once

#include <stdlib.h>
#include <cstdint>
#include <functional>
#include <limits>
#include <tuple>

#ifndef ICPC
#include <xmmintrin.h>
using int128_t = __int128;
#else
using int128_t = __int128_t;
#endif

#include "data-structures/returnelement.hpp"

template <class Key, class Data, bool markable>
class simple_slot
{
private:
    static constexpr size_t marked_bit = 1<<63;
    static constexpr size_t bitmask    = marked_bit-1;

public:
    using key_type = Key;
    using mapped_type = Data;
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr bool allows_marking               = markable;
    static constexpr bool allows_deletions             = true;
    static constexpr bool allows_atomic_updates        = !markable;
    static constexpr bool allows_updates               = true;
    static constexpr bool allows_referential_integrity = false;
    static constexpr bool needs_cleanup                = false;

    static_assert(true, "some asserts about the inserted types")


    // THIS IS AFTER THE ELEMENT IS READ (i.e. consistency within one query)
    class slot_type
    {
    private:
        key_type key;
        mapped_type data;

    public:
        slot_type(const key_type& k, const mapped_type& d, size_t hash);
        slot_type(const value_type& pair, size_t hash);
        slot_type(key_type&& k, mapped_type&& d, size_t hash);
        slot_type(value_type&& pair, size_t hash);
        slot_type(const slot_type& source) = default;
        slot_type(slot_type&& source) = default;
        slot_type& operator=(const slot_type& source) = default;
        slot_type& operator=(slot_type&& source) = default;
        slot_type(__m128i source);

        inline key_type    get_key() const;
        inline mapped_type get_mapped() const;

        inline bool is_empty()   const;
        inline bool is_deleted() const;
        inline bool is_marked()  const;
        inline bool compare_key(const key_type & k, size_t hash) const;

        inline operator value_type() const;
        inline bool operator==(const simple_slot& r) const;
        inline bool operator!=(const simple_slot& r) const;
    };


    // THIS IS IN THE TABLE, IT ONLY HAS THE CAS+UPDATE STUFF
    class atomic_slot_type
    {
    private:
        __m128i _raw_data;

    public:
        slot_type load() const;
        bool cas(slot_type& expected, const slot_type& goal);
        bool atomic_delete(slot_type& expected);
        bool atomic_mark  (slot_type& expected);

        template<class F, class ...Types>
        std::pair<mapped_type, bool> atomic_update(simple_slot & expected,
                                                   F f, Types&& ... args);
        template<class F, class ...Types>
        std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ... args);
    };

    static slot_type   get_empty();
    static slot_type   get_deleted();
    static value_type* allocate();
    static void        deallocate(value_type* ptr);
};


// SLOT_TYPE *******************************************************************
// *** constructors ************************************************************
template <class K, class D, bool m>
simple_slot<K,D,m>::slot_type::slot_type(const key_type& k,
                                         const mapped_type& d,
                                         [[maybe_unused]]size_t hash)
    : key(k), data(d) { }

template <class K, class D, bool m>
simple_slot<K,D,m>::slot_type::slot_type(const value_type& pair,
                                         [[maybe_unused]]size_t hash)
    : key(pair.first), data(pair.second) { }

template <class K, class D, bool m>
simple_slot<K,D,m>::slot_type::slot_type(key_type&& k, mapped_type&& d,
                                         [[maybe_unused]]size_t hash)
    : key(k), data(d) { }

template <class K, class D, bool m>
simple_slot<K,D,m>::slot_type::slot_type(value_type&& pair,
                                         [[maybe_unused]]size_t hash)
    : key(pair.first), data(pair.second) { }

template <class K, class D, bool m>
simple_slot<K,D,m>::slot_type::slot_type(__m128i source)
{
    *reinterpret_cast<__m128i*>(this) = source;
}


// *** getter ******************************************************************
template <class K, class D, bool m>
typename simple_slot<K,D,m>::key_type
simple_slot<K,D,m>::slot_type::get_key() const
{
    if constexpr (!m) return key;
    return key & simple_slot::bitmask;
}

template <class K, class D, bool m>
typename simple_slot<K,D,m>::mapped_type
simple_slot<K,D,m>::slot_type::get_mapped() const
{
    return data;
}


// *** state *******************************************************************
template <class K, class D, bool m>
bool
simple_slot<K,D,m>::slot_type::is_empty() const
{
    return key == 0;
}

template <class K, class D, bool m>
bool
simple_slot<K,D,m>::slot_type::is_deleted() const
{
    return key == simple_slot::get_deleted().key
}

template <class K, class D, bool m>
bool
simple_slot<K,D,m>::slot_type::is_marked() const
{
    if constexpr (!m) return false;
    return key & marked_bit;
}

template <class K, class D, bool m>
bool
simple_slot<K,D,m>::slot_type::compare_key(const key& k,
                                           [[maybe_unused]]size_t hash) const
{
    return key == k;
    // if (!m) return key == k;
    // return (key & bitmask) == k;
}


// *** operators ***************************************************************
template <class K, class D, bool m>
simple_slot<K,D,m>::slot_type::operator value_type() const
{
    return std::make_pair(get_key(), get_data());
}

template <class K, class D, bool m>
bool
simple_slot<K,D,m>::slot_type::operator==(const slot_type& r) const
{
    return key == r.key;
}

template <class K, class D, bool m>
bool
simple_slot<K,D,m>::slot_type::operator!=(const slot_type& r) const
{
    return key != r.key;
}



// ATOMIC_SLOT_TYPE ************************************************************
// *** common atomics **********************************************************
template <class K, class D, bool m>
typename simple_slot<K,D,m>::slot_type
simple_slot<K,D,m>::atomic_slot_type::load() const
{
    // Roman used: _mm_loadu_ps Think about using
    // _mm_load_ps because the memory should be aligned

    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    auto temp = _mm_loadu_si128(&_raw_data);
    return slot_type(temp);
}

template <class K, class D, bool m>
bool
simple_slot<K,D,m>::atomic_slot_type::cas(slot_type& expected,
                                          const slot_type&  goal)
{
    return __sync_bool_compare_and_swap_16(&_raw_data,
                                           reinterpret_cast<__m128i&>(expected),
                                           reinterpret_cast<const __m128i&>(desired));
}

template <class K, class D, bool m>
bool
simple_slot<K,D,m>::atomic_slot_type::atomic_delete(slot_type& expected)
{
    return cas(expected, get_deleted());
}

template <class K, class D, bool m>
bool
simple_slot<K,D,m>::atomic_slot_type::atomic_mark(slot_type& expected)
{
    auto temp = expected;
    temp.key |= marked_bit;
    return cas(expected, temp);
}

// *** functor style updates ***************************************************
template<class F, class ...Types>
std::pair<typename simple_slot<K,D,m>::mapped_type, bool>
simple_slot<K,D,m>::atomic_slot_type::atomic_update(simple_slot & expected,
                                                    F f, Types&& ... args)
{
    // TODO not implemented
    if constexpr (! debug::debug_mode) return false;
    static std::atomic_bool once = true;
    if (once.load())
        debug::if_debug("non-atomic update is not implemented in complex types",
                        once.exchange(false));
}

template<class F, class ...Types>
std::pair<typename simple_slot<K,D,m>::mapped_type, bool>
simple_slot<K,D,m>::atomic_slot_type::non_atomic_update(F f, Types&& ... args);
{
    // TODO not implemented
    if constexpr (! debug::debug_mode) return false;
    static std::atomic_bool once = true;
    if (once.load())
        debug::if_debug("non-atomic update is not implemented in complex types",
                        once.exchange(false));
}
