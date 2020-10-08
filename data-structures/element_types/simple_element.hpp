/*******************************************************************************
 * data-structures/element_types/simple_element.hpp
 *
 * simple_elements represent cells of our hash table data-structure. They
 * encapsulate some CAS and update methods.
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

#ifndef ICPC
#include <xmmintrin.h>
using int128_t = __int128;
#else
using int128_t = __int128_t;
#endif

#include "data-structures/returnelement.hpp"

namespace growt {

class simple_element
{
public:
    using key_type    = std::uint64_t;
    using mapped_type = std::uint64_t;
    using value_type  = std::pair<const key_type, mapped_type>;

    static constexpr bool allows_marking               = false;
    static constexpr bool allows_deletions             = true;
    static constexpr bool allows_atomic_updates        = true;
    static constexpr bool allows_updates               = true;
    static constexpr bool allows_referential_integrity = false;
    static constexpr bool needs_cleanup                = false;

    simple_element();
    simple_element(const key_type& k, const mapped_type& d);
    simple_element(const value_type& pair);
    simple_element(const simple_element &e);
    simple_element & operator=(const simple_element & e);
    simple_element(simple_element &&e);

    key_type    get_key()  const;
    mapped_type get_data() const;
    static simple_element get_empty() { return simple_element(0,0); }

    bool is_empty() const;
    bool is_deleted() const;
    bool is_marked() const;
    bool compare_key(const key_type & k) const;

    bool atomic_mark(simple_element& expected);
    bool cas(simple_element & expected, const simple_element & desired);
    bool atomic_delete(const simple_element & expected);

    template<class F, class ...Types>
    std::pair<mapped_type, bool> atomic_update(simple_element & expected, F f, Types&& ...args);
    template<class F, class ...Types>
    std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ...args);

    inline bool operator==(simple_element& other) { return (key == other.key); }
    inline bool operator!=(simple_element& other) { return (key != other.key); }

    inline operator value_type()      const { return std::make_pair(get_key(), get_data()); }

private:
    key_type    key;
    mapped_type data;

    int128_t       &as128i();
    const int128_t &as128i() const;

    const static unsigned long long BITMASK    = ( (1ull << 63) -1 );
    const static unsigned long long MARKED_BIT =   (1ull << 63);

    template <bool> friend class TAtomic;
};


inline simple_element::simple_element()
{ }
inline simple_element::simple_element(const key_type& k, const mapped_type& d)
    : key(k), data(d) { }
inline simple_element::simple_element(const value_type& p)
    : key(p.first), data(p.second) { }
inline simple_element::simple_element(const simple_element &e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
}
inline simple_element & simple_element::operator=(const simple_element & e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
    return *this;
}
inline simple_element::simple_element(simple_element &&e)
    : key(e.key), data(e.data) { }


inline simple_element::key_type    simple_element::get_key()  const { return key;  }
inline simple_element::mapped_type simple_element::get_data() const { return data; }

inline bool simple_element::is_empty()   const { return key == 0; }
inline bool simple_element::is_deleted() const { return key == BITMASK; }
inline bool simple_element::is_marked()  const { return false; }
inline bool simple_element::compare_key(const key_type & k)  const { return key == k; }
inline bool simple_element::atomic_mark(simple_element&)            { return true; }



inline bool simple_element::cas(simple_element & expected,
                  const simple_element & desired)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           desired.as128i());
}

inline bool simple_element::atomic_delete(const simple_element & expected)
{
    auto temp = expected;
    temp.key = BITMASK;
    return __sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           temp.as128i());
}






// UPDATE FUNCTIONS PRECEDED BY SOME TEMPLATE MAGIC, WHICH CHECKS IF AN
// ATOMIC VARIANT IS DEFINED.

// TESTS IF A GIVEN OBJECT HAS A FUNCTION NAMED atomic
template <typename TFunctor>
class THasAtomic
{
    typedef char one;
    typedef long two;

    template <typename C> static one test( decltype(&C::atomic) ) ;
    template <typename C> static two test(...);

public:
    enum { value = sizeof(test<TFunctor>(0)) == sizeof(char) };
};

// USED IF f.atomic(...) EXISTS
template <bool TTrue>
struct TAtomic
{
    template <class TFunctor>
    inline static bool execute(simple_element& that, simple_element&,
                         const simple_element& desired, TFunctor f)
    {
        f.atomic(that.data, desired.key, desired.data);
        return true;
    }

    template <class F, class ...Types>
    inline static std::pair<typename simple_element::mapped_type, bool>
    execute(simple_element& that, simple_element&, F f, Types ... args)
    {
        simple_element::mapped_type temp = f.atomic(that.data, std::forward<Types>(args)...);
        return std::make_pair(temp, true);
    }
};


// USED OTHERWISE
template <>
struct TAtomic<false>
{
    template <class TFunctor>
    inline static bool execute(simple_element& that, simple_element& exp,
                         const simple_element& des, TFunctor f)
    {
        simple_element::mapped_type td = exp.data;
        f(td, des.key, des.data);
        return __sync_bool_compare_and_swap(&(that.data),
                                            exp.data,
                                            td);
    }

    template <class F, class ...Types>
    inline static std::pair<typename simple_element::mapped_type, bool>
    execute(simple_element& that, simple_element& exp, F f, Types ... args)
    {
        simple_element::mapped_type td = exp.data;
        f(td, std::forward<Types>(args)...);
        bool succ = __sync_bool_compare_and_swap(&(that.data),
                                            exp.data,
                                            td);
        return std::make_pair(td, succ);
    }
};

template<class F, class ... Types>
inline std::pair<typename simple_element::mapped_type, bool>
simple_element::atomic_update(simple_element & expected,
                             F f, Types&& ... args)
{
    return TAtomic<THasAtomic<F>::value>::execute
        (*this, expected, f, std::forward<Types>(args)...);
}



template<class F, class ... Types>
inline std::pair<typename simple_element::mapped_type, bool>
simple_element::non_atomic_update(F f, Types&& ... args)
{
    return std::make_pair(f(data, std::forward<Types>(args)...),
                          true);
}



inline int128_t       & simple_element::as128i()
{ return *reinterpret_cast<int128_t *>(this); }

inline const int128_t & simple_element::as128i() const
{ return *reinterpret_cast<const int128_t *>(this); }

}
