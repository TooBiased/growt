/*******************************************************************************
 * data-structures/simpleelement.h
 *
 * SimpleElements represent cells of our hash table data-structure. They
 * encapsulate some CAS and update methods.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef SIMPLEELEMENT_H
#define SIMPLEELEMENT_H


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

#include "data-structures/returnelement.h"

namespace growt {

class SimpleElement
{
public:
    using key_type    = std::uint64_t;
    using mapped_type = std::uint64_t;
    using value_type  = std::pair<const key_type, mapped_type>;

    SimpleElement();
    SimpleElement(const key_type& k, const mapped_type& d);
    SimpleElement(const value_type& pair);
    SimpleElement(const SimpleElement &e);
    SimpleElement & operator=(const SimpleElement & e);
    SimpleElement(SimpleElement &&e);

    static SimpleElement get_empty()
    { return SimpleElement( 0, 0 ); }

    static SimpleElement get_deleted()
    { return SimpleElement( (1ull<<63)-1ull, 0 ); }

    key_type    key;
    mapped_type data;

    bool is_empty() const;
    bool is_deleted() const;
    bool is_marked() const;
    bool compare_key(const key_type & k) const;
    bool atomic_mark(SimpleElement& expected);
    key_type    get_key()  const;
    mapped_type get_data() const;
    bool set_data(const mapped_type);

    bool cas(SimpleElement & expected,
       const SimpleElement & desired);

    bool atomic_delete(const SimpleElement & expected);

    template<class F>
    bool atomic_update(SimpleElement & expected,
                 const SimpleElement & desired,
                       F f);
    template<class F>
    bool non_atomic_update(SimpleElement & expected,
                     const SimpleElement & desired,
                           F f);

    template<class F, class ...Types>
    std::pair<mapped_type, bool> atomic_update(SimpleElement & expected,
                                               F f, Types&& ...args);
    template<class F, class ...Types>
    std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ...args);

    inline bool operator==(SimpleElement& other) { return (key == other.key); }
    inline bool operator!=(SimpleElement& other) { return (key != other.key); }

    // inline ReturnElement get_return() const { return ReturnElement (get_key(), get_data()); }
    // inline operator ReturnElement()         { return ReturnElement (get_key(), get_data()); }
    inline operator value_type()      const { return std::make_pair(get_key(), get_data()); }

private:
    int128_t       &as128i();
    const int128_t &as128i() const;

    const static unsigned long long BITMASK    = ( (1ull << 63) -1 );
    const static unsigned long long MARKED_BIT =   (1ull << 63);
};


inline SimpleElement::SimpleElement() { }
inline SimpleElement::SimpleElement(const key_type& k, const mapped_type& d)
    : key(k), data(d) { }
inline SimpleElement::SimpleElement(const value_type& p)
    : key(p.first), data(p.second) { }
inline SimpleElement::SimpleElement(const SimpleElement &e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
}
inline SimpleElement & SimpleElement::operator=(const SimpleElement & e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
    return *this;
}
inline SimpleElement::SimpleElement(SimpleElement &&e)
    : key(e.key), data(e.data) { }



inline bool SimpleElement::is_empty()   const { return key == 0; }
inline bool SimpleElement::is_deleted() const { return key == BITMASK; }
inline bool SimpleElement::is_marked()  const { return false; }
inline bool SimpleElement::compare_key(const key_type & k)  const { return key == k; }
inline bool SimpleElement::atomic_mark(SimpleElement&)            { return true; }

inline SimpleElement::key_type    SimpleElement::get_key()  const { return key;  }
inline SimpleElement::mapped_type SimpleElement::get_data() const { return data; }

    inline bool SimpleElement::set_data(const mapped_type d)
{
    data = d;
    return true;
}



inline bool SimpleElement::cas(SimpleElement & expected,
                  const SimpleElement & desired)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           desired.as128i());
}

inline bool SimpleElement::atomic_delete(const SimpleElement & expected)
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
    inline static bool execute(SimpleElement& that, SimpleElement&,
                         const SimpleElement& desired, TFunctor f)
    {
        f.atomic(that.data, desired.key, desired.data);
        return true;
    }

    template <class F, class ...Types>
    inline static std::pair<typename SimpleElement::mapped_type, bool>
    execute(SimpleElement& that, SimpleElement&, F f, Types ... args)
    {
        SimpleElement::mapped_type temp = f.atomic(that.data, std::forward<Types>(args)...);
        return std::make_pair(temp, true);
    }
};


// USED OTHERWISE
template <>
struct TAtomic<false>
{
    template <class TFunctor>
    inline static bool execute(SimpleElement& that, SimpleElement& exp,
                         const SimpleElement& des, TFunctor f)
    {
        SimpleElement::mapped_type td = exp.data;
        f(td, des.key, des.data);
        return __sync_bool_compare_and_swap(&(that.data),
                                            exp.data,
                                            td);
    }

    template <class F, class ...Types>
    inline static std::pair<typename SimpleElement::mapped_type, bool>
    execute(SimpleElement& that, SimpleElement& exp, F f, Types ... args)
    {
        SimpleElement::mapped_type td = exp.data;
        f(td, std::forward<Types>(args)...);
        bool succ = __sync_bool_compare_and_swap(&(that.data),
                                            exp.data,
                                            td);
        return std::make_pair(td, succ);
    }
};


template<class F>
inline bool SimpleElement::atomic_update(SimpleElement & expected,
                                   const SimpleElement & desired,
                                         F f)
{
    return TAtomic<THasAtomic<F>::value>::execute(*this, expected, desired, f);
}



template<class F>
inline bool SimpleElement::non_atomic_update(SimpleElement &,
                                       const SimpleElement & desired,
                                             F f)
{
    f(data, desired.key, desired.data);
    return true;
}

template<class F, class ... Types>
inline std::pair<typename SimpleElement::mapped_type, bool>
SimpleElement::atomic_update(SimpleElement & expected,
                             F f, Types&& ... args)
{
    return TAtomic<THasAtomic<F>::value>::execute
        (*this, expected, f, std::forward<Types>(args)...);
}



template<class F, class ... Types>
inline std::pair<typename SimpleElement::mapped_type, bool>
SimpleElement::non_atomic_update(F f, Types&& ... args)
{
    return std::make_pair(f(data, std::forward<Types>(args)...),
                          true);
}



inline int128_t       & SimpleElement::as128i()
{ return *reinterpret_cast<int128_t *>(this); }

inline const int128_t & SimpleElement::as128i() const
{ return *reinterpret_cast<const int128_t *>(this); }

}

#endif // SIMPLEELEMENT_H
