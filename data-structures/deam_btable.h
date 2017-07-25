/*******************************************************************************
 * data-structures/deam_btable.h
 *
 * base table implementation for the deamortized variant
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2017 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include <stdlib.h>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <memory>
#include <iostream>

#include "data-structures/returnelement.h"
#include "data-structures/deam_biterator.h"
#include "example/update_fcts.h"

namespace growt {

template<class>
class DeamTableData;

template<class E, class HashFct = std::hash<typename E::key_type>,
         class A = std::allocator<E> >
class BaseDeam
{
private:
    using This_t          = BaseDeam<E,HashFct,A>;
    using Allocator_t     = typename A::template rebind<E>::other;

    template <class> friend class DeamTableHandle;

public:
    using value_intern       = E;

    using key_type           = typename value_intern::key_type;
    using mapped_type        = typename value_intern::mapped_type;
    using value_type         = E;//typename std::pair<const key_type, mapped_type>;
    using iterator           = IteratorDeam<This_t, false>;//E*;
    using const_iterator     = IteratorDeam<This_t, true>;
    using size_type          = size_t;
    using difference_type    = std::ptrdiff_t;
    using reference          = ReferenceBase<This_t, false>;
    using const_reference    = ReferenceBase<This_t, true>;
    using mapped_reference       = MappedRefBase<This_t, false>;
    using const_mapped_reference = MappedRefBase<This_t, true>;
    using insert_return_type = std::pair<iterator, bool>;


    using local_iterator       = void;
    using const_local_iterator = void;
    using node_type            = void;

    using Handle             = This_t&;
private:
    using insert_return_intern = std::pair<iterator, ReturnCode>;

    friend iterator;
    friend const_iterator;

    template<class>
    friend class DeamTableData;

public:
    BaseDeam(size_type size_ = 1<<18);

    BaseDeam(const BaseDeam&) = delete;
    BaseDeam& operator=(const BaseDeam&) = delete;

    // Obviously move-constructor and move-assignment are not thread safe
    // They are merely comfort functions used during setup
    BaseDeam(BaseDeam&& rhs);
    BaseDeam& operator=(BaseDeam&& rhs);

    ~BaseDeam();

    Handle getHandle() { return *this; }

    iterator       begin();
    iterator       end();
    const_iterator cbegin() const;
    const_iterator cend()   const;
    const_iterator begin()  const { return cbegin(); }
    const_iterator end()    const { return cend();   }

    insert_return_type insert(const key_type& k, const mapped_type& d);
    size_type          erase (const key_type& k);
    iterator           find  (const key_type& k);
    const_iterator     find  (const key_type& k) const;

    insert_return_type insert_or_assign(const key_type& k, const mapped_type& d)
    { return insertOrUpdate(k, d, example::Overwrite(), d); }

    mapped_reference operator[](const key_type& k)
    { return (*insert(k, mapped_type())).second; }

    template <class F, class ... Types>
    insert_return_type update
    (const key_type& k, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_type update_unsafe
    (const key_type& k, F f, Types&& ... args);


    template <class F, class ... Types>
    insert_return_type insertOrUpdate
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_type insertOrUpdate_unsafe
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);


    void grow();
    //size_type migrate(This_t& target, size_type s, size_type e);

private:
    static constexpr size_type max_capacity = 1 << 30;
    static constexpr size_type block_grow   = 4096;
    static constexpr size_type over_grow    = 512;
    static constexpr double    init_factor  = 3.0;

protected:
    Allocator_t allocator;
    static_assert(std::is_same<typename Allocator_t::value_type, value_intern>::value,
                  "Wrong allocator type given to BaseDeam!");

    std::atomic_size_t s_bitmask;
    std::atomic_size_t l_bitmask;
    std::atomic_size_t capacity;
    std::atomic_size_t next_grow;
    std::atomic_size_t init_grow;

    HashFct hash;

    std::unique_ptr<value_intern[]> t;
    size_type h(const key_type & k) const
    {
        auto temp = hash(k) & l_bitmask.load(std::memory_order_relaxed);
        return (temp < capacity.load(std::memory_order_relaxed)) ?
            temp : temp & s_bitmask.load(std::memory_order_relaxed);
    }

private:
    insert_return_intern insert_intern(const key_type& k, const mapped_type& d);
    ReturnCode           erase_intern (const key_type& k);


    template <class F, class ... Types>
    insert_return_intern update_intern
    (const key_type& k, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_intern update_unsafe_intern
    (const key_type& k, F f, Types&& ... args);


    template <class F, class ... Types>
    insert_return_intern insertOrUpdate_intern
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_intern insertOrUpdate_unsafe_intern
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);




    // HELPER FUNCTION FOR ITERATOR CREATION ***********************************

    inline iterator           makeIterator (const key_type& k, const mapped_type& d,
                                            value_intern* ptr)
    { return iterator(std::make_pair(k,d), ptr, this); }
    inline const_iterator     makeCIterator (const key_type& k, const mapped_type& d,
                                            value_intern* ptr) const
    { return const_iterator(std::make_pair(k,d), ptr, this); }
    inline insert_return_type makeInsertRet(const key_type& k, const mapped_type& d,
                                            value_intern* ptr, bool succ)
    { return std::make_pair(makeIterator(k,d, ptr), succ); }
    inline insert_return_type makeInsertRet(iterator it, bool succ)
    { return std::make_pair(it, succ); }

    inline insert_return_intern makeInsertRet(const key_type& k, const mapped_type& d,
                                            value_intern* ptr, ReturnCode code)
    { return std::make_pair(makeIterator(k,d, ptr), code); }
    inline insert_return_intern makeInsertRet(iterator it, ReturnCode code)
    { return std::make_pair(it, code); }




    // OTHER HELPER FUNCTIONS **************************************************
    void insert_unsafe(const key_type& k, const mapped_type& d, const size_type pos);
    void insert_marked(const key_type& k, const mapped_type& d, const size_type pos);
};









// CONSTRUCTORS/ASSIGNMENTS ****************************************************

template<class E, class HashFct, class A>
BaseDeam<E,HashFct,A>::BaseDeam(size_type cap)
{
    auto temp = allocator.allocate(max_capacity);
    t = std::unique_ptr<value_intern[]>(temp);
    if ( !t ) {std::cout << "nullptr in allocator" << std::endl; std::bad_alloc(); }

    size_type tcap = size_type(double(cap * init_factor)/double(block_grow));
    tcap *= block_grow;
    tcap = (tcap > 16*block_grow) ? tcap : 16*block_grow;
    size_type tbit = 16*block_grow;
    while (tbit <= tcap) tbit <<= 1;

    capacity.store (tcap,        std::memory_order_relaxed);
    l_bitmask.store(tbit-1,      std::memory_order_relaxed);
    s_bitmask.store((tbit>>1)-1, std::memory_order_relaxed);
    next_grow.store(tcap,        std::memory_order_relaxed);
    init_grow.store(tcap + over_grow, std::memory_order_relaxed);

    std::fill( t.get(), t.get() + tcap + over_grow, value_intern::getEmpty() );
}

template<class E, class HashFct, class A>
BaseDeam<E,HashFct,A>::~BaseDeam()
{
    //if (t) allocator.deallocate(t, max_capacity);
}


template<class E, class HashFct, class A>
BaseDeam<E,HashFct,A>::BaseDeam(BaseDeam&& rhs)
    : s_bitmask(rhs.s_bitmask.load(std::memory_order_relaxed)),
      l_bitmask(rhs.l_bitmask.load(std::memory_order_relaxed)),
      capacity (rhs.capacity .load(std::memory_order_relaxed)),
      next_grow(rhs.next_grow.load(std::memory_order_relaxed)),
      t(nullptr)
{
    //if (currentCopyBlock.load()) std::invalid_argument("Cannot move a growing table!");
    rhs.capacity .store(0, std::memory_order_relaxed);
    rhs.s_bitmask.store(0, std::memory_order_relaxed);
    rhs.l_bitmask.store(0, std::memory_order_relaxed);
    rhs.next_grow.store(0, std::memory_order_relaxed);

    std::swap(t, rhs.t);
}

template<class E, class HashFct, class A>
BaseDeam<E,HashFct,A>&
BaseDeam<E,HashFct,A>::operator=(BaseDeam&& rhs)
{
    //if (rhs.currentCopyBlock.load()) std::invalid_argument("Cannot move a growing table!");
    auto t1 = capacity.exchange(rhs.capacity.load());
    rhs.capacity.store(t1);
    auto t2 = s_bitmask.exchange(rhs.s_bitmask.load());
    rhs.s_bitmask.store(t2);
    auto t3 = l_bitmask.exchange(rhs.l_bitmask.load());
    rhs.l_bitmask.store(t3);
    auto t4 = next_grow.exchange(rhs.next_grow.load());
    rhs.next_grow.store(t4);

    std::swap(t, rhs.t);

    return *this;
}








// ITERATOR FUNCTIONALITY ******************************************************

template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::iterator
BaseDeam<E,HashFct,A>::begin()
{
    for (size_t i = 0; i<capacity; ++i)
    {
        auto temp = t[i];
        if (!temp.isEmpty() && !temp.isDeleted())
            return makeIterator(temp.getKey(), temp.getData(), &t[i]);
    }
    return end();
}

template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::iterator
BaseDeam<E,HashFct,A>::end()
{ return iterator(); }


template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::const_iterator
BaseDeam<E,HashFct,A>::cbegin() const
{
    for (size_t i = 0; i<capacity; ++i)
    {
        auto temp = t[i];
        if (!temp.isEmpty() && !temp.isDeleted())
            return makeCIterator(temp.getKey(), temp.getData(), &t[i]);
    }
    return end();
}

template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::const_iterator
BaseDeam<E,HashFct,A>::cend() const
{ return const_iterator(); }








// MAIN HASH TABLE FUNCTIONALITY (INTERN) **************************************

template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::insert_return_intern
BaseDeam<E,HashFct,A>::insert_intern(const key_type& k, const mapped_type& d)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i)
    {
        value_intern curr(t[i]);
        if (curr.isMarked()   )
        {
            do { curr = t[i]; } while ( curr.isMarked() );
            i = htemp = h(k);
            //return insert_intern(k,d);
            //return makeInsertRet(end() , ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
            return makeInsertRet(k, curr.getData(), &t[i], ReturnCode::UNSUCCESS_ALREADY_USED);
        else if (curr.isEmpty())
        {
            if (i >= capacity.load(std::memory_order_relaxed)+over_grow)
                return makeInsertRet(end(), ReturnCode::UNSUCCESS_FULL);
            auto marked_in = value_intern(k,d,true);
            if ( t[i].CAS(curr, marked_in) )
            {
                if (htemp == h(k))
                {
                    t[i].unmark();
                    return makeInsertRet(k,d, &t[i], ReturnCode::SUCCESS_IN);
                }
                else
                {
                    t[i].CAS(marked_in, value_intern::getEmpty());
                }
            }
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


template<class E, class HashFct, class A> template<class F, class ... Types>
inline typename BaseDeam<E,HashFct,A>::insert_return_intern
BaseDeam<E,HashFct,A>::update_intern(const key_type& k, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        value_intern curr(t[i]);
        if (curr.isMarked())
        {
            do { curr = t[i]; } while ( curr.isMarked() );
            i = h(k);
            //return insert_intern(k,d);
            //return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = t[i].atomicUpdate(curr,f, std::forward<Types>(args)...);
            if (succ)
                return makeInsertRet(k,data, &t[i], ReturnCode::SUCCESS_UP);
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

template<class E, class HashFct, class A> template<class F, class ... Types>
inline typename BaseDeam<E,HashFct,A>::insert_return_intern
BaseDeam<E,HashFct,A>::update_unsafe_intern(const key_type& k, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        value_intern curr(t[i]);
        if (curr.isMarked())
        {
            do { curr = t[i]; } while ( curr.isMarked() );
            i = h(k);
            //return insert_intern(k,d);
            //return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = t[i].nonAtomicUpdate(f, std::forward<Types>(args)...);
            if (succ)
                return makeInsertRet(k,data, &t[i], ReturnCode::SUCCESS_UP);
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


template<class E, class HashFct, class A> template<class F, class ... Types>
inline typename BaseDeam<E,HashFct,A>::insert_return_intern
BaseDeam<E,HashFct,A>::insertOrUpdate_intern(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
     size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        value_intern curr(t[i]);
        if (curr.isMarked())
        {
            do { curr = t[i]; } while ( curr.isMarked() );
            i = h(k);
            //return insert_intern(k,d);
            //return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = t[i].atomicUpdate(curr, f, std::forward<Types>(args)...);
            if (succ)
                return makeInsertRet(k,data, &t[i], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            if ( t[i].CAS(curr, value_intern(k,d)) ) return makeInsertRet(k,d, &t[i], ReturnCode::SUCCESS_IN);
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

template<class E, class HashFct, class A> template<class F, class ... Types>
inline typename BaseDeam<E,HashFct,A>::insert_return_intern
BaseDeam<E,HashFct,A>::insertOrUpdate_unsafe_intern(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        value_intern curr(t[i]);
        if (curr.isMarked())
        {
            do { curr = t[i]; } while ( curr.isMarked() );
            i = h(k);
            //return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = t[i].nonAtomicUpdate(f, std::forward<Types>(args)...);
            if (succ)
                return makeInsertRet(k,data, &t[i], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            if ( t[i].CAS(curr, value_intern(k,d)) ) return makeInsertRet(k,d, &t[i], ReturnCode::SUCCESS_IN);
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

template<class E, class HashFct, class A>
inline ReturnCode BaseDeam<E,HashFct,A>::erase_intern(const key_type& k)
{
    size_type htemp = h(k);
    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        value_intern curr(t[i]);
        if (curr.isMarked())
        {
            do { curr = t[i]; } while ( curr.isMarked() );
            i = h(k);
            //return insert_intern(k,d);
            //return ReturnCode::UNSUCCESS_INVALID;
        }
        else if (curr.compareKey(k))
        {
            if (t[i].atomicDelete(curr))
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







// MAIN HASH TABLE FUNCTIONALITY (EXTERN) **************************************

template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::iterator
BaseDeam<E,HashFct,A>::find(const key_type& k)
{
    size_type htemp = h(k);
    for (size_type i = htemp; ; ++i)
    {
        value_intern curr(t[i]);
        if (curr.isMarked())
        {
            do { curr = t[i]; } while ( curr.isMarked() );
            i = h(k);
        }
        else if (curr.compareKey(k)) return makeIterator(k, curr.getData(), &t[i]); // curr;
        else if (curr.isEmpty())     return end(); // ReturnElement::getEmpty();
    }
    return end(); // ReturnElement::getEmpty();
}

template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::const_iterator
BaseDeam<E,HashFct,A>::find(const key_type& k) const
{
    size_type htemp = h(k);
    for (size_type i = htemp; ; ++i)
    {
        value_intern curr(t[i]);
        if (curr.isMarked())
        {
            do { curr = t[i]; } while ( curr.isMarked() );
            i = h(k);
        }
        else if (curr.compareKey(k)) return makeCIterator(k, curr.getData(), &t[i]); // curr;
        else if (curr.isEmpty()) return cend(); // ReturnElement::getEmpty();
    }
    return cend(); // ReturnElement::getEmpty();
}




template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::insert_return_type
BaseDeam<E,HashFct,A>::insert(const key_type& k, const mapped_type& d)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = insert_intern(k,d);
    return std::make_pair(it, successful(c));
}

template<class E, class HashFct, class A>
inline typename BaseDeam<E,HashFct,A>::size_type
BaseDeam<E,HashFct,A>::erase(const key_type& k)
{
    ReturnCode c = erase_intern(k);
    return (successful(c)) ? 1 : 0;
}

template<class E, class HashFct, class A> template <class F, class ... Types>
inline typename BaseDeam<E,HashFct,A>::insert_return_type
BaseDeam<E,HashFct,A>::update(const key_type& k, F f, Types&& ... args)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = update_intern(k,f, std::forward<Types>(args)...);
    return std::make_pair(it, successful(c));
}

template<class E, class HashFct, class A> template <class F, class ... Types>
inline typename BaseDeam<E,HashFct,A>::insert_return_type
BaseDeam<E,HashFct,A>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = update_unsafe_intern(k,f, std::forward<Types>(args)...);
    return std::make_pair(it, successful(c));
}

template<class E, class HashFct, class A> template <class F, class ... Types>
inline typename BaseDeam<E,HashFct,A>::insert_return_type
BaseDeam<E,HashFct,A>::insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = insertOrUpdate_intern(k,d,f, std::forward<Types>(args)...);
    return std::make_pair(it, (c == ReturnCode::SUCCESS_IN));
}

template<class E, class HashFct, class A> template <class F, class ... Types>
inline typename BaseDeam<E,HashFct,A>::insert_return_type
BaseDeam<E,HashFct,A>::insertOrUpdate_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = insertOrUpdate_unsafe_intern(k,d,f, std::forward<Types>(args)...);
    return std::make_pair(it, (c == ReturnCode::SUCCESS_IN));
}















// MIGRATION/GROWING STUFF *****************************************************

template<class E, class HashFct, class A>
inline void
BaseDeam<E,HashFct,A>::grow()
{
    size_type r_start  = next_grow.fetch_add(block_grow, std::memory_order_acq_rel);

    // std::cout << "hi " << r_start << std::endl;

    size_type lbit   = 1;
    while (lbit < r_start) lbit <<=1;
    lbit   -= 1;
    size_type sbit = lbit >> 1;

    size_type l_start   = sbit & r_start;
    size_type l_end     = l_start + block_grow;
    size_type r_end     = r_start + block_grow;
    size_type r_start_p = r_start + over_grow;
    std::fill (t.get()+r_start_p,
               t.get()+r_end+over_grow, value_intern::getEmpty());

    size_type temp  = r_start_p;
    while (! init_grow.compare_exchange_weak(temp, r_end+over_grow))
    { temp = r_start_p; /* wait for previous initialization */ }

    // std::cout << "1:" << std::endl;

    // std::cout <<  "ls=" << l_start
    //           << " le=" << l_end
    //           << " rs=" << r_start
    //           << " re=" << r_end
    //           << " sb=" << sbit
    //           << " lb=" << lbit << std::endl;

    size_type true_l_end = l_end;
    while (true)
    {
        auto curr = t[true_l_end];
        while (curr.isMarked()) curr = t[true_l_end];
        if (curr.isEmpty())
        {
            if (t[true_l_end].atomicMark(curr)) break;
            else --true_l_end;
        }
        ++true_l_end;
    }

    // std::cout << "2:" << std::endl;

    size_type true_r_start = r_start;
    while (true)
    {
        auto curr = t[true_r_start];
        while (curr.isMarked()) curr = t[true_r_start];
        if (curr.isEmpty())
        {
            if (t[true_r_start].atomicMark(curr)) break;
            else --true_r_start;
        }
        ++true_r_start;
    }

    // std::cout << "3:" << std::endl;

    // std::cout << "tle=" << true_l_end
    //           << " trs=" << true_r_start << std::endl;

    size_type curr_end = true_l_end;
    int il_start = l_start;
    for (int i = true_l_end-1; i >= il_start; --i)
    {
        auto curr = t[i];
        if (curr.isMarked())
        {
            //std::cout << i << std::endl;
            while (curr.isMarked()) { curr = t[i]; }
        }
        if (! t[i].atomicMark(curr)) { ++i; }
        else if (curr.isEmpty())
        {
            for (size_type j = i+1; j < curr_end; ++j)
            {
                // reinsert with some new marked insert function
                curr = t[j];
                t[j].CAS(curr, value_intern::getMarkedEmpty());
                key_type    k = curr.getKey();
                mapped_type d = curr.getData();

                size_type pos = hash(k) & lbit;

                insert_unsafe(k,d,pos);
            }
        }
    }

    //std::cout << "4:" << std::endl;

    while (capacity.load(std::memory_order_acquire) < r_start)
    { /* wait for any previous running grows */ }
    if (r_end > lbit)
    {
        s_bitmask.store(lbit, std::memory_order_release);
        l_bitmask.store(2*lbit+1, std::memory_order_release);
        // std::cout <<  "sb=" << lbit
        //           << " lb=" << 2*lbit+1
        //           << " cp=" << r_end << std::endl;
    }
    capacity.store(r_end, std::memory_order_release);

    //std::cout << "5:" << std::endl;

    t[true_r_start].unmark();
    for (int i = true_l_end; i >= il_start; --i)
    {
        t[i].unmark();
    }

    // std::cout << "6" << std::endl;
}

template<class E, class HashFct, class A>
inline void BaseDeam<E,HashFct,A>::insert_unsafe(const key_type&    k,
                                                 const mapped_type& d,
                                                 const size_type    pos)
{
    for (size_type i = pos; ; ++i)
    {
        value_intern curr(t[i]);
        if (curr.isEmpty())
        {
            t[i].key |= k;
            t[i].data = d;
            return;
        }
    }
}

}
