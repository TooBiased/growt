/*******************************************************************************
 * data-structures/seqcircular.h
 *
 * A Sequential variant of our non growing table with an added growing function.
 * All atomics are substituted for faster alternatives.
 * This hash table is mostly used to generate baselines for speedup plots.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#pragma once

#include "utils/default_hash.hpp"
#include "data-structures/base_linear.hpp"

namespace growt {

template<class Table, bool is_const = false>
class seq_iterator
{
    using table_type         = Table;
    using const_table_type   = typename std::conditional<is_const, const table_type, table_type>::type;

    using key_type        = typename table_type::key_type;
    using mapped_type     = typename table_type::mapped_type;
    using slot_config     = typename table_type::slot_config;
    using slot_type       = typename slot_config::slot_type;
    using atomic_slot_type = typename slot_config::atomic_slot_type;
    using value_table     = std::pair<const key_type, mapped_type>;
    using cval_intern     = typename std::conditional
                           <is_const, const slot_type , slot_type >::type;
    using pointer_intern  = typename table_type::atomic_slot_type*;

public:
    using difference_type = std::ptrdiff_t;
    using value_type      = typename std::conditional
                               <is_const, const value_table, value_table>::type;
    using reference       = typename std::conditional
                               <is_const, const value_type&, value_type&>::type;
    using pointer         = typename std::conditional
                               <is_const, const value_type*, value_type*>::type;
    using iterator_category = std::forward_iterator_tag;

    template<class T, bool b>
    friend void swap(seq_iterator<T,b>&, seq_iterator<T,b>&);
    template<class T, bool b>
    friend bool operator==(const seq_iterator<T,b>&, const seq_iterator<T,b>&);
    template<class T, bool b>
    friend bool operator!=(const seq_iterator<T,b>&, const seq_iterator<T,b>&);


    // Constructors ************************************************************

    seq_iterator(pointer_intern p, const key_type& k, size_t v, const_table_type& t)
        : _ptr(p), _key(k), _ver(v), _tab(t) { }

    seq_iterator(const seq_iterator& r) = default;
    seq_iterator& operator=(const seq_iterator& r) = default;

    ~seq_iterator() = default;


    // Basic Iterator Functionality

    inline seq_iterator& operator++()
    {
        if (_tab._version != _ver) refresh();
        while ( _ptr < _tab._t + _tab._mapper.capacity && _ptr->is_empty()) _ptr++;
        if (_ptr == _tab._t+ _tab._mapper.capacity) { _ptr = nullptr; _key = key_type(); }
        else { _key = _ptr->get_key(); }
        return *this;
    }

    inline seq_iterator& operator++(int)
    {
        seq_iterator copy(*this);
        ++(*this);
        return copy;
    }

    inline reference operator* ()
    { if (_tab._version != _ver) refresh(); return *reinterpret_cast<value_type*>(_ptr); }

    inline pointer   operator->()
    { if (_tab._version != _ver) refresh(); return  reinterpret_cast<value_type*>(_ptr); }

    inline bool operator==(const seq_iterator& r) const { return _ptr == r._ptr; }
    inline bool operator!=(const seq_iterator& r) const { return _ptr != r._ptr; }

private:
    pointer_intern    _ptr;
    key_type          _key;
    size_t            _ver;
    const_table_type& _tab;

    inline void refresh()
    {
        seq_iterator it = _tab.find(_key);
        _ptr = it._ptr;
        _ver = it._ver;
    }

};



template<class Slot,
         class HashFct = utils_tm::hash_tm::default_hash,
         class Alloc = std::allocator<typename Slot::atomic_slot_type>,
         bool CyclicMap    = false,
         bool CyclicProb   = true,
         bool NeedsCleanup = true>
class seq_linear_parameters
{
public:
    using base_config = base_linear_config<Slot, HashFct, Alloc,
                                           CyclicMap, CyclicProb, NeedsCleanup>;
    using base_table  = base_linear<base_config>;
};


template <class Config>
class seq_linear : public Config::base_table
{
private:
    using this_type          = seq_linear<Config>;
    using base_type          = typename Config::base_table;

public:
    using mapper_type        = typename base_type::mapper_type;
    using slot_config        = typename base_type::slot_config;
    using slot_type          = typename slot_config::slot_type;
    using atomic_slot_type   = typename slot_config::atomic_slot_type;

    using key_type           = typename base_type::key_type;
    using mapped_type        = typename base_type::mapped_type;
    using value_type         = typename base_type::value_type;
    using iterator           = seq_iterator<this_type, false>;
    using const_iterator     = seq_iterator<this_type, true>;
    using size_type          = typename base_type::size_type;
    using difference_type    = typename base_type::difference_type;
    using reference          =       std::pair<const key_type, mapped_type>&;
    using const_reference    = const std::pair<const key_type, mapped_type>&;
    using mapped_reference       = mapped_type&;
    using const_mapped_reference = const mapped_type&;
    using insert_return_type = std::pair<iterator, bool>;


    using local_iterator       = void;
    using const_local_iterator = void;
    using node_type            = void;

private:
    using base_type::_t;
    // using base_type::_bitmask;
    using base_type::h;
    using base_type::_mapper;
    // using base_type::_capacity;
    using base_type::_hash;
    using base_type::_version;
    // using base_type::_right_shift;
    static constexpr double _max_fill_factor = 0.666;

    template<class, bool>
    friend class seq_iterator;

public:

    seq_linear(size_t size )
        : base_type(size),
          _n_elem(0), _thresh(_mapper.capacity*_max_fill_factor) {}

    seq_linear(mapper_type mapper, size_t version)
        : base_type(mapper, version),
          _n_elem(0), _thresh(_mapper.capacity*_max_fill_factor) {}

    // These are used for our tests, such that seq_linear behaves like grow_table
    using handle_type = this_type&;
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

    template<class F, class ... Types>
    insert_return_type update(const key_type& k, F f, Types&& ... args);
    template<class F, class ... Types>
    insert_return_type insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args);

private:
    iterator make_it(atomic_slot_type* p, const key_type& k)
    { return iterator(p,k,_version,*this); }
    iterator make_cit(const atomic_slot_type* p, const key_type& k) const
    { return const_iterator(p,k,_version,*this); }

    size_t _n_elem;
    size_t _thresh;

    inline bool inc_n()
    {
        _n_elem += 1;
        if (_n_elem > _thresh)
        {
            grow();
            return true;
        }
        return false;
    }

    inline void grow()
    {
        auto new_mapper = _mapper.resize(_n_elem, 0);
        this_type temp(new_mapper, _version+1);
        migrate(temp);
        swap(temp);
    }


    void swap(seq_linear & o)
    {
        std::swap(_t          , o._t);
        std::swap(_mapper     , o._mapper);
        std::swap(_version    , o._version);
        std::swap(_thresh     , o._thresh);
        std::swap(_hash       , o._hash);
    }

    inline size_t migrate( seq_linear& target )
    {
        std::fill( target._t ,target._t + target._mapper.capacity , slot_config::get_empty() );

        auto count = 0u;

        for (size_t i = 0; i < _mapper.capacity; ++i)
        {
            auto curr = _t[i].load();
            if ( ! curr.is_empty() )
            {
                count++;
                //target.insert( curr );
                if (!target.insert(curr.get_key(), curr.get_mapped()).second)
                {
                    std::logic_error("Unsuccessful insert during sequential migration!");
                }
            }
        }
        return count;

    }
};


template <class C>
inline typename seq_linear<C>::iterator
seq_linear<C>::begin()
{
    for (size_t i = 0; i < _mapper.capacity; ++i)
    {
        auto curr = _t[i];
        if (! curr.is_empty()) return make_it(&_t[i], curr.get_key());
    }
    return end();
}

template <class C>
inline typename seq_linear<C>::iterator
seq_linear<C>::end()
{
    return make_it(nullptr, key_type());
}

template <class C>
inline typename seq_linear<C>::const_iterator
seq_linear<C>::cbegin() const
{
    for (size_t i = 0; i < _mapper.capacity; ++i)
    {
        auto curr = _t[i];
        if (! curr.is_empty()) return make_cit(&_t[i], curr.get_key());
    }
    return cend();
}

template <class C>
inline typename seq_linear<C>::const_iterator
seq_linear<C>::cend() const
{
    return make_cit(nullptr, key_type());
}

template <class C>
inline typename seq_linear<C>::iterator
seq_linear<C>::find(const key_type & k)
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp);;++i)  // i < htemp+MaDis
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();
        if (curr.compare_key(k,htemp)) return make_it(&_t[temp], k);
        else if (curr.is_empty()) return end();
    }
}

template <class C>
inline typename seq_linear<C>::const_iterator
seq_linear<C>::find(const key_type & k) const
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp);;++i)
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();
        if (curr.compare_key(k,htemp)) return make_cit(&_t[temp], k);
        else if (curr.is_empty()) return cend();
    }
}

template <class C>
inline typename seq_linear<C>::insert_return_type
seq_linear<C>::insert(const key_type& k, const mapped_type& d)
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp);;++i)
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();

        if (curr.compare_key(k,htemp))
            return insert_return_type(make_it(&_t[temp], k), false); // already hashed
        else if (curr.is_empty())
        {
            if (inc_n()) { _n_elem--; return insert(k,d); }
            _t[temp].non_atomic_set(slot_type(k,d,htemp));
            return insert_return_type(make_it(&_t[temp], k), true);
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
    // grow();
    // return insert(e);
}

template <class C>
template<class F, class ...Types>
inline typename seq_linear<C>::insert_return_type
seq_linear<C>::update(const key_type& k, F f, Types&& ... args)
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp);;++i)
    {
        size_t temp = _mapper.remap(i);
        slot_type curr = _t[temp].load();
        if (curr.compare_key(k,htemp))
        {
            _t[temp].non_atomic_update(f, std::forward<Types>(args)...);
            // return ReturnCode::SUCCESS_UP;
            return insert_return_type(make_it(&_t[temp], k), true);
        }
        else if (curr.is_empty())
        {
            return insert_return_type(end(), false);
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
}

template <class C>
template<class F, class ...Types>
inline typename seq_linear<C>::insert_return_type
seq_linear<C>::insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    size_type htemp = h(k);
    for (size_type i = _mapper.map(htemp);;++i)
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();

        if (curr.compare_key(k,htemp))
        {
            _t[temp].non_atomic_update(f, std::forward<Types>(args) ...);
            return insert_return_type(make_it(&_t[temp], k), false);
        }
        else if (curr.is_empty())
        {
            if (inc_n()) { _n_elem--; return insert(k,d); }
            _t[temp].non_atomic_set(slot_type(k,d,htemp));
            return insert_return_type(make_it(&_t[temp], k), true);
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
}


template <class C>
inline typename seq_linear<C>::size_type
seq_linear<C>::erase(const key_type & k)
{
    size_type htemp = h(k);
    size_type i = _mapper.map(htemp);
    for (;;++i)
    {
        size_type temp = _mapper.remap(i);
        auto curr = _t[temp].load();
        if (curr.compare_key(k,htemp)) break;
        else if (curr.is_empty()) return 0;
    }
    i &= _mapper.remap(i);
    _t[i] = slot_config::get_empty();
    for (size_type j = i+1;; ++j)
    {
        size_type temp = _mapper.remap(j);
        slot_type curr = _t[temp].load();
        if (curr.is_empty())
            return 1;
        else if (h(curr.get_key()) <= i)
        {
            _t[i] = curr;
            _t[temp] = slot_config::get_empty();
            i = temp;
            j = temp;
        }
    }
}


}
