/*******************************************************************************
 * data-structures/markableelement.h
 *
 * MarkableElements represent the cells of a table, that has to be able to mark
 * a copied cell (used in uaGrow and paGrow). They encapsulate some CAS and
 * update methods.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef MARKABLEELEMENT_H
#define MARKABLEELEMENT_H

#include <stdlib.h>
#include <cstdint>
#include <functional>
#include <limits>

#include <xmmintrin.h>

#include "data-structures/returnelement.h"

#ifndef ICPC
#include <xmmintrin.h>
using int128_t = __int128;
#else
using int128_t = __int128_t;
#endif

namespace growt {

class MarkableElement
{
public:
    using key_type    = std::uint64_t;
    using mapped_type = std::uint64_t;
    using value_type  = std::pair<const key_type, mapped_type>;

    MarkableElement();
    MarkableElement(const key_type& k, const mapped_type& d);
    MarkableElement(const value_type& pair);
    MarkableElement(const MarkableElement& e);
    MarkableElement & operator=(const MarkableElement& e);
    MarkableElement(MarkableElement &&e);
    //MarkableElement & operator=(MarkableElement && e);

    static MarkableElement get_empty()
    { return MarkableElement( 0, 0 ); }

    key_type    key;
    mapped_type data;

    bool is_empty()   const;
    bool is_deleted() const;
    bool is_marked()  const;
    bool compare_key(const key_type & k) const;
    bool atomic_mark(MarkableElement& expected);
    key_type    get_key()  const;
    mapped_type get_data() const;
    bool set_data(const mapped_type);

    bool cas(      MarkableElement & expected,
             const MarkableElement & desired);

    bool atomic_delete(const MarkableElement & expected);

    template<class F>
    bool atomic_update(      MarkableElement & expected,
                       const MarkableElement & desired,
                            F f);
    template<class F>
    bool non_atomic_update(  MarkableElement & expected,
                       const MarkableElement & desired,
                             F f);

    template<class F, class ...Types>
    std::pair<mapped_type, bool> atomic_update(   MarkableElement & expected,
                         F f, Types&& ... args);
    template<class F, class ...Types>
    std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ... args);

    inline bool operator==(MarkableElement& r) { return (key == r.key); }
    inline bool operator!=(MarkableElement& r) { return (key != r.key); }

    // inline ReturnElement get_return() const
    // {  return ReturnElement(get_key(), get_data());  }

    // inline operator ReturnElement()
    // {  return ReturnElement(get_key(), get_data());  }

    inline operator value_type() const
    {  return std::make_pair(get_key(), get_data()); }

private:
    int128_t       &as128i();
    const int128_t &as128i() const;

    static const unsigned long long BITMASK    = (1ull << 63) -1;
    static const unsigned long long MARKED_BIT =  1ull << 63;
};




inline MarkableElement::MarkableElement() { }
inline MarkableElement::MarkableElement(const key_type& k, const mapped_type& d) : key(k), data(d) { }
inline MarkableElement::MarkableElement(const value_type& p) : key(p.first), data(p.second) { }

// Roman used: _mm_loadu_ps Think about using
// _mm_load_ps because the memory should be aligned
inline MarkableElement::MarkableElement(const MarkableElement &e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
}

inline MarkableElement & MarkableElement::operator=(const MarkableElement & e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
    return *this;
}

inline MarkableElement::MarkableElement(MarkableElement &&e)
    : key(e.key), data(e.data) { }

// inline MarkableElement & MarkableElement::operator=(MarkableElement &&e)
// {
//     key  = e.key;
//     data = e.data;
//     return *this;
// }


inline bool MarkableElement::is_empty()   const { return (key & BITMASK) == 0; }
inline bool MarkableElement::is_deleted() const { return (key & BITMASK) == BITMASK; }
inline bool MarkableElement::is_marked()  const { return (key & MARKED_BIT); }
inline bool MarkableElement::compare_key(const key_type & k) const
{ return (key & BITMASK) == k; }
inline MarkableElement::key_type    MarkableElement::get_key()  const
{ return (key != BITMASK) ? (key & BITMASK) : 0; }
inline MarkableElement::mapped_type MarkableElement::get_data() const { return data; }
inline bool MarkableElement::set_data(const mapped_type d)
{
    MarkableElement temp = *this;
    if (temp.is_marked()) return false;
    return __sync_bool_compare_and_swap_16(& as128i(), temp.as128i(),
                                           MarkableElement(temp.key, d).as128i());
}

inline bool MarkableElement::atomic_mark(MarkableElement& expected)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                               expected.as128i(),
                               (expected.as128i() | MARKED_BIT));
}

inline bool MarkableElement::cas( MarkableElement & expected,
                            const MarkableElement & desired)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           desired.as128i());
}

inline bool MarkableElement::atomic_delete(const MarkableElement & expected)
{
    auto temp = expected;
    temp.key = BITMASK;
    auto result =__sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           temp.as128i());
    return result;
}

inline int128_t       & MarkableElement::as128i()
{ return *reinterpret_cast<__int128 *>(this); }

inline const int128_t &MarkableElement::as128i() const
{ return *reinterpret_cast<const __int128 *>(this); }




template<class F>
inline bool MarkableElement::atomic_update(MarkableElement & expected,
                                     const MarkableElement & desired,
                                           F f)
{
    mapped_type td = expected.data;
    f(td, desired.key, desired.data);
    return cas(expected, MarkableElement(desired.key, td));

}

template<class F>
inline bool MarkableElement::non_atomic_update(MarkableElement &,
                                         const MarkableElement & desired,
                                               F f)
{
    f(data, desired.key, desired.data);
    return true;
}

template<class F, class ...Types>
inline std::pair<typename MarkableElement::mapped_type, bool>
MarkableElement::atomic_update(MarkableElement &exp,
                              F f, Types&& ... args)
{
    auto temp = exp.get_data();
    f(temp, std::forward<Types>(args)...);
    return std::make_pair(temp, cas(exp, MarkableElement(exp.key, temp)));

}

template<class F, class ...Types>
inline std::pair<typename MarkableElement::mapped_type, bool>
MarkableElement::non_atomic_update(F f, Types&& ... args)
{
    return std::make_pair(f(data, std::forward<Types>(args)...),
                          true);
}

}

#endif // MARKABLEELEMENT_H
