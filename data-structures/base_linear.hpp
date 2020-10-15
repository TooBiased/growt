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

#include "utils/default_hash.hpp"
#include "data-structures/returnelement.hpp"
#include "data-structures/base_iterator.hpp"
#include "example/update_fcts.hpp"

namespace growt
{

template <bool CyclicMap, bool CyclicProb>
class pow2_mapper
{
private:
    // capacity is at least twice as large, as the inserted capacity
    static size_t compute_capacity(size_t desired_capacity)
    {
        auto temp = 16384u;
        while (temp < desired_capacity) temp <<= 1;
        return temp << 1;
    }

    static size_t compute_right_shift(size_t capacity)
    {
        size_t log_size = 0;
        while (capacity >>= 1) log_size++;
        return 64 - log_size;                    // HashFct::significant_digits
    }

public:
    pow2_mapper() : capacity(0), bitmask(0), right_shift(0) { }
    pow2_mapper(size_t capacity_, bool migrated_ = false)
        : capacity(migrated_ ? capacity_ : compute_capacity(capacity_)),
          bitmask(capacity-1),
          right_shift(compute_right_shift(capacity))
    {
        if constexpr (!cyclic_probing) capacity += 256;
    }

    size_t capacity;
    size_t bitmask;
    size_t right_shift;
    static constexpr bool cyclic_mapping = CyclicMap;
    static constexpr bool cyclic_probing = CyclicProb;

    inline size_t map (size_t hashed) const
    {
        if constexpr (cyclic_mapping)
            return hashed & bitmask;
        else
            return hashed >> right_shift;
    }

    inline size_t remap(size_t hashed) const
    {
        if constexpr (cyclic_probing)
            return hashed & bitmask;
        else
            return hashed;
    }

    inline pow2_mapper resize(size_t inserted, size_t deleted)
    {
        auto nsize = bitmask+1;
        double fill_rate = double(inserted - deleted)/double(capacity);

        if (fill_rate > 0.6/2.) nsize <<= 1;

        return pow2_mapper(nsize, true);
    }

};


template<class E, class HashFct = utils_tm::hash_tm::default_hash,
         class A = std::allocator<typename E::atomic_slot_type>,
         class M = pow2_mapper<true, false>>
class base_linear
{
private:
    using this_type          = base_linear<E,HashFct,A>;
    using allocator_type     = typename A::template rebind<
        typename E::atomic_slot_type>::other;
    using mapper_type        = M;

    template <class> friend class migration_table_handle;

public:
    using slot_config            = E;
    using slot_type              = typename E::slot_type;
    using atomic_slot_type       = typename E::atomic_slot_type;

    using key_type               = typename E::key_type;
    using mapped_type            = typename E::mapped_type;
    using value_type             = typename E::value_type;
    using iterator               = base_iterator<this_type, false>;
    using const_iterator         = base_iterator<this_type, true>;
    using size_type              = size_t;
    using difference_type        = std::ptrdiff_t;
    using reference              = base_reference<this_type, false>;
    using const_reference        = base_reference<this_type, true>;
    using mapped_reference       = base_mapped_reference<this_type, false>;
    using const_mapped_reference = base_mapped_reference<this_type, true>;
    using insert_return_type     = std::pair<iterator, bool>;


    using local_iterator         = void;
    using const_local_iterator   = void;
    using node_type              = void;

    using handle_type            = this_type&;

private:
    using insert_return_intern = std::pair<iterator, ReturnCode>;

public:
    base_linear(size_type size_ = 1<<18);
    base_linear(mapper_type mapper_, size_type version_);

    base_linear(const base_linear&) = delete;
    base_linear& operator=(const base_linear&) = delete;

    // Obviously move-constructor and move-assignment are not thread safe
    // They are merely comfort functions used during setup
    base_linear(base_linear&& rhs) noexcept;
    base_linear& operator=(base_linear&& rhs) noexcept;

    ~base_linear();

    handle_type get_handle() { return *this; }

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
    { return insert_or_update(k, d, example::Overwrite(), d); }

    mapped_reference operator[](const key_type& k)
    { return (*(insert(k, mapped_type()).first)).second; }

    template <class F, class ... Types>
    insert_return_type update
    (const key_type& k, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_type update_unsafe
    (const key_type& k, F f, Types&& ... args);


    template <class F, class ... Types>
    insert_return_type insert_or_update
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_type insert_or_update_unsafe
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);

    size_type          erase_if(const key_type& k, const mapped_type& d);

    size_type migrate(this_type& target, size_type s, size_type e);

protected:
    mapper_type        _mapper;
    size_type          _version;
    std::atomic_size_t _current_copy_block;
    HashFct            _hash;
    allocator_type     _allocator;

    //size_type   _capacity;
    //size_type   _bitmask;
    //size_type   _right_shift;

    static_assert(std::is_same<typename allocator_type::value_type,
                               atomic_slot_type>::value,
                  "Wrong allocator type given to base_linear!");

    atomic_slot_type* _t;
    inline size_type h    (const key_type & k) const { return _hash(k); }
    // inline size_type map  (const size_type & hashed) const
    // { return hashed >> _right_shift; }
    // inline size_type remap(const size_type & hashed) const
    // {
    //     if constexpr (circular)
    //         return hashed && _bitmask;
    //     else
    //         return hashed;
    // }

private:
    insert_return_intern insert_intern(const key_type& k, const mapped_type& d);
    ReturnCode           erase_intern (const key_type& k);
    ReturnCode           erase_if_intern (const key_type& k, const mapped_type& d);


    template <class F, class ... Types>
    insert_return_intern update_intern
    (const key_type& k, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_intern update_unsafe_intern
    (const key_type& k, F f, Types&& ... args);


    template <class F, class ... Types>
    insert_return_intern insert_or_update_intern
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);

    template <class F, class ... Types>
    insert_return_intern insert_or_update_unsafe_intern
    (const key_type& k, const mapped_type& d, F f, Types&& ... args);




    // HELPER FUNCTION FOR ITERATOR CREATION ***********************************

    inline iterator           make_iterator (const key_type& k, const mapped_type& d, atomic_slot_type* ptr)
    { return iterator(std::make_pair(k,d), ptr, _t+_mapper.capacity); }

    inline const_iterator     make_citerator (const key_type& k, const mapped_type& d, atomic_slot_type* ptr) const
    { return const_iterator(std::make_pair(k,d), ptr, _t+_mapper.capacity); }

    inline insert_return_type make_insert_ret(const key_type& k, const mapped_type& d, atomic_slot_type* ptr, bool succ)
    { return std::make_pair(make_iterator(k,d, ptr), succ); }

    inline insert_return_type make_insert_ret(iterator it, bool succ)
    { return std::make_pair(it, succ); }

    inline insert_return_intern make_insert_ret(const key_type& k, const mapped_type& d, atomic_slot_type* ptr, ReturnCode code)
    { return std::make_pair(make_iterator(k,d, ptr), code); }

    inline insert_return_intern make_insert_ret(iterator it, ReturnCode code)
    { return std::make_pair(it, code); }




    // OTHER HELPER FUNCTIONS **************************************************

    void insert_unsafe(const slot_type& e);
    void slot_cleanup () // called, either by the destructor, or by the destructor of the parenttable
    { for (size_t i = 0; i<_mapper.capacity; ++i) _t[i].load().cleanup(); }

public:
    using range_iterator = iterator;
    using const_range_iterator = const_iterator;

    /* size has to divide capacity */
    range_iterator       range (size_t rstart, size_t rend);
    const_range_iterator crange(size_t rstart, size_t rend);
    range_iterator       range_end ()       { return  end(); }
    const_range_iterator range_cend() const { return cend(); }
    size_t               capacity()   const { return _mapper.capacity; }

};









// CONSTRUCTORS/ASSIGNMENTS ****************************************************

template<class E, class H, class A, class M>
base_linear<E,H,A,M>::base_linear(size_type capacity_)
    : _mapper(capacity_),
      _version(0),
      _current_copy_block(0)
{
    _t = _allocator.allocate(_mapper.capacity);
    if ( !_t ) std::bad_alloc();

    std::fill( _t ,_t + _mapper.capacity , E::get_empty() );
}

/*should always be called with a capacity_=2^k  */
template<class E, class H, class A, class M>
base_linear<E,H,A,M>::base_linear(mapper_type mapper_, size_type version_)
    : _mapper(mapper_),
      _version(version_),
      _current_copy_block(0)
{
    _t = _allocator.allocate(_mapper.capacity);
    if ( !_t ) std::bad_alloc();

    /* The table is initialized in parallel, during the migration */
    // std::fill( _t ,_t + _mapper.capacity , value_intern::get_empty() );
}

template<class E, class H, class A, class M>
base_linear<E,H,A,M>::~base_linear()
{
    // if constexpr (slot_config::needs_cleanup && !base_table)
    // {
    //     slot_cleanup()
    // }
    if (_t) _allocator.deallocate(_t, _mapper.capacity);
}


template<class E, class H, class A, class M>
base_linear<E,H,A,M>::base_linear(base_linear&& rhs) noexcept
    : _mapper(rhs._mapper), _version(rhs._version),
      _current_copy_block(0), _t(nullptr)
{
    if (rhs._current_copy_block.load())
        std::invalid_argument("Cannot move a growing table!");
    rhs._mapper = mapper_type();
    std::swap(_t, rhs._t);
}

template<class E, class H, class A, class M>
base_linear<E,H,A,M>&
base_linear<E,H,A,M>::operator=(base_linear&& rhs) noexcept
{
    if (this == &rhs) return *this;
    this->~base_linear();
    new (this) base_linear(std::move(rhs));
    return *this;
}








// ITERATOR FUNCTIONALITY ******************************************************

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::iterator
base_linear<E,H,A,M>::begin()
{
    for (size_t i = 0; i<_mapper.capacity; ++i)
    {
        auto temp = _t[i].load();
        if (!temp.is_empty() && !temp.is_deleted())
            return make_iterator(temp.get_key(), temp.get_mapped(), &_t[i]);
    }
    return end();
}

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::iterator
base_linear<E,H,A,M>::end()
{ return iterator(std::make_pair(key_type(), mapped_type()),nullptr,nullptr); }


template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::const_iterator
base_linear<E,H,A,M>::cbegin() const
{
    for (size_t i = 0; i<_mapper.capacity; ++i)
    {
        auto temp = _t[i].load();
        if (!temp.is_empty() && !temp.is_deleted())
            return make_citerator(temp.get_key(), temp.get_mapped(), &_t[i]);
    }
    return end();
}

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::const_iterator
base_linear<E,H,A,M>::cend() const
{
    return const_iterator(std::make_pair(key_type(),mapped_type()),
                          nullptr,nullptr);
}


// RANGE ITERATOR FUNCTIONALITY ************************************************

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::range_iterator
base_linear<E,H,A,M>::range(size_t rstart, size_t rend)
{
    auto temp_rend = std::min(rend, _mapper.capacity);
    for (size_t i = rstart; i < temp_rend; ++i)
    {
        auto temp = _t[i].load();
        if (!temp.is_empty() && !temp.is_deleted())
            return range_iterator(std::make_pair(temp.get_key(),
                                                 temp.get_mapped()),
                                  &_t[i], &_t[temp_rend]);
    }
    return range_end();
}

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::const_range_iterator
base_linear<E,H,A,M>::crange(size_t rstart, size_t rend)
{
    auto temp_rend = std::min(rend, _mapper.capacity);
    for (size_t i = rstart; i < temp_rend; ++i)
    {
        auto temp = _t[i].load();
        if (!temp.is_empty() && !temp.is_deleted())
            return const_range_iterator(std::make_pair(temp.get_key(),
                                                       temp.get_mapped()),
                                  &_t[i], &_t[temp_rend]);
    }
    return range_cend();
}



// MAIN HASH TABLE FUNCTIONALITY (INTERN) **************************************

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::insert_return_intern
base_linear<E,H,A,M>::insert_intern(const key_type& k,
                                         const mapped_type& d)
{
    size_type htemp = h(k);

    for (size_type i = _mapper.map(htemp); ; ++i) //i < htemp+MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();

        if (curr.is_marked())
        {
            return make_insert_ret(end(),
                                   ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.is_empty())
        {
            if ( _t[temp].cas(curr, slot_type(k,d,htemp)) )
                return make_insert_ret(k,d, &_t[temp],
                                       ReturnCode::SUCCESS_IN);

            //somebody changed the current element! recheck it
            --i;
        }
        else if (curr.compare_key(k, htemp))
        {
            return make_insert_ret(k, curr.get_mapped(), &_t[temp],
                                   ReturnCode::UNSUCCESS_ALREADY_USED);
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
    return make_insert_ret(end(), ReturnCode::UNSUCCESS_FULL);
}


template<class E, class H, class A, class M> template<class F, class ... Types>
inline typename base_linear<E,H,A,M>::insert_return_intern
base_linear<E,H,A,M>::update_intern(const key_type& k, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = _mapper.map(htemp); ; ++i) //i < htemp+MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr =_t[temp].load();
        if (curr.is_marked())
        {
            return make_insert_ret(end(),
                                   ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.is_empty())
        {
            return make_insert_ret(end(),
                                   ReturnCode::UNSUCCESS_NOT_FOUND);
        }
        else if (curr.compare_key(k, htemp))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = _t[temp].atomic_update(curr,f,
                                                          std::forward<Types>(args)...);
            if (succ)
                return make_insert_ret(k,data, &_t[temp],
                                       ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
    return make_insert_ret(end(),
                           ReturnCode::UNSUCCESS_NOT_FOUND);
}

template<class E, class H, class A, class M> template<class F, class ... Types>
inline typename base_linear<E,H,A,M>::insert_return_intern
base_linear<E,H,A,M>::update_unsafe_intern(const key_type& k, F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = _mapper.map(htemp); ; ++i) //i < htemp+MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp];
        if (curr.is_marked())
        {
            return make_insert_ret(end(),
                                   ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.is_empty())
        {
            return make_insert_ret(end(),
                                   ReturnCode::UNSUCCESS_NOT_FOUND);
        }
        else if (curr.compare_key(k, htemp))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = _t[temp].non_atomic_update(f,
                                                            std::forward<Types>(args)...);
            if (succ)
                return make_insert_ret(k,data, &_t[temp],
                                       ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
    return make_insert_ret(end(), ReturnCode::UNSUCCESS_NOT_FOUND);
}


template<class E, class H, class A, class M> template<class F, class ... Types>
inline typename base_linear<E,H,A,M>::insert_return_intern
base_linear<E,H,A,M>::insert_or_update_intern(const key_type& k,
                                                   const mapped_type& d,
                                                   F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = _mapper.map(htemp); ; ++i) //i < htemp+MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();
        if (curr.is_marked())
        {
            return make_insert_ret(end(), ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.is_empty())
        {
            if ( _t[temp].cas(curr, slot_type(k,d,htemp)) )
                return make_insert_ret(k,d, &_t[temp],
                                       ReturnCode::SUCCESS_IN);

            //somebody changed the current element! recheck it
            --i;
        }
        else if (curr.compare_key(k, htemp))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = _t[temp].atomic_update(curr, f,
                                                          std::forward<Types>(args)...);
            if (succ)
                return make_insert_ret(k,data, &_t[temp],
                                       ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
    return make_insert_ret(end(), ReturnCode::UNSUCCESS_FULL);
}

template<class E, class H, class A, class M> template<class F, class ... Types>
inline typename base_linear<E,H,A,M>::insert_return_intern
base_linear<E,H,A,M>::insert_or_update_unsafe_intern(const key_type& k,
                                                          const mapped_type& d,
                                                          F f, Types&& ... args)
{
    size_type htemp = h(k);

    for (size_type i = _mapper.map(htemp); ; ++i) //i < htemp+MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();
        if (curr.is_marked())
        {
            return make_insert_ret(end(),
                                   ReturnCode::UNSUCCESS_INVALID);
        }
        else if (curr.is_empty())
        {
            if ( _t[temp].cas(curr, slot_type(k,d,htemp)) )
                return make_insert_ret(k,d, &_t[temp],
                                       ReturnCode::SUCCESS_IN);
            //somebody changed the current element! recheck it
            --i;
        }
        else if (curr.compare_key(k, htemp))
        {
            mapped_type data;
            bool        succ;
            std::tie(data, succ) = _t[temp].non_atomic_update(f,
                                                            std::forward<Types>(args)...);
            if (succ)
                return make_insert_ret(k,data, &_t[temp],
                                       ReturnCode::SUCCESS_UP);
            i--;
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
    return make_insert_ret(end(),
                           ReturnCode::UNSUCCESS_FULL);
}

template<class E, class H, class A, class M>
inline ReturnCode base_linear<E,H,A,M>::erase_intern(const key_type& k)
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp); ; ++i) //i < htemp+MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();
        if (curr.is_marked())
        {
            return ReturnCode::UNSUCCESS_INVALID;
        }
        else if (curr.is_empty())
        {
            return ReturnCode::UNSUCCESS_NOT_FOUND;
        }
        else if (curr.compare_key(k, htemp))
        {
            if (_t[temp].atomic_delete(curr))
                return ReturnCode::SUCCESS_DEL;
            i--;
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }

    return ReturnCode::UNSUCCESS_NOT_FOUND;
}

template<class E, class H, class A, class M>
inline ReturnCode base_linear<E,H,A,M>::erase_if_intern(const key_type& k, const mapped_type& d)
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp); ; ++i) //i < htemp+MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();
        if (curr.is_marked())
        {
            return ReturnCode::UNSUCCESS_INVALID;
        }
        else if (curr.is_empty())
        {
            return ReturnCode::UNSUCCESS_NOT_FOUND;
        }
        else if (curr.compare_key(k, htemp))
        {
            if (curr.get_mapped() != d) return ReturnCode::UNSUCCESS_NOT_FOUND;

            if (_t[temp].atomic_delete(curr))
                return ReturnCode::SUCCESS_DEL;
            i--;
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }

    return ReturnCode::UNSUCCESS_NOT_FOUND;
}





// MAIN HASH TABLE FUNCTIONALITY (EXTERN) **************************************

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::iterator
base_linear<E,H,A,M>::find(const key_type& k)
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp); ; ++i)
    {
        auto temp = _mapper.remap(i);
        auto curr = _t[temp].load();
        if (curr.is_empty())
            return end();
        if (curr.compare_key(k, htemp))
            return make_iterator(k, curr.get_mapped(), &_t[temp]);
    }
    return end();
}

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::const_iterator
base_linear<E,H,A,M>::find(const key_type& k) const
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp); ; ++i)
    {
        auto temp = _mapper.remap(i);
        auto curr = _t[temp].load;
        if (curr.is_empty())
            return cend();
        if (curr.compare_key(k, htemp))
            return make_citerator(k, curr.get_mapped(), &_t[temp]);
    }
    return cend();
}

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::insert_return_type
base_linear<E,H,A,M>::insert(const key_type& k, const mapped_type& d)
{
    auto[it,rcode] = insert_intern(k,d);
    return std::make_pair(it, successful(rcode));
}

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::size_type
base_linear<E,H,A,M>::erase(const key_type& k)
{
    ReturnCode c = erase_intern(k);
    return (successful(c)) ? 1 : 0;
}

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::size_type
base_linear<E,H,A,M>::erase_if(const key_type& k, const mapped_type& d)
{
    ReturnCode c = erase_if_intern(k,d);
    return (successful(c)) ? 1 : 0;
}

template<class E, class H, class A, class M> template <class F, class ... Types>
inline typename base_linear<E,H,A,M>::insert_return_type
base_linear<E,H,A,M>::update(const key_type& k, F f, Types&& ... args)
{
    auto[it,rcode] = update_intern(k,f, std::forward<Types>(args)...);
    return std::make_pair(it, successful(rcode));
}

template<class E, class H, class A, class M> template <class F, class ... Types>
inline typename base_linear<E,H,A,M>::insert_return_type
base_linear<E,H,A,M>::update_unsafe(const key_type& k, F f, Types&& ... args)
{
    auto[it,rcode] = update_unsafe_intern(k,f, std::forward<Types>(args)...);
    return std::make_pair(it, successful(rcode));
}

template<class E, class H, class A, class M> template <class F, class ... Types>
inline typename base_linear<E,H,A,M>::insert_return_type
base_linear<E,H,A,M>::insert_or_update(const key_type& k,
                                            const mapped_type& d,
                                            F f, Types&& ... args)
{
    auto[it,rcode] = insert_or_update_intern(k,d,f,
                                             std::forward<Types>(args)...);
    return std::make_pair(it, (rcode == ReturnCode::SUCCESS_IN));
}

template<class E, class H, class A, class M> template <class F, class ... Types>
inline typename base_linear<E,H,A,M>::insert_return_type
base_linear<E,H,A,M>::insert_or_update_unsafe(const key_type& k,
                                                   const mapped_type& d,
                                                   F f, Types&& ... args)
{
    auto[it,rcode] = insert_or_update_unsafe_intern(k,d,f,
                                                    std::forward<Types>(args)...);
    return std::make_pair(it, (rcode == ReturnCode::SUCCESS_IN));
}












// MIGRATION/GROWING STUFF *****************************************************

// TRIVIAL MIGRATION (ASSUMES INITIALIZED TABLE)
// template<class E, class H, class A, class M>
// inline typename base_linear<E,H,A,M>::size_type
// base_linear<E,H,A,M>::migrate(this_type& target, size_type s, size_type e)
// {
//     size_type count = 0;
//     for (size_t i = s; i < e; ++i)
//     {
//         auto curr = _t[i];
//         while (! _t[i].atomic_mark(curr))
//         { /* retry until we successfully mark the element */ }

//         target.insert(curr.get_key(), curr.get_mapped());
//         ++count;
//     }

//     return count;
// }

template<class E, class H, class A, class M>
inline typename base_linear<E,H,A,M>::size_type
base_linear<E,H,A,M>::migrate(this_type& target, size_type s, size_type e)
{
    size_type n = 0;
    auto i = s;
    auto curr = E::get_empty();

    //HOW MUCH BIGGER IS THE TARGET TABLE
    auto shift = 0u;
    while (target._mapper.capacity > (_mapper.capacity << shift)) ++shift;


    //FINDS THE FIRST EMPTY BUCKET (START OF IMPLICIT BLOCK)
    while (i<e)
    {
        curr = _t[i].load();           //no bitmask necessary (within one block)
        if (curr.is_empty())
        {
            if (_t[i].atomic_mark(curr)) break;
            else --i;
        }
        ++i;
    }

    std::fill(target._t+(i<<shift), target._t+(e<<shift), E::get_empty());

    //MIGRATE UNTIL THE END OF THE BLOCK
    for (; i<e; ++i)
    {
        curr = _t[i].load();
        if (! _t[i].atomic_mark(curr))
        {
            --i;
            continue;
        }
        else if (! curr.is_empty())
        {
            if (!curr.is_deleted())
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
        auto pos  = _mapper.remap(i);
        auto t_pos= pos<<shift;
        for (size_type j = 0; j < 1ull<<shift; ++j)
            target._t[t_pos+j].non_atomic_set(E::get_empty());
        //target.t[t_pos] = E::get_empty();

        curr = _t[pos].load();

        if (! _t[pos].atomic_mark(curr)) --i;
        if ( (b = ! curr.is_empty()) ) // this might be nicer as an else if, but this is faster
        {
            if (!curr.is_deleted()) { target.insert_unsafe(curr); n++; }
        }
    }

    return n;
}

template<class E, class H, class A, class M>
inline void base_linear<E,H,A,M>::insert_unsafe(const slot_type& e)
{
    const key_type k = e.get_key();

    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp); ; ++i)  // i < htemp + MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();

        if (curr.is_empty())
        {
            _t[temp].non_atomic_set(e);
            return;
        }
    }
    throw std::bad_alloc();
}

}
