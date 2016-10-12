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
    typedef uint64_t Key;
    typedef uint64_t Data;

    MarkableElement();
    MarkableElement(Key k, Data d);
    MarkableElement(const MarkableElement &e);
    MarkableElement & operator=(const MarkableElement & e);
    MarkableElement(MarkableElement &&e);

    static MarkableElement getEmpty()
    { return MarkableElement( 0, 0 ); }

    static MarkableElement getDeleted()
    { return MarkableElement( (1ull<<63)-1ull, 0 ); }

    Key  key;
    Data data;

    bool isEmpty() const;
    bool isDeleted() const;
    bool isMarked() const;
    bool compareKey(const Key & k) const;
    bool atomicMark(MarkableElement& expected);
    Key  getKey() const;
    Data getData() const;
    bool CAS(      MarkableElement & expected,
             const MarkableElement & desired);

    bool atomicDelete(const MarkableElement & expected);

    template<class F>
    bool atomicUpdate(      MarkableElement & expected,
                      const MarkableElement & desired,
                            F f);
    template<class F>
    bool nonAtomicUpdate(   MarkableElement & expected,
                      const MarkableElement & desired,
                            F f);


    inline ReturnElement getReturn() const
    {  return ReturnElement(getKey(), getData());  }

    inline operator ReturnElement()
    {  return ReturnElement(getKey(), getData());  }
private:
    int128_t       &as128i();
    const int128_t &as128i() const;

    static const unsigned long long BITMASK    = (1ull << 63) -1;
    static const unsigned long long MARKED_BIT =  1ull << 63;
};




inline MarkableElement::MarkableElement() { }
inline MarkableElement::MarkableElement(Key k, Data d) : key(k), data(d) { }


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



inline bool MarkableElement::isEmpty() const   { return (key & BITMASK) == 0; }
inline bool MarkableElement::isDeleted() const { return (key & BITMASK) == BITMASK; }
inline bool MarkableElement::isMarked() const  { return (key & MARKED_BIT); }
inline bool MarkableElement::compareKey(const Key & k) const { return (key & BITMASK) == k; }
inline MarkableElement::Key  MarkableElement::getKey() const { return (key != BITMASK) ? (key & BITMASK) : 0; }
inline MarkableElement::Data MarkableElement::getData() const { return data; }


inline bool MarkableElement::atomicMark(MarkableElement& expected)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                               expected.as128i(),
                               (expected.as128i() | MARKED_BIT));
}

inline bool MarkableElement::CAS( MarkableElement & expected,
                            const MarkableElement & desired)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           desired.as128i());
}

bool MarkableElement::atomicDelete(const MarkableElement & expected)
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
inline bool MarkableElement::atomicUpdate(MarkableElement & expected,
                                    const MarkableElement & desired,
                                          F f)
{
    Data td = expected.data;
    f(td, desired.key, desired.data);
    return CAS(expected, MarkableElement(desired.key, td));

}

template<class F>
inline bool MarkableElement::nonAtomicUpdate(MarkableElement &,
                                       const MarkableElement & desired,
                                             F f)
{
    f(data, desired.key, desired.data);
    return true;
}

}

#endif // MARKABLEELEMENT_H
