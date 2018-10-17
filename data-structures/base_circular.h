/*******************************************************************************
 * data-structures/base_circular.h
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

#pragma once

#include <stdlib.h>
#include <functional>
#include <atomic>
#include <stdexcept>

#include "data-structures/returnelement.h"
#include "data-structures/base_iterator.h"
#include "example/update_fcts.h"

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
    using const_iterator     = IteratorBase<This_t, true>;
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

public:
    BaseCircular(size_type size_ = 1<<18);
    BaseCircular(size_type size_, size_type version_);

    BaseCircular(const BaseCircular&) = delete;
    BaseCircular& operator=(const BaseCircular&) = delete;

    // Obviously move-constructor and move-assignment are not thread safe
    // They are merely comfort functions used during setup
    BaseCircular(BaseCircular&& rhs);
    BaseCircular& operator=(BaseCircular&& rhs);

    ~BaseCircular();

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
    { return (*(insert(k, mapped_type()).first)).second; }

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


    size_type migrate(This_t& target, size_type s, size_type e);

    size_type _capacity;
    size_type _version;
    std::atomic_size_t _current_copy_block;

    static size_type resize(size_type current, size_type inserted, size_type deleted)
    {
        auto nsize = current;
        double fill_rate = double(inserted - deleted)/double(current);

        if (fill_rate > 0.6/2.) nsize <<= 1;

        return nsize;
    }

protected:
    Allocator_t allocator;
    static_assert(std::is_same<typename Allocator_t::value_type, value_intern>::value,
                  "Wrong allocator type given to BaseCircular!");

    size_type _bitmask;
    size_type _right_shift;
    HashFct   hash;

    value_intern* t;
    size_type h(const key_type & k) const { return hash(k) >> _right_shift; }

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
    { return iterator(std::make_pair(k,d), ptr, t+_capacity); }
    inline const_iterator     makeCIterator (const key_type& k, const mapped_type& d,
                                            value_intern* ptr) const
    { return const_iterator(std::make_pair(k,d), ptr, t+_capacity); }
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

    void insert_unsafe(const value_intern& e);

    static size_type compute_capacity(size_type desired_capacity)
    {
        auto temp = 16384u;
        while (temp < desired_capacity*(MiSt/100.)) temp <<= 1;
        return temp;
    }

    static size_type compute_right_shift(size_type capacity)
    {
        size_type log_size = 0;
        while (capacity >>= 1) log_size++;
        return HashFct::significant_digits - log_size;
    }

public:
    using range_iterator = iterator;
    using const_range_iterator = const_iterator;

    /* size has to divide capacity */
    range_iterator       range (size_t rstart, size_t rend);
    const_range_iterator crange(size_t rstart, size_t rend);
    range_iterator       range_end ()       { return  end(); }
    const_range_iterator range_cend() const { return cend(); }
    size_t               capacity()   const { return _capacity; }

};









// CONSTRUCTORS/ASSIGNMENTS ****************************************************

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>::BaseCircular(size_type capacity_)
    : _capacity(compute_capacity(capacity_)),
      _version(0),
      _current_copy_block(0),
      _bitmask(_capacity-1),
      _right_shift(compute_right_shift(_capacity))

{
    t = allocator.allocate(_capacity);
    if ( !t ) std::bad_alloc();

    std::fill( t ,t + _capacity , value_intern::getEmpty() );
}

/*should always be called with a capacity_=2^k  */
template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>::BaseCircular(size_type capacity_, size_type version_)
    : _capacity(capacity_),
      _version(version_),
      _current_copy_block(0),
      _bitmask(_capacity-1),
      _right_shift(compute_right_shift(_capacity))
{
    t = allocator.allocate(_capacity);
    if ( !t ) std::bad_alloc();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>::~BaseCircular()
{
    if (t) allocator.deallocate(t, _capacity);
}


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>::BaseCircular(BaseCircular&& rhs)
    : _capacity(rhs._capacity), _version(rhs._version),
      _current_copy_block(rhs._current_copy_block.load()),
      _bitmask(rhs._bitmask), _right_shift(rhs._right_shift), t(nullptr)
{
    if (_current_copy_block.load()) std::invalid_argument("Cannot move a growing table!");
    rhs._capacity = 0;
    rhs._bitmask = 0;
    rhs._right_shift = HashFct::significant_digits;
    std::swap(t, rhs.t);
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
BaseCircular<E,HashFct,A,MaDis,MiSt>&
BaseCircular<E,HashFct,A,MaDis,MiSt>::operator=(BaseCircular&& rhs)
{
    if (rhs._current_copy_block.load()) std::invalid_argument("Cannot move a growing table!");
    _capacity   = rhs._capacity;
    _version    = rhs._version;
    _current_copy_block.store(0);;
    _bitmask     = rhs._bitmask;
    _right_shift = rhs._right_shift;
    rhs._capacity    = 0;
    rhs._bitmask = 0;
    rhs._right_shift = HashFct::significant_digits;
    std::swap(t, rhs.t);

    return *this;
}








// ITERATOR FUNCTIONALITY ******************************************************

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::begin()
{
    for (size_t i = 0; i<_capacity; ++i)
    {
        auto temp = t[i];
        if (!temp.isEmpty() && !temp.isDeleted())
            return makeIterator(temp.getKey(), temp.getData(), &t[i]);
    }
    return end();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::end()
{ return iterator(std::make_pair(key_type(), mapped_type()),nullptr,nullptr); }


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::const_iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::cbegin() const
{
    for (size_t i = 0; i<_capacity; ++i)
    {
        auto temp = t[i];
        if (!temp.isEmpty() && !temp.isDeleted())
            return makeCIterator(temp.getKey(), temp.getData(), &t[i]);
    }
    return end();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::const_iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::cend() const
{
    return const_iterator(std::make_pair(key_type(),mapped_type()),
                          nullptr,nullptr);
}


// RANGE ITERATOR FUNCTIONALITY ************************************************

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::range_iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::range(size_t rstart, size_t rend)
{
    auto temp_rend = std::min(rend, _capacity);
    for (size_t i = rstart; i < temp_rend; ++i)
    {
        auto temp = t[i];
        if (!temp.isEmpty() && !temp.isDeleted())
            return range_iterator(std::make_pair(temp.getKey(), temp.getData()),
                                  &t[i], &t[temp_rend]);
    }
    return range_end();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::const_range_iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::crange(size_t rstart, size_t rend)
{
    auto temp_rend = std::min(rend, _capacity);
    for (size_t i = rstart; i < temp_rend; ++i)
    {
        auto temp = t[i];
        if (!temp.isEmpty() && !temp.isDeleted())
            return const_range_iterator(std::make_pair(temp.getKey(), temp.getData()),
                                  &t[i], &t[temp_rend]);
    }
    return range_cend();
}



// MAIN HASH TABLE FUNCTIONALITY (INTERN) **************************************

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_intern
BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_intern(const key_type& k,
                                                    const mapped_type& d)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        size_type temp = i & _bitmask;
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


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F, class ... Types>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_intern
BaseCircular<E,HashFct,A,MaDis,MiSt>::update_intern(const key_type& k, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        size_type temp = i & _bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = t[temp].atomicUpdate(curr,f, std::forward<Types>(args)...);
            if (succ)
                return makeInsertRet(k,data, &t[temp], ReturnCode::SUCCESS_UP);
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

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F, class ... Types>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_intern
BaseCircular<E,HashFct,A,MaDis,MiSt>::update_unsafe_intern(const key_type& k, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        size_type temp = i & _bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = t[temp].nonAtomicUpdate(f, std::forward<Types>(args)...);
            if (succ)
                return makeInsertRet(k,data, &t[temp], ReturnCode::SUCCESS_UP);
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


template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F, class ... Types>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_intern
BaseCircular<E,HashFct,A,MaDis,MiSt>::insertOrUpdate_intern(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        size_type temp = i & _bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = t[temp].atomicUpdate(curr, f, std::forward<Types>(args)...);
            if (succ)
                return makeInsertRet(k,data, &t[temp], ReturnCode::SUCCESS_UP);
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

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template<class F, class ... Types>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_intern
BaseCircular<E,HashFct,A,MaDis,MiSt>::insertOrUpdate_unsafe_intern(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        size_type temp = i & _bitmask;
        value_intern curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.compareKey(k))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = t[temp].nonAtomicUpdate(f, std::forward<Types>(args)...);
            if (succ)
                return makeInsertRet(k,data, &t[temp], ReturnCode::SUCCESS_UP);
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
inline ReturnCode BaseCircular<E,HashFct,A,MaDis,MiSt>::erase_intern(const key_type& k)
{
    size_type htemp = h(k);
    for (size_type i = htemp; ; ++i) //i < htemp+MaDis
    {
        size_type temp = i & _bitmask;
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







// MAIN HASH TABLE FUNCTIONALITY (EXTERN) **************************************

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::find(const key_type& k)
{
    size_type htemp = h(k);
    for (size_type i = htemp; ; ++i)
    {
        value_intern curr(t[i & _bitmask]);
        if (curr.compareKey(k)) return makeIterator(k, curr.getData(), &t[i & _bitmask]); // curr;
        if (curr.isEmpty()) return end(); // ReturnElement::getEmpty();
    }
    return end(); // ReturnElement::getEmpty();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::const_iterator
BaseCircular<E,HashFct,A,MaDis,MiSt>::find(const key_type& k) const
{
    size_type htemp = h(k);
    for (size_type i = htemp; ; ++i)
    {
        value_intern curr(t[i & _bitmask]);
        if (curr.compareKey(k)) return makeCIterator(k, curr.getData(), &t[i & _bitmask]); // curr;
        if (curr.isEmpty()) return cend(); // ReturnElement::getEmpty();
    }
    return cend(); // ReturnElement::getEmpty();
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::insert(const key_type& k, const mapped_type& d)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = insert_intern(k,d);
    return std::make_pair(it, successful(c));
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::size_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::erase(const key_type& k)
{
    ReturnCode c = erase_intern(k);
    return (successful(c)) ? 1 : 0;
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template <class F, class ... Types>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::update(const key_type& k, F f, Types&& ... args)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = update_intern(k,f, std::forward<Types>(args)...);
    return std::make_pair(it, successful(c));
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template <class F, class ... Types>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = update_unsafe_intern(k,f, std::forward<Types>(args)...);
    return std::make_pair(it, successful(c));
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template <class F, class ... Types>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = insertOrUpdate_intern(k,d,f, std::forward<Types>(args)...);
    return std::make_pair(it, (c == ReturnCode::SUCCESS_IN));
}

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt> template <class F, class ... Types>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::insert_return_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::insertOrUpdate_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    iterator   it = end();
    ReturnCode c  = ReturnCode::ERROR;
    std::tie(it,c) = insertOrUpdate_unsafe_intern(k,d,f, std::forward<Types>(args)...);
    return std::make_pair(it, (c == ReturnCode::SUCCESS_IN));
}















// MIGRATION/GROWING STUFF *****************************************************

template<class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename BaseCircular<E,HashFct,A,MaDis,MiSt>::size_type
BaseCircular<E,HashFct,A,MaDis,MiSt>::migrate(This_t& target, size_type s, size_type e)
{
    size_type n = 0;
    auto i = s;
    auto curr = value_intern::getEmpty();

    //HOW MUCH BIGGER IS THE TARGET TABLE
    auto shift = 0u;
    while (target._capacity > (_capacity << shift)) ++shift;


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
        auto pos  = i&_bitmask;
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
        size_type temp = i & _bitmask;
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
