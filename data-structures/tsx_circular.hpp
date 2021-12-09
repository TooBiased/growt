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

#include <atomic>
#include <functional>
#include <stdexcept>
#include <stdlib.h>

//#include <rtm.h>
#include <immintrin.h>

#include "data-structures/returnelement.hpp"
#include "data-structures/tsx_iterator.hpp"
#include "example/update_fcts.hpp"

namespace growt
{

template <class E, typename HashFct = std::hash<typename E::Key>,
          class A = std::allocator<E>, size_t MaDis = 128, size_t MiSt = 200>
class TSXCircular
{
  private:
    using This_t      = TSXCircular<E, HashFct, A, MaDis, MiSt>;
    using Allocator_t = typename A::template rebind<E>::other;

    template <class> friend class GrowTableHandle;

  public:
    using value_intern = E;

    using key_type    = typename value_intern::key_type;
    using mapped_type = typename value_intern::mapped_type;
    using value_type  = E; // typename std::pair<const key_type, mapped_type>;
    using iterator    = IteratorTSX<This_t, false>; // E*;
    using const_iterator         = IteratorTSX<This_t, true>;
    using size_type              = size_t;
    using difference_type        = std::ptrdiff_t;
    using reference              = ReferenceTSX<This_t, false>;
    using const_reference        = ReferenceTSX<This_t, true>;
    using mapped_reference       = MappedRefTSX<This_t, false>;
    using const_mapped_reference = MappedRefTSX<This_t, true>;
    using insert_return_type     = std::pair<iterator, bool>;


    using local_iterator       = void;
    using const_local_iterator = void;
    using node_type            = void;

    using Handle = This_t&;

  private:
    using insert_return_intern = std::pair<iterator, ReturnCode>;

  public:
    TSXCircular(size_type size_ = 1 << 18);
    TSXCircular(size_type size_, size_type version_);

    TSXCircular(const TSXCircular&) = delete;
    TSXCircular& operator=(const TSXCircular&) = delete;

    TSXCircular(TSXCircular&& rhs);
    TSXCircular& operator=(TSXCircular&& rhs);

    ~TSXCircular();

    Handle getHandle() { return *this; }

    iterator       begin();
    iterator       end();
    const_iterator cbegin() const;
    const_iterator cend() const;
    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }

    insert_return_type insert(const key_type& k, const mapped_type& d);
    size_type          erase(const key_type& k);
    iterator           find(const key_type& k);
    const_iterator     find(const key_type& k) const;

    template <class F, class... Types>
    insert_return_type update(const key_type& k, F f, Types&&... args);

    template <class F, class... Types>
    insert_return_type update_unsafe(const key_type& k, F f, Types&&... args);


    template <class F, class... Types>
    insert_return_type insertOrUpdate(const key_type& k, const mapped_type& d,
                                      F f, Types&&... args);

    template <class F, class... Types>
    insert_return_type
    insertOrUpdate_unsafe(const key_type& k, const mapped_type& d, F f,
                          Types&&... args);

    size_type migrate(This_t& target, size_type s, size_type e);

    size_type              capacity;
    size_type              version;
    std::atomic<size_type> currentCopyBlock;

    static size_type resize(size_type cur, size_type ins, size_type del)
    {
        auto   temp      = cur;
        double fill_rate = double(ins - del) / double(cur);

        if (fill_rate > 0.6 / 2.) temp <<= 1;

        return temp;
    }

  protected:
    Allocator_t allocator;
    static_assert(
        std::is_same<typename Allocator_t::value_type, value_intern>::value,
        "Wrong allocator type given to TSXCircular!");

    size_type bitmask;
    size_type right_shift;
    HashFct   hash;

    value_intern* t;
    size_type     h(const key_type& k) const { return hash(k) >> right_shift; }

  private:
    insert_return_intern insert_intern(const key_type& k, const mapped_type& d);
    ReturnCode           erase_intern(const key_type& k);

    template <class F, class... Types>
    insert_return_intern update_intern(const key_type& k, F f, Types&&... args);

    template <class F, class... Types>
    insert_return_intern
    update_unsafe_intern(const key_type& k, F f, Types&&... args);


    template <class F, class... Types>
    insert_return_intern
    insertOrUpdate_intern(const key_type& k, const mapped_type& d, F f,
                          Types&&... args);

    template <class F, class... Types>
    insert_return_intern
    insertOrUpdate_unsafe_intern(const key_type& k, const mapped_type& d, F f,
                                 Types&&... args);

    insert_return_intern
    non_atomic_insert(size_type pos, const key_type& k, const mapped_type& d);
    template <class F, class... Types>
    insert_return_intern
    non_atomic_update(size_type pos, const key_type& k, F f, Types&&... args);
    template <class F, class... Types>
    insert_return_intern
    non_atomic_insertOrUpdate(size_type pos, const key_type& k,
                              const mapped_type& d, F f, Types&&... args);

    insert_return_intern
    atomic_insert(size_type pos, const key_type& k, const mapped_type& d);
    ReturnCode     atomic_erase(const key_type& k);
    iterator       base_find(const key_type& k);
    const_iterator base_find(const key_type& k) const;

    template <class F, class... Types>
    insert_return_intern
    atomic_update(size_type pos, const key_type& k, F f, Types&&... args);
    template <class F, class... Types>
    insert_return_intern atomic_update_unsafe(size_type pos, const key_type& k,
                                              F f, Types&&... args);

    template <class F, class... Types>
    insert_return_intern
    atomic_insertOrUpdate(size_type pos, const key_type& k,
                          const mapped_type& d, F f, Types&&... args);
    template <class F, class... Types>
    insert_return_intern
    atomic_insertOrUpdate_unsafe(size_type pos, const key_type& k,
                                 const mapped_type& d, F f, Types&&... args);

    // HELPER FUNCTION FOR ITERATOR CREATION ***********************************

    inline iterator
    makeIterator(const key_type& k, const mapped_type& d, value_intern* ptr)
    {
        return iterator(std::make_pair(k, d), ptr, t + capacity);
    }

    inline const_iterator makeCIterator(const key_type& k, const mapped_type& d,
                                        value_intern* ptr) const
    {
        return const_iterator(std::make_pair(k, d), ptr, t + capacity);
    }

    inline insert_return_type
    makeInsertRet(const key_type& k, const mapped_type& d, value_intern* ptr,
                  bool succ)
    {
        return std::make_pair(makeIterator(k, d, ptr), succ);
    }

    inline insert_return_type makeInsertRet(iterator it, bool succ)
    {
        return std::make_pair(it, succ);
    }

    inline insert_return_intern
    makeInsertRet(const key_type& k, const mapped_type& d, value_intern* ptr,
                  ReturnCode code)
    {
        return std::make_pair(makeIterator(k, d, ptr), code);
    }

    inline insert_return_intern makeInsertRet(iterator it, ReturnCode code)
    {
        return std::make_pair(it, code);
    }


    // GROW STUFF **************************************************************

    void insert_unsave(const value_intern& e);

    static size_type compute_size(size_type desired_capacity)
    {
        auto temp = 4096u;
        while (temp < desired_capacity * (MiSt / 100.)) temp <<= 1;
        return temp;
    }

    static size_type compute_right_shift(size_type capacity)
    {
        size_type log_cap  = 0;
        size_type temp_cap = capacity;
        while (temp_cap >>= 1) log_cap++;
        return HashFct::significant_digits - log_cap;
    }
};



template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E, HashFct, A, MaDis, MiSt>::TSXCircular(size_type cap)
    : capacity(compute_size(cap)), version(0), currentCopyBlock(0),
      bitmask(capacity - 1), right_shift(compute_right_shift(capacity))
{
    t = allocator.allocate(capacity);
    if (!t) std::bad_alloc();

    std::fill(t, t + capacity, E::getEmpty());
}

/*should always be called with a size_=2^k  */
template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E, HashFct, A, MaDis, MiSt>::TSXCircular(size_type cap,
                                                     size_type ver)
    : capacity(cap), version(ver), currentCopyBlock(0), bitmask(capacity - 1),
      right_shift(compute_right_shift(capacity))
{
    t = allocator.allocate(capacity);
    if (!t) std::bad_alloc();
}

template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E, HashFct, A, MaDis, MiSt>::~TSXCircular()
{
    allocator.deallocate(t, capacity);
}

template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E, HashFct, A, MaDis, MiSt>::TSXCircular(TSXCircular&& rhs)
    : capacity(rhs.capacity), version(rhs.version),
      currentCopyBlock(rhs.currentCopyBlock.load()), bitmask(rhs.bitmask),
      right_shift(rhs.right_shift), t(nullptr)
{
    if (currentCopyBlock.load())
        std::invalid_argument("Cannot move a growing table!");
    rhs.capacity    = 0;
    rhs.bitmask     = 0;
    rhs.right_shift = HashFct::significant_digits;
    std::swap(t, rhs.t);
}

template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
TSXCircular<E, HashFct, A, MaDis, MiSt>&
TSXCircular<E, HashFct, A, MaDis, MiSt>::operator=(TSXCircular&& rhs)
{
    if (currentCopyBlock.load())
        std::invalid_argument("Cannot move a growing table!");
    capacity = rhs.capacity;
    version  = rhs.version;
    currentCopyBlock.store(0);
    ;
    bitmask         = rhs.bitmask;
    right_shift     = rhs.right_shift;
    rhs.capacity    = 0;
    rhs.bitmask     = 0;
    rhs.right_shift = HashFct::significant_digits;
    std::swap(t, rhs.t);

    return *this;
}



// * Iterator Functionality * **************************************************

template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename TSXCircular<E, HashFct, A, MaDis, MiSt>::iterator
TSXCircular<E, HashFct, A, MaDis, MiSt>::begin()
{
    for (size_t i = 0; i < capacity; ++i)
    {
        auto temp = t[i];
        if (!temp.isEmpty() && !temp.isDeleted())
            return makeIterator(temp.get_key(), temp.get_data(), &t[i]);
    }
    return end();
}

template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename TSXCircular<E, HashFct, A, MaDis, MiSt>::iterator
TSXCircular<E, HashFct, A, MaDis, MiSt>::end()
{
    return iterator(std::make_pair(key_type(), mapped_type()), nullptr,
                    nullptr);
}


template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename TSXCircular<E, HashFct, A, MaDis, MiSt>::const_iterator
TSXCircular<E, HashFct, A, MaDis, MiSt>::cbegin() const
{
    for (size_t i = 0; i < capacity; ++i)
    {
        auto temp = t[i];
        if (!temp.isEmpty() && !temp.isDeleted())
            return makeCIterator(temp.get_key(), temp.get_data(), &t[i]);
    }
    return end();
}

template <class E, class HashFct, class A, size_t MaDis, size_t MiSt>
inline typename TSXCircular<E, HashFct, A, MaDis, MiSt>::const_iterator
TSXCircular<E, HashFct, A, MaDis, MiSt>::cend() const
{
    return const_iterator(std::make_pair(key_type(), mapped_type()), nullptr,
                          nullptr);
}




// * Insert * ******************************************************************

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::non_atomic_insert(size_type          pos,
                                                 const key_type&    k,
                                                 const mapped_type& d)
{
    for (size_type i = pos;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);

        if (curr.isMarked())
            return makeInsertRet(end(), ReturnCode::TSX_UNSUCCESS_INVALID);
        else if (curr.compareKey(k))
            return makeInsertRet(k, curr.get_data(), &t[temp],
                                 ReturnCode::TSX_UNSUCCESS_ALREADY_USED);
        else if (curr.isEmpty())
        {
            // This is not threadsave (but only called with tsx)
            t[temp] = value_intern(k, d);
            return makeInsertRet(k, d, &t[temp], ReturnCode::TSX_SUCCESS_IN);
        }
    }
}

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::atomic_insert(size_type pos, const key_type& k,
                                             const mapped_type& d)
{
    for (size_type i = pos;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isMarked())
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        else if (curr.compareKey(k))
            return makeInsertRet(
                k, curr.get_data(), &t[temp],
                ReturnCode::UNSUCCESS_ALREADY_USED); // already hashed
        else if (curr.isEmpty())
        {
            if (t[temp].CAS(curr, value_intern(k, d)))
                return makeInsertRet(k, d, &t[temp], ReturnCode::SUCCESS_IN);
            // somebody changed the current element! recheck it
            --i;
        }
    }
}

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::insert_intern(const key_type&    k,
                                             const mapped_type& d)
{
    size_type pos = h(k);

    size_type status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_insert(pos, k, d);
        _xend();

        return temp;
    }

    return atomic_insert(pos, k, d);
}

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_type
TSXCircular<E, HF, A, MD, MS>::insert(const key_type& k, const mapped_type& d)
{
    iterator   it = end();
    ReturnCode rc;
    std::tie(it, rc) = insert_intern(k, d);

    return makeInsertRet(it, successful(rc));
}




// * Update * ******************************************************************

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::non_atomic_update(size_type       pos,
                                                 const key_type& k, F f,
                                                 Types&&... args)
{
    for (size_type i = pos;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isMarked())
            return makeInsertRet(end(), ReturnCode::TSX_UNSUCCESS_INVALID);
        else if (curr.compareKey(k))
        {
            // not threadsafe => use TSX
            auto val = t[temp]
                           .non_atomic_update(f, std::forward<Types>(args)...)
                           .first;
            return makeInsertRet(k, val, &t[temp], ReturnCode::TSX_SUCCESS_UP);
        }
        else if (curr.isEmpty())
        {
            return makeInsertRet(end(), ReturnCode::TSX_UNSUCCESS_NOT_FOUND);
            // not threadsafe => use TSX
        }
    }
}

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::atomic_update(size_type pos, const key_type& k,
                                             F f, Types&&... args)
{
    for (size_type i = pos;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        if (curr.compareKey(k))
        {
            mapped_type val;
            bool        suc;
            std::tie(val, suc) =
                t[temp].atomicUpdate(curr, f, std::forward<Types>(args)...);
            if (suc)
                return makeInsertRet(k, val, &t[temp], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_NOT_FOUND);
        }
    }
}


template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::update_intern(const key_type& k, F f,
                                             Types&&... args)
{
    size_type pos = h(k);

    auto status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_update(pos, k, f, std::forward<Types>(args)...);
        _xend();
        return temp;
    }
    return atomic_update(pos, k, f, std::forward<Types>(args)...);
}

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_type
TSXCircular<E, HF, A, MD, MS>::update(const key_type& k, F f, Types&&... args)
{
    iterator   it = end();
    ReturnCode rc;
    std::tie(it, rc) = update_intern(k, f, std::forward<Types>(args)...);

    return makeInsertRet(it, successful(rc));
}


template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::atomic_update_unsafe(size_type       pos,
                                                    const key_type& k, F f,
                                                    Types&&... args)
{
    for (size_type i = pos;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isMarked())
        {
            q return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        if (curr.compareKey(k))
        {
            mapped_type val;
            bool        suc;
            std::tie(val, suc) =
                t[temp].non_atomic_update(f, std::forward<Types>(args)...);
            if (suc)
                return makeInsertRet(k, val, &t[temp], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_NOT_FOUND);
        }
    }
}


template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::update_unsafe_intern(const key_type& k, F f,
                                                    Types&&... args)
{
    size_type pos = h(k);

    auto status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_update(pos, k, f, std::forward<Types>(args)...);
        _xend();
        return temp;
    }
    return atomic_update_unsafe(pos, k, f, std::forward<Types>(args)...);
}

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_type
TSXCircular<E, HF, A, MD, MS>::update_unsafe(const key_type& k, F f,
                                             Types&&... args)
{
    iterator   it = end();
    ReturnCode rc;
    std::tie(it, rc) = update_unsafe_intern(k, f, std::forward<Types>(args)...);

    return makeInsertRet(it, successful(rc));
}






// * Insert or Update * ********************************************************

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::non_atomic_insertOrUpdate(size_type          pos,
                                                         const key_type&    k,
                                                         const mapped_type& d,
                                                         F f, Types&&... args)
{
    for (size_type i = pos;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isMarked())
            return makeInsertRet(end(), ReturnCode::TSX_UNSUCCESS_INVALID);
        else if (curr.compareKey(k))
        {
            // not threadsafe => use TSX
            mapped_type val =
                t[temp]
                    .non_atomic_update(f, std::forward<Types>(args)...)
                    .first;
            return makeInsertRet(k, val, &t[temp], ReturnCode::TSX_SUCCESS_UP);
        }
        else if (curr.isEmpty())
        {
            // not threadsafe => use TSX
            t[temp] = value_intern(k, d);
            return makeInsertRet(k, d, &t[temp], ReturnCode::TSX_SUCCESS_IN);
        }
    }
}

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::atomic_insertOrUpdate(size_type          pos,
                                                     const key_type&    k,
                                                     const mapped_type& d, F f,
                                                     Types&&... args)
{
    for (size_type i = pos;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        if (curr.compareKey(k))
        {
            mapped_type val;
            bool        suc;
            std::tie(val, suc) =
                t[temp].atomicUpdate(curr, f, std::forward<Types>(args)...);
            if (suc)
                return makeInsertRet(k, val, &t[temp], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            if (t[temp].CAS(curr, value_intern(k, d)))
                return makeInsertRet(k, d, &t[temp], ReturnCode::SUCCESS_IN);
            // somebody changed the current element! recheck it
            --i;
        }
    }
}


template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::insertOrUpdate_intern(const key_type&    k,
                                                     const mapped_type& d, F f,
                                                     Types&&... args)
{
    size_type pos = h(k);

    auto status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp = non_atomic_insertOrUpdate(pos, k, d, f,
                                              std::forward<Types>(args)...);
        _xend();
        return temp;
    }

    return atomic_insertOrUpdate(pos, k, d, f, std::forward<Types>(args)...);
}

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_type
TSXCircular<E, HF, A, MD, MS>::insertOrUpdate(const key_type&    k,
                                              const mapped_type& d, F f,
                                              Types&&... args)
{
    iterator   it = end();
    ReturnCode rc;
    std::tie(it, rc) =
        insertOrUpdate_intern(k, d, f, std::forward<Types>(args)...);
    return makeInsertRet(it, (rc == ReturnCode::TSX_SUCCESS_IN) ||
                                 (rc == ReturnCode::SUCCESS_IN));
}

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::atomic_insertOrUpdate_unsafe(
    size_type pos, const key_type& k, const mapped_type& d, F f,
    Types&&... args)
{
    for (size_type i = pos;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isMarked())
        {
            return makeInsertRet(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        if (curr.compareKey(k))
        {
            mapped_type val;
            bool        suc;
            std::tie(val, suc) =
                t[temp].non_atomic_update(f, std::forward<Types>(args)...);
            if (suc)
                return makeInsertRet(k, val, &t[temp], ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.isEmpty())
        {
            if (t[temp].CAS(curr, value_intern(k, d)))
                return makeInsertRet(k, d, &t[temp], ReturnCode::SUCCESS_IN);
            // somebody changed the current element! recheck it
            --i;
        }
    }
}

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_intern
TSXCircular<E, HF, A, MD, MS>::insertOrUpdate_unsafe_intern(
    const key_type& k, const mapped_type& d, F f, Types&&... args)
{
    auto status = _xbegin();

    if (_XBEGIN_STARTED == status)
    {
        auto temp =
            non_atomic_insertOrUpdate(k, d, f, std::forward<Types>(args)...);
        _xend();
        return temp;
    }

    return atomic_insertOrUpdate_unsafe(k, d, f, std::forward<Types>(args)...);
}

template <class E, class HF, class A, size_t MD, size_t MS>
template <class F, class... Types>
inline typename TSXCircular<E, HF, A, MD, MS>::insert_return_type
TSXCircular<E, HF, A, MD, MS>::insertOrUpdate_unsafe(const key_type&    k,
                                                     const mapped_type& d, F f,
                                                     Types&&... args)
{
    iterator   it = end();
    ReturnCode rc;
    std::tie(it, rc) =
        insertOrUpdate_unsafe_intern(k, d, f, std::forward<Types>(args)...);
    return makeInsertRet(it, (rc == ReturnCode::TSX_SUCCESS_IN) ||
                                 (rc == ReturnCode::SUCCESS_IN));
}




// * Find * ********************************************************************

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::iterator
TSXCircular<E, HF, A, MD, MS>::base_find(const key_type& k)
{
    size_type htemp = h(k);
    for (size_type i = htemp;; ++i) // i < htemp+MaDis
    {
        E curr(t[i & bitmask]);
        if (curr.compareKey(k))
            return makeIterator(k, curr.get_data(), &t[i & bitmask]);
        if (curr.isEmpty()) return end();
    }
}

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::iterator
TSXCircular<E, HF, A, MD, MS>::find(const key_type& k)
{
    // no need to use tsx since find never needed to be atomic
    // the answer is consistent even in case of a torn read
    return base_find(k);
}

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::const_iterator
TSXCircular<E, HF, A, MD, MS>::base_find(const key_type& k) const
{
    size_type htemp = h(k);
    for (size_type i = htemp;; ++i) // i < htemp+MaDis
    {
        E curr(t[i & bitmask]);
        if (curr.compareKey(k))
            return makeCIterator(k, curr.get_data(), &t[i & bitmask]);
        if (curr.isEmpty()) return cend();
    }
}

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::const_iterator
TSXCircular<E, HF, A, MD, MS>::find(const key_type& k) const
{
    // no need to use tsx since find never needed to be atomic
    // the answer is consistent even in case of a torn read
    return base_find(k);
}




// * Erase * *******************************************************************

template <class E, class HF, class A, size_t MD, size_t MS>
inline ReturnCode TSXCircular<E, HF, A, MD, MS>::atomic_erase(const key_type& k)
{
    size_type htemp = h(k);
    for (size_type i = htemp;; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isMarked()) { return ReturnCode::UNSUCCESS_INVALID; }
        else if (curr.compareKey(k))
        {
            if (t[temp].atomicDelete(curr)) return ReturnCode::SUCCESS_DEL;
            i--;
        }
        else if (curr.isEmpty())
        {
            return ReturnCode::UNSUCCESS_NOT_FOUND;
        }
        else if (curr.isDeleted())
        {
            // do something appropriate
        }
    }

    return ReturnCode::UNSUCCESS_NOT_FOUND;
}

template <class E, class HF, class A, size_t MD, size_t MS>
inline ReturnCode TSXCircular<E, HF, A, MD, MS>::erase_intern(const key_type& k)
{
    return atomic_erase(k);
}

template <class E, class HF, class A, size_t MD, size_t MS>
inline typename TSXCircular<E, HF, A, MD, MS>::size_type
TSXCircular<E, HF, A, MD, MS>::erase(const key_type& k)
{
    return (successful(atomic_erase(k))) ? 1 : 0;
}





// * Grow Stuff * **************************************************************

template <class E, class HF, class A, size_t MD, size_t MS>
inline size_t
TSXCircular<E, HF, A, MD, MS>::migrate(This_t& target, size_type s, size_type e)
{
    size_type n    = 0;
    auto      i    = s;
    auto      curr = E::getEmpty();

    // HOW MUCH BIGGER IS THE TARGET TABLE
    auto shift = 0u;
    while (target.capacity > (capacity << shift)) ++shift;


    // FINDS THE FIRST EMPTY BUCKET (START OF IMPLICIT BLOCK)
    while (i < e)
    {
        curr = t[i]; // no bitmask necessary (within one block)
        if (curr.isEmpty())
        {
            if (t[i].atomicMark(curr))
                break;
            else
                --i;
        }
        ++i;
    }

    std::fill(target.t + (i << shift), target.t + (e << shift), E::getEmpty());

    // MIGRATE UNTIL THE END OF THE BLOCK
    for (; i < e; ++i)
    {
        curr = t[i];
        if (!t[i].atomicMark(curr))
        {
            --i;
            continue;
        }
        else if (!curr.isEmpty())
        {
            if (!curr.isDeleted())
            {
                target.insert_unsave(curr);
                ++n;
            }
        }
    }

    auto b = true; // b indicates, if t[i-1] was non-empty

    // CONTINUE UNTIL WE FIND AN EMPTY BUCKET
    // THE TARGET POSITIONS WILL NOT BE INITIALIZED
    for (; b; ++i)
    {
        auto pos   = i & bitmask;
        auto t_pos = pos << shift;
        for (size_type j = 0; j < 1ull << shift; ++j)
            target.t[t_pos + j] = E::getEmpty();
        // target.t[t_pos] = E::getEmpty();

        curr = t[pos];

        if (!t[pos].atomicMark(curr)) --i;
        if ((b = !curr.isEmpty())) // this might be nicer as an else if, but
                                   // this is faster
        {
            if (!curr.isDeleted())
            {
                target.insert_unsave(curr);
                n++;
            }
        }
    }

    return n;
}


template <class E, class HF, class A, size_t MD, size_t MS>
void TSXCircular<E, HF, A, MD, MS>::insert_unsave(const value_intern& e)
{
    const key_type k = e.get_key();

    for (size_type i = h(k);; ++i) // i < htemp+MaDis
    {
        size_type temp = i & bitmask;
        E         curr(t[temp]);
        if (curr.isEmpty() || curr.compareKey(k))
        {
            t[temp] = e;
            return;
        }
    }
    throw std::bad_alloc();
}

} // namespace growt

#endif // TSXCIRCULAR_H
