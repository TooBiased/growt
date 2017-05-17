/*******************************************************************************
 * data-structures/circular.h
 *
 * Non growing table variant, that is also used by our growing
 * tables to represent the current table.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef CIRCULAR
#define CIRCULAR

#include <stdlib.h>
#include <functional>
#include <atomic>
#include <stdexcept>

#include "data-structures/returnelement.h"
#include "data-structures/iterator_base.h"

namespace growt {

template<class E, class HashFct = std::hash<typename E::key_type>,
         class A = std::allocator<E>,
         size_t MaDis = 128, size_t MiSt = 200>
class BaseCircular
{
private:
    using This_t          = BaseCircular<E,HashFct,A,MaDis,MiSt>;
    using Allocator_t     = typename A::template rebind<E>::other;

    template <class> friend class GrowTableHandle;

public:
    using value_intern       = E;

    using key_type           = typename value_intern::key_type;
    using mapped_type        = typename value_intern::mapped_type;
    using value_type         = E;//typename std::pair<const key_type, mapped_type>;
    using iterator           = IteratorBase<This_t, false>;//E*;
    using const_iterator     =    void;
    using size_type          = size_t;
    using difference_type    = std::ptrdiff_t;
    using reference          =    void;
    using const_reference    =    void;
    using insert_return_type = std::pair<iterator, ReturnCode>;

    using local_iterator       = void;
    using const_local_iterator = void;
    using node_type            = void;

    // Handle and get Handle are used for our tests using them
    // They are here so BaseCircular behaves like GrowTable
    //using Handle  = This_t&;
    //Handle getHandle() { return *this; }

    BaseCircular(size_type size_ = 1<<18);
    BaseCircular(size_type size_, size_type version_);

    BaseCircular(const BaseCircular&) = delete;
    BaseCircular& operator=(const BaseCircular&) = delete;

    // Obviously move-constructor and move-assignment are not thread safe
    // They are merely comfort functions used during setup
    BaseCircular(BaseCircular&& rhs);
    BaseCircular& operator=(BaseCircular&& rhs);

    ~BaseCircular();

    insert_return_type insert(const key_type k, const mapped_type d);
    template <class F>
    insert_return_type update(const key_type k, const mapped_type d, F f);
    template <class F>
    insert_return_type insertOrUpdate(const key_type k, const mapped_type d, F f);
    template <class F>
    insert_return_type update_unsafe(const key_type k, const mapped_type d, F f);
    template <class F>
    insert_return_type insertOrUpdate_unsafe(const key_type k, const mapped_type d, F f);

    ReturnCode erase(const key_type& k);
    //ReturnElement
    iterator find  (const key_type& k);

    size_type migrate(This_t& target, size_type s, size_type e);

    size_type capacity;
    size_type version;
    std::atomic_size_t currentCopyBlock;

    static size_type resize(size_type current, size_type inserted, size_type deleted)
    {
        auto nsize = current;
        double fill_rate = double(inserted - deleted)/double(current);

        if (fill_rate > 0.6/2.) nsize <<= 1;

        return nsize;
    }

    inline iterator end() { return iterator(std::make_pair(key_type(), mapped_type()), nullptr, nullptr); }

protected:
    Allocator_t allocator;
    static_assert(std::is_same<typename Allocator_t::value_type, value_intern>::value,
                  "Wrong allocator type given to BaseCircular!");

    size_type bitmask;
    size_type right_shift;
    HashFct hash;

    value_intern* t;
    size_type h(const key_type & k) const { return hash(k) >> right_shift; }

private:
    inline iterator           makeIterator (const key_type& k, const mapped_type& d,
                                            value_intern* ptr)
    { return iterator(std::make_pair(k,d), ptr, t+capacity); }
    inline insert_return_type makeInsertRet(const key_type& k, const mapped_type& d,
                                            value_intern* ptr, ReturnCode code)
    { return std::make_pair(makeIterator(k,d, ptr), code); }
    inline insert_return_type makeInsertRet(iterator it, ReturnCode code)
    { return std::make_pair(it, code); }

    // insert_return_type insert(const value_intern& e);
    // template <class F>
    // insert_return_type update(const value_intern& e, F f);
    // template <class F>
    // insert_return_type insertOrUpdate(const value_intern& e, F f);
    // template <class F>
    // insert_return_type update_unsafe(const value_intern& e, F f);
    // template <class F>
    // insert_return_type insertOrUpdate_unsafe(const value_intern& e, F f);

    void insert_unsafe(const value_intern& e);

    static size_type compute_capacity(size_type desired_capacity)
    {
        auto temp = 4096u;
        while (temp < desired_capacity*(MiSt/100.)) temp <<= 1;
        return temp;
    }

    static size_type compute_right_shift(size_type capacity)
    {
        size_type log_size = 0;
        while (capacity >>= 1) log_size++;
        return HashFct::significant_digits - log_size;
    }
};



template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>::BaseCircular(size_type capacity_)
    : capacity(compute_capacity(capacity_)),
      version(0),
      currentCopyBlock(0),
      bitmask(capacity-1),
      right_shift(compute_right_shift(capacity))

{
    t = allocator.allocate(capacity);
    if ( !t ) std::bad_alloc();

    std::fill( t ,t + capacity , value_intern::getEmpty() );
}

/*should always be called with a capacity_=2^k  */
template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>::BaseCircular(size_type capacity_, size_type version_)
    : capacity(capacity_),
      version(version_),
      currentCopyBlock(0),
      bitmask(capacity-1),
      right_shift(compute_right_shift(capacity))
{
    t = allocator.allocate(capacity);
    if ( !t ) std::bad_alloc();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>::~BaseCircular()
{
    if (t) allocator.deallocate(t, capacity);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>::BaseCircular(BaseCircular&& rhs)
    : capacity(rhs.capacity), version(rhs.version),
      currentCopyBlock(rhs.currentCopyBlock.load()),
      bitmask(rhs.bitmask), right_shift(rhs.right_shift), t(nullptr)
{
    if (currentCopyBlock.load()) std::invalid_argument("Cannot move a growing table!");
    rhs.capacity = 0;
    rhs.bitmask = 0;
    rhs.right_shift = HashFct::significant_digits;
    std::swap(t, rhs.t);
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>&
BaseCircular<E,HashFct,A,MaDis,MiSt>::operator=(BaseCircular&& rhs)
{
    if (rhs.currentCopyBlock.load()) std::invalid_argument("Cannot move a growing table!");
    capacity        = rhs.capacity;
    version     = rhs.version;
    currentCopyBlock.store(0);;
    bitmask     = rhs.bitmask;
    right_shift = rhs.right_shift;
    rhs.capacity    = 0;
    rhs.bitmask = 0;
    rhs.right_shift = HashFct::significant_digits;
    std::swap(t, rhs.t);

    return *this;
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::insert(const key_type k, const mapped_type d)
{
    size_type htemp = h(k);

    for (size_type i = htemp; i < htemp+MaDis; ++i)
    {
        size_type temp = i & bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked()   ) return makeInsertRet(end() , ReturnCode::UNSUCCESS_INVALID);
        else if (curr.compareKey(k)) return makeInsertRet(k, curr.getData(), &t[temp],ReturnCode::UNSUCCESS_ALREADY_USED);
        else if (curr.isEmpty())
        {
            if ( t[temp].CAS(curr, value_intern(k,d)) ) return makeInsertRet(k,d, &t[temp], ReturnCode::SUCCESS_IN);
            //somebody changed the current element! recheck it
            --i;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return makeInsertRet(end(), ReturnCode::UNSUCCESS_FULL);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::update(const key_type k, const mapped_type d, F f)
{
    size_type htemp = h(k);

    for (size_type i = htemp; i < htemp+MaDis; ++i)
    {
        size_type temp = i & bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            if (t[temp].atomicUpdate(curr, value_intern(k,d), f))
                return makeInsertRet(k,d, &t[temp], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_NOT_FOUND);
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return makeInsertRet(end(), ReturnCode::UNSUCCESS_NOT_FOUND);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::insertOrUpdate(const key_type k, const mapped_type d, F f)
{
    size_type htemp = h(k);

    for (size_type i = htemp; i < htemp+MaDis; ++i)
    {
        size_type temp = i & bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            if (t[temp].atomicUpdate(curr, value_intern(k,d), f))
                return makeInsertRet(k,d, &t[temp], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            if ( t[temp].CAS(curr, value_intern(k,d)) ) return makeInsertRet(k,d, &t[temp], ReturnCode::SUCCESS_IN);
            //somebody changed the current element! recheck it
            --i;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return makeInsertRet(end(), ReturnCode::UNSUCCESS_FULL);
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::update_unsafe(const key_type k, const mapped_type d, F f)
{
    size_type htemp = h(k);

    for (size_type i = htemp; i < htemp+MaDis; ++i)
    {
        size_type temp = i & bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            if (t[temp].nonAtomicUpdate(curr, value_intern(k,d), f))
                return makeInsertRet(k,d, &t[temp], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_NOT_FOUND);
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return makeInsertRet(end(), ReturnCode::UNSUCCESS_NOT_FOUND);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::insertOrUpdate_unsafe(const key_type k, const mapped_type d, F f)
{
    size_type htemp = h(k);

    for (size_type i = htemp; i < htemp+MaDis; ++i)
    {
        size_type temp = i & bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            if (t[temp].nonAtomicUpdate(curr, value_intern(k,d), f))
                return makeInsertRet(k,d, &t[temp], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            if ( t[temp].CAS(curr, value_intern(k,d)) ) return makeInsertRet(k,d, &t[temp], ReturnCode::SUCCESS_IN);
            //somebody changed the current element! recheck it
            --i;
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    return makeInsertRet(end(), ReturnCode::UNSUCCESS_FULL);
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline ReturnCode BaseCircular<E,HashFct,A,MaDis,MiSt>::erase(const key_type & k)
{
    size_type htemp = h(k);
    for (size_type i = htemp; i < htemp+MaDis; ++i)
    {
        size_type temp = i & bitmask;
        value_intern curr(t[temp]);
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
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::find(const key_type & k)
{
    size_type htemp = h(k);
    for (size_type i = htemp; i < htemp+MaDis; ++i)
    {
        value_intern curr(t[i & bitmask]);
        if (curr.compareKey(k)) return makeIterator(k, curr.getData(), &t[i & bitmask]); // curr;
        if (curr.isEmpty()) return end(); // ReturnElement::getEmpty();
    }
    return end(); // ReturnElement::getEmpty();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::size_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::migrate(This_t& target, size_type s, size_type e)
{
    size_type n = 0;
    auto i = s;
    auto curr = value_intern::getEmpty();

    //HOW MUCH BIGGER IS THE TARGET TABLE
    auto shift = 0u;
    while (target.capacity > (capacity << shift)) ++shift;


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

    std::fill(target.t+(i<<shift), target.t+(e<<shift), value_intern::getEmpty());

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
                target.insert_unsafe(curr);
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
        for (size_type j = 0; j < 1ull<<shift; ++j) target.t[t_pos+j] = value_intern::getEmpty();
        //target.t[t_pos] = E::getEmpty();

        curr = t[pos];

        if (! t[pos].atomicMark(curr)) --i;
        if ( (b = ! curr.isEmpty()) ) // this might be nicer as an else if, but this is faster
        {
            if (!curr.isDeleted()) { target.insert_unsafe(curr); n++; }
        }
    }

    return n;
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline void BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_unsafe(const value_intern& e)
{
    const key_type k = e.getKey();

    size_type htemp = h(k);
    for (size_type i = htemp; i < htemp+MaDis; ++i)
    {
        size_type temp = i & bitmask;
        value_intern curr(t[temp]);
        if (curr.isEmpty())
        {
            t[temp] = e;
            return;
        }
    }
    throw std::bad_alloc();
}

}
#endif // CIRCULAR
