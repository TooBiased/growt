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
    typedef uint64_t Key;
    typedef uint64_t Data;

    SimpleElement();
    SimpleElement(Key k, Data d);
    SimpleElement(const SimpleElement &e);
    SimpleElement & operator=(const SimpleElement & e);
    SimpleElement(const SimpleElement &&e);

    static SimpleElement getEmpty()
    { return SimpleElement( 0, 0 ); }

    static SimpleElement getDeleted()
    { return SimpleElement( (1ull<<63)-1ull, 0 ); }

    Key  getKey() const;
    Data getData() const;

    Key  key;
    Data data;

    bool isEmpty() const;
    bool isDeleted() const;
    bool isMarked() const;
    bool compareKey(const Key & k) const;
    bool atomicMark(SimpleElement& expected);

    bool CAS(      SimpleElement & expected,
             const SimpleElement & desired);

    bool atomicDelete(const SimpleElement & expected);

    template<class F>
    bool atomicUpdate(      SimpleElement & expected,
                      const SimpleElement & desired,
                            F f);
    template<class F>
    bool nonAtomicUpdate(   SimpleElement & expected,
                      const SimpleElement & desired,
                            F f);


    inline bool operator==(SimpleElement& other) { return (key == other.key); }
    inline bool operator!=(SimpleElement& other) { return (key != other.key); }

    inline ReturnElement getReturn() const {  return ReturnElement(getKey(), getData()); }
    inline operator ReturnElement() { return ReturnElement(getKey(), getData());  }

private:
    int128_t       &as128i();
    const int128_t &as128i() const;

    const static unsigned long long BITMASK    = ( (1ull << 63) -1 );
    const static unsigned long long MARKED_BIT =   (1ull << 63);
};


SimpleElement::SimpleElement() { }
SimpleElement::SimpleElement(Key k, Data d) : key(k), data(d) { }
SimpleElement::SimpleElement(const SimpleElement &&e) : key(e.key), data(e.data) { }


SimpleElement::SimpleElement(const SimpleElement &e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
}


SimpleElement & SimpleElement::operator=(const SimpleElement & e)
{
    //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
    as128i() = reinterpret_cast<int128_t>(_mm_loadu_si128((__m128i*) &e));
    return *this;
}







inline bool SimpleElement::isEmpty() const { return key == 0; }
inline bool SimpleElement::isDeleted() const { return key == BITMASK; }
inline bool SimpleElement::isMarked() const { return false; }
inline bool SimpleElement::compareKey(const Key & k) const { return key == k; }
inline bool SimpleElement::atomicMark(SimpleElement&) { return true; }
inline SimpleElement::Key SimpleElement::getKey() const { return key;  }
inline SimpleElement::Data SimpleElement::getData() const { return data; }



inline bool SimpleElement::CAS(SimpleElement & expected,
                  const SimpleElement & desired)
{
    return __sync_bool_compare_and_swap_16(& as128i(),
                                           expected.as128i(),
                                           desired.as128i());
}

inline bool SimpleElement::atomicDelete(const SimpleElement & expected)
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
    static bool execute(SimpleElement& that, SimpleElement&, const SimpleElement& desired, TFunctor f)
    {
        f.atomic(that.data, desired.key, desired.data);
        return true;
    }
};


// USED OTHERWISE
template <>
struct TAtomic<false>
{
    template <class TFunctor>
    static bool execute(SimpleElement& that, SimpleElement& expected, const SimpleElement& desired, TFunctor f)
    {
        SimpleElement::Data td = expected.data;
        f(td, desired.key, desired.data);
        return __sync_bool_compare_and_swap(&(that.data),
                                            expected.data,
                                            td);
    }
};


template<class F>
inline bool SimpleElement::atomicUpdate(SimpleElement & expected,
                           const SimpleElement & desired,
                                            F f)
{
    return TAtomic<THasAtomic<F>::value>::execute(*this, expected, desired, f);
}



template<class F>
inline bool SimpleElement::nonAtomicUpdate(SimpleElement &,
                              const SimpleElement & desired,
                                    F f)
{
    f(data, desired.key, desired.data);
    return true;
}




inline int128_t       & SimpleElement::as128i()
{ return *reinterpret_cast<int128_t *>(this); }

inline const int128_t & SimpleElement::as128i() const
{ return *reinterpret_cast<const int128_t *>(this); }

}

#endif // SIMPLEELEMENT_H
