/*******************************************************************************
 * data-structures/circular.h
 *
 * Non growing table variant, that is also used by our growing
 * tables to represent the current table. Compared to Circular, this
 * Implementation replaces atomics with TSX transactions for some better
 * performance.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef TSXCIRCULAR_H
#define TSXCIRCULAR_H

#include <stdlib.h>
#include <functional>
#include <atomic>
#include <stdexcept>

//#include <rtm.h>
#include <immintrin.h>

#include "data-structures/returnelement.h"

namespace growt {

template<class E, typename HashFct = std::hash<typename E::Key>,
         class A = std::allocator<E>,
         size_t MaDis = 128, size_t MiSt = 200>
class TSXCircular
{
private:
    using This_t      = TSXCircular<E,HashFct,A,MaDis,MiSt>;
    using Allocator_t = typename A::template rebind<E>::other;

    template <class> friend class GrowTableHandle;

public:
    using InternElement_t = E;
    using Element         = ReturnElement;
    using Key             = typename InternElement_t::Key;
    using Data            = typename InternElement_t::Data;
    using Handle          = This_t&;
    Handle getHandle() { return *this; }

    TSXCircular(size_t size_ = 1<<18);
    TSXCircular(size_t size_, size_t version_);
    TSXCircular(const TSXCircular&) = delete;
    TSXCircular& operator=(const TSXCircular&) = delete;
    TSXCircular(TSXCircular&& rhs);
    TSXCircular& operator=(TSXCircular&& rhs);

    ~TSXCircular();

    ReturnCode insert               (const Key k, const Data d);
    template <class F>
    ReturnCode update               (const Key k, const Data d, F f);
    template <class F>
    ReturnCode insertOrUpdate       (const Key k, const Data d, F f);
    template <class F>
    ReturnCode update_unsafe        (const Key k, const Data d, F f);
    template <class F>
    ReturnCode insertOrUpdate_unsafe(const Key k, const Data d, F f);

    ReturnCode remove(const Key& k);
    ReturnElement find  (const Key& k);

    size_t migrate(This_t& target, size_t s, size_t e);

    size_t size;
    size_t version;
    std::atomic<size_t> currentCopyBlock;

    static size_t resize(size_t current, size_t inserted, size_t deleted)
    {
        auto size = current;
        double fill_rate = double(inserted - deleted)/double(current);

        if (fill_rate > 0.6/2.) size <<= 1;

        return size;
    }

protected:
    Allocator_t allocator;
    static_assert(std::is_same<typename Allocator_t::value_type, InternElement_t>::value,
                  "Wrong allocator type given to TSXCircular!");

    size_t bitmask;
    size_t right_shift;
    HashFct hash;

    InternElement_t* t;
    size_t h(const Key & k) const { return hash(k) >> right_shift; }

private:
    ReturnCode insert               (const InternElement_t& e);
    template <class F>
    ReturnCode update               (const InternElement_t& e, F f);
    template <class F>
    ReturnCode insertOrUpdate       (const InternElement_t& e, F f);
    template <class F>
    ReturnCode update_unsafe        (const InternElement_t& e, F f);
    template <class F>
    ReturnCode insertOrUpdate_unsafe(const InternElement_t& e, F f);

    ReturnCode non_atomic_insert    (const InternElement_t& e);
    template <class F>
    ReturnCode non_atomic_update    (const InternElement_t& e, F f);
    template <class F>
    ReturnCode non_atomic_insertOrUpdate(const InternElement_t& e, F f);

    ReturnCode atomic_insert        (const InternElement_t& e);
    template <class F>
    ReturnCode atomic_update        (const InternElement_t& e, F f);
    template <class F>
    ReturnCode atomic_insertOrUpdate(const InternElement_t& e, F f);
    template <class F>
    ReturnCode atomic_update_unsafe (const InternElement_t& e, F f);
    template <class F>
    ReturnCode atomic_insertOrUpdate_unsafe(const InternElement_t& e, F f);

    // ReturnCode non_atomic_remove    (const Key& k); // Turned out to be unsafe
    ReturnCode atomic_remove        (const Key& k);

    ReturnElement base_find         (const Key& k);

    void insert_unsave              (const InternElement_t& e);

    static size_t compute_size(size_t desired_capacity)
    {
        auto temp = 4096u;
        while (temp < desired_capacity*(MiSt/100.)) temp <<= 1;
        return temp;
    }

    static size_t compute_right_shift(size_t size)
    {
        size_t log_size = 0;
        while (size >>= 1) log_size++;
        return HashFct::significant_digits - log_size;
    }
};



template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E, HashFct, A, MaDis, MiSt>::TSXCircular(size_t size_)
    : size(compute_size(size_)),
      version(0),
      currentCopyBlock(0),
      bitmask(size-1),
      right_shift(compute_right_shift(size))
{
    t = allocator.allocate(size);
    if ( !t ) std::bad_alloc();

    std::fill( t ,t + size , E::getEmpty() );
}

/*should always be called with a size_=2^k  */
template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E, HashFct, A, MaDis, MiSt>::TSXCircular(size_t size_, size_t version_)
    : size(size_),
      version(version_),
      currentCopyBlock(0),
      bitmask(size-1),
      right_shift(compute_right_shift(size))
{
    t = allocator.allocate(size);
    if ( !t ) std::bad_alloc();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E, HashFct, A, MaDis, MiSt>::~TSXCircular()
{
    allocator.deallocate(t, size);
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E,HashFct,A,MaDis,MiSt>::TSXCircular(TSXCircular&& rhs)
    : size(rhs.size), version(rhs.version),
      currentCopyBlock(rhs.currentCopyBlock.load()),
      bitmask(rhs.bitmask), right_shift(rhs.right_shift), t(nullptr)
{
    if (currentCopyBlock.load()) std::invalid_argument("Cannot move a growing table!");
    rhs.size = 0;
    rhs.bitmask = 0;
    rhs.right_shift = HashFct::significant_digits;
    std::swap(t, rhs.t);
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E,HashFct,A,MaDis,MiSt>& TSXCircular<E,HashFct,A,MaDis,MiSt>::operator=(TSXCircular&& rhs)
{
    if (currentCopyBlock.load()) std::invalid_argument("Cannot move a growing table!");
    size    = rhs.size;
    version = rhs.version;
    currentCopyBlock.store(0);;
    bitmask = rhs.bitmask;
    right_shift = rhs.right_shift;
    rhs.size = 0;
    rhs.bitmask = 0;
    rhs.right_shift = HashFct::significant_digits;
    std::swap(t, rhs.t);

    return *this;
}



template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::non_atomic_insert(const InternElement_t& e)
{
    const Key k = e.getKey();

    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);

        if (curr.isMarked()   )      return ReturnCode::TSX_UNSUCCESS_INVALID;
        else if (curr.compareKey(k)) return ReturnCode::TSX_UNSUCCESS_ALREADY_USED; // already hashed
        else if (curr.isEmpty())
        {
            //This is not threadsave (but only called with tsx)
            t[temp] = e;
            return ReturnCode::TSX_SUCCESS_IN;
        }
        if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return ReturnCode::TSX_UNSUCCESS_FULL;
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::atomic_insert(const InternElement_t& e)
{
    const Key k = e.getKey();

    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isMarked()   ) return ReturnCode::UNSUCCESS_INVALID;
        else if (curr.compareKey(k)) return ReturnCode::UNSUCCESS_ALREADY_USED; // already hashed
        else if (curr.isEmpty())
        {
            if ( t[temp].CAS(curr, e) ) return ReturnCode::SUCCESS_IN;
            //somebody changed the current element! recheck it
            --i;
        }
        if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return ReturnCode::UNSUCCESS_FULL;
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::insert(const Key k, const Data d)
{   return insert(InternElement_t(k,d));   }

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::insert(const InternElement_t& e)
{
    size_t status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_insert(e);
        _xend();

        return temp;
    }

    return atomic_insert(e);
}



template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::non_atomic_update(const InternElement_t& e, F f)
{
    const Key k = e.getKey();

    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isMarked()) return ReturnCode::TSX_UNSUCCESS_INVALID;
        else if (curr.compareKey(k))
        {
            //not threadsafe => use TSX
            t[temp].nonAtomicUpdate(curr, e, f);
            return ReturnCode::TSX_SUCCESS_UP;
        }
        else if (curr.isEmpty())
        {
            return ReturnCode::TSX_UNSUCCESS_NOT_FOUND;
            //not threadsafe => use TSX
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return ReturnCode::TSX_UNSUCCESS_NOT_FOUND;
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::atomic_update(const InternElement_t& e, F f)
{
    const Key k = e.getKey();

    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isMarked())
        {
            return ReturnCode::UNSUCCESS_INVALID;
        }
        if (curr.compareKey(k))
        {
            if (t[temp].atomicUpdate(curr, e, f))
                return ReturnCode::SUCCESS_UP;
            i--;
        }
        else if (curr.isEmpty())
        {
            return ReturnCode::UNSUCCESS_NOT_FOUND;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return ReturnCode::UNSUCCESS_NOT_FOUND;
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::update(const Key k, const Data d, F f)
{   return update(InternElement_t(k,d), f);   }


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::update(const InternElement_t& e, F f)
{
    auto status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_update(e, f);
        _xend();
        return temp;
    }
    return atomic_update(e, f);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::atomic_update_unsafe(const InternElement_t& e, F f)
{
    const Key k = e.getKey();

    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isMarked())
        {
            return ReturnCode::UNSUCCESS_INVALID;
        }
        if (curr.compareKey(k))
        {
            if (t[temp].nonAtomicUpdate(curr, e, f))
                return ReturnCode::SUCCESS_UP;
            i--;
        }
        else if (curr.isEmpty())
        {
            return ReturnCode::UNSUCCESS_NOT_FOUND;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return ReturnCode::UNSUCCESS_NOT_FOUND;
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::update_unsafe(const Key k, const Data d, F f)
{   return update_unsafe(InternElement_t(k,d), f);   }


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::update_unsafe(const InternElement_t& e, F f)
{
    auto status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_update(e, f);
        _xend();
        return temp;
    }
    return atomic_update_unsafe(e, f);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::non_atomic_insertOrUpdate(const InternElement_t& e, F f)
{
    const Key k = e.getKey();

    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isMarked()) return ReturnCode::TSX_UNSUCCESS_INVALID;
        else if (curr.compareKey(k))
        {
            //not threadsafe => use TSX
            t[temp].nonAtomicUpdate(curr, e, f);
            return ReturnCode::TSX_SUCCESS_UP;
        }
        else if (curr.isEmpty())
        {
            //not threadsafe => use TSX
            t[temp] = e;
            return ReturnCode::TSX_SUCCESS_IN;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return ReturnCode::TSX_UNSUCCESS_FULL;
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::atomic_insertOrUpdate(const InternElement_t& e, F f)
{
    const Key k = e.getKey();

    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isMarked())
        {
            return ReturnCode::UNSUCCESS_INVALID;
        }
        if (curr.compareKey(k))
        {
            if (t[temp].atomicUpdate(curr, e, f))
                return ReturnCode::SUCCESS_UP;
            i--;
        }
        else if (curr.isEmpty())
        {
            if ( t[temp].CAS(curr, e) ) return ReturnCode::SUCCESS_IN;
            //somebody changed the current element! recheck it
            --i;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return ReturnCode::UNSUCCESS_FULL;
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::insertOrUpdate(const Key k, const Data d, F f)
{   return insertOrUpdate(InternElement_t(k,d), f);   }

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::insertOrUpdate(const InternElement_t& e, F f)
{
    auto status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_insertOrUpdate(e, f);
        _xend();
        return temp;
    }

    return atomic_insertOrUpdate(e, f);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::atomic_insertOrUpdate_unsafe(const InternElement_t& e, F f)
{
    const Key k = e.getKey();

    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isMarked())
        {
            return ReturnCode::UNSUCCESS_INVALID;
        }
        if (curr.compareKey(k))
        {
            if (t[temp].nonAtomicUpdate(curr, e, f))
                return ReturnCode::SUCCESS_UP;
            i--;
        }
        else if (curr.isEmpty())
        {
            if ( t[temp].CAS(curr, e) ) return ReturnCode::SUCCESS_IN;
            //somebody changed the current element! recheck it
            --i;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    //throw std::bad_alloc();
    return ReturnCode::UNSUCCESS_FULL;
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::insertOrUpdate_unsafe(const Key k, const Data d, F f)
{   return insertOrUpdate_unsafe(InternElement_t(k,d), f);   }

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::insertOrUpdate_unsafe(const InternElement_t& e, F f)
{
    auto status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_insertOrUpdate(e, f);
        _xend();
        return temp;
    }

    return atomic_insertOrUpdate_unsafe(e, f);
}



template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnElement TSXCircular<E, HashFct, A, MaDis, MiSt>::base_find  (const Key& k)
{
    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        E curr(t[i & bitmask]);
        if (curr.compareKey(k)) return curr;
        if (curr.isEmpty()) return E::getEmpty();
    }
    return ReturnElement::getEmpty();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnElement TSXCircular<E, HashFct, A, MaDis, MiSt>::find(const Key & k)
{
    // no need to use tsx since find never needed to be atomic
    // the answer is consistent even in case of a torn read
    return base_find(k);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::atomic_remove(const Key & k)
{
    size_t htemp = h(k);
    for (size_t i = htemp; i < htemp+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isMarked())
        {
            return ReturnCode::UNSUCCESS_INVALID;
        }
        else if (curr.compareKey(k))
        {
            if (t[temp].atomicDelete(curr))
                return ReturnCode::SUCCESS_DEL;
            i--;
        }
        else if (curr.isEmpty())
        {
            return ReturnCode::UNSUCCESS_NOT_FOUND;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }

    return ReturnCode::UNSUCCESS_NOT_FOUND;
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnCode TSXCircular<E, HashFct, A, MaDis, MiSt>::remove(const Key &k)
{
    return atomic_remove(k);
}




template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline size_t TSXCircular<E,HashFct,A,MaDis,MiSt>::migrate(This_t& target, size_t s, size_t e)
{
    size_t n = 0;
    auto i = s;
    auto curr = E::getEmpty();

    //HOW MUCH BIGGER IS THE TARGET TABLE
    auto shift = 0u;
    while (target.size > (size << shift)) ++shift;


    //FINDS THE FIRST EMPTY BUCKET (START OF IMPLICIT BLOCK)
    while (i<e)
    {
        curr = t[i];                    //no bitmask necessary (within one block)
        if (curr.isEmpty())
        {
            if (t[i].atomicMark(curr)) break;
            else --i;
        }
        ++i;
    }

    std::fill(target.t+(i<<shift), target.t+(e<<shift), E::getEmpty());

    //MIGRATE UNTIL THE END OF THE BLOCK
    for (; i<e; ++i)
    {
        curr = t[i];
        if (! t[i].atomicMark(curr))
        {
            --i;
            continue;
        }
        else if (! curr.isEmpty())
        {
            if (!curr.isDeleted())
            {
                target.insert_unsave(curr);
                ++n;
            }
        }
    }

    auto b = true; // b indicates, if t[i-1] was non-empty

    //CONTINUE UNTIL WE FIND AN EMPTY BUCKET
    //THE TARGET POSITIONS WILL NOT BE INITIALIZED
    for (; b; ++i)
    {
        auto pos  = i&bitmask;
        auto t_pos= pos<<shift;
        for (size_t j = 0; j < 1ull<<shift; ++j) target.t[t_pos+j] = E::getEmpty();
        //target.t[t_pos] = E::getEmpty();

        curr = t[pos];

        if (! t[pos].atomicMark(curr)) --i;
        if ( (b = ! curr.isEmpty()) ) // this might be nicer as an else if, but this is faster
        {
            if (!curr.isDeleted()) { target.insert_unsave(curr); n++; }
        }
    }

    return n;
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
void TSXCircular<E, HashFct, A, MaDis, MiSt>::insert_unsave(const E& e)
{
    const Key k = e.getKey();

    for (size_t i = h(k); i < h(k)+MaDis; ++i)
    {
        size_t temp = i & bitmask;
        E curr(t[temp]);
        if (curr.isEmpty() || curr.compareKey(k))
        {
            t[temp] = e;
            return;
        }
    }
    throw std::bad_alloc();
}

}

#endif // TSXCIRCULAR_H
