/*******************************************************************************
 * data-structures/element_types/markable_element.hpp
 *
 * markable_elements represent the cells of a table, that has to be able to mark
 * a copied cell (used in uaGrow and paGrow). They encapsulate some CAS and
 * update methods.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <stdlib.h>
#include <cstdint>
#include <functional>
#include <limits>

#include <xmmintrin.h>

#include "data-structures/returnelement.hpp"

#ifndef ICPC
#include <xmmintrin.h>
using int128_t = __int128;
#else
using int128_t = __int128_t;
#endif

namespace growt {

class markable_element
{
public:
    using key_type    = std::uint64_t;
    using mapped_type = std::uint64_t;
    using value_type  = std::pair<const key_type, mapped_type>;

    static constexpr bool allows_marking               = true;
    static constexpr bool allows_deletions             = true;
    static constexpr bool allows_atomic_updates        = false;
    static constexpr bool allows_updates               = true;
    static constexpr bool allows_referential_integrity = false;
    static constexpr bool needs_cleanup                = false;

    markable_element();
    markable_element(const key_type& k, const mapped_type& d);
    markable_element(const value_type& pair);
    markable_element(const markable_element& e);
    markable_element & operator=(const markable_element& e);
    markable_element(markable_element &&e);
    //markable_element& operator=(markable_element && e);

    key_type    get_key()  const;
    mapped_type get_data() const;
    static markable_element get_empty() { return markable_element(0,0); }

    bool is_empty()   const;
    bool is_deleted() const;
    bool is_marked()  const;
    bool compare_key(const key_type & k) const;

    bool atomic_mark(markable_element& expected);
    bool cas(markable_element & expected, const markable_element & desired);
    bool atomic_delete(const markable_element & expected);

    template<class F, class ...Types>
    std::pair<mapped_type, bool> atomic_update(   markable_element & expected, F f, Types&& ... args);
    template<class F, class ...Types>
    std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ... args);

    inline bool operator==(markable_element& r) { return (key == r.key); }
    inline bool operator!=(markable_element& r) { return (key != r.key); }

    inline operator value_type() const
    {  return std::make_pair(get_key(), get_data()); }

private:
    key_type    key;
    mapped_type data;

    int128_t       &as128i();
    const int128_t &as128i() const;

    static constexpr unsigned long long BITMASK    = (1ull << 63) -1;
    static constexpr unsigned long long MARKED_BIT =  1ull << 63;
};




inline markable_element::markable_element() { }
inline markable_element::markable_element(const key_type& k, const mapped_type& d) : key(k), data(d) { }
inline markable_element::markable_element(const value_type& p) : key(p.first), data(p.second) { }

// Roman used: _mm_loadu_ps Think about using
// _mm_load_ps because the memory should be aligned
inline markable_element::markable_element(const markable_element &e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
}

inline markable_element & markable_element::operator=(const markable_element & e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
    return *this;
}

inline markable_element::markable_element(markable_element &&e)
    : key(e.key), data(e.data) { }

// inline markable_element & markable_element::operator=(markable_element &&e)
// {
//     key  = e.key;
//     data = e.data;
//     return *this;
// }


inline bool markable_element::is_empty()   const { return (key & BITMASK) == 0; }
inline bool markable_element::is_deleted() const { return (key & BITMASK) == BITMASK; }
inline bool markable_element::is_marked()  const { return (key & MARKED_BIT); }
inline bool markable_element::compare_key(const key_type & k) const
{ return (key & BITMASK) == k; }
inline markable_element::key_type    markable_element::get_key()  const
{ return (key != BITMASK) ? (key & BITMASK) : 0; }
inline markable_element::mapped_type markable_element::get_data() const { return data; }

inline bool markable_element::atomic_mark(markable_element& expected)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                               expected.as128i(),
                               (expected.as128i() | MARKED_BIT));
}

inline bool markable_element::cas( markable_element & expected,
                            const markable_element & desired)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           desired.as128i());
}

inline bool markable_element::atomic_delete(const markable_element & expected)
{
    auto temp = expected;
    temp.key = BITMASK;
    auto result =__sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           temp.as128i());
    return result;
}

inline int128_t       & markable_element::as128i()
{ return *reinterpret_cast<__int128 *>(this); }

inline const int128_t &markable_element::as128i() const
{ return *reinterpret_cast<const __int128 *>(this); }



template<class F, class ...Types>
inline std::pair<typename markable_element::mapped_type, bool>
markable_element::atomic_update(markable_element &exp,
                              F f, Types&& ... args)
{
    auto temp = exp.get_data();
    f(temp, std::forward<Types>(args)...);
    return std::make_pair(temp, cas(exp, markable_element(exp.key, temp)));

}

template<class F, class ...Types>
inline std::pair<typename markable_element::mapped_type, bool>
markable_element::non_atomic_update(F f, Types&& ... args)
{
    return std::make_pair(f(data, std::forward<Types>(args)...),
                          true);
}

}
