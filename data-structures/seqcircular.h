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

#ifndef SEQCIRCULAR
#define SEQCIRCULAR

#include "utils/default_hash.hpp"
#include "data-structures/base_circular.h"

namespace growt {

template<class Table, bool is_const = false>
class SeqIterator
{
    using Table_t         = Table;
    using cTable_t        = typename std::conditional<is_const, const Table_t, Table_t>::type;

    using key_type        = typename Table_t::key_type;
    using mapped_type     = typename Table_t::mapped_type;
    using value_intern    = typename Table_t::value_intern;
    using value_table     = std::pair<const key_type, mapped_type>;
    using cval_intern     = typename std::conditional
                           <is_const, const value_intern , value_intern >::type;
    using pointer_intern  = typename Table_t::value_intern*;

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
    friend void swap(SeqIterator<T,b>&, SeqIterator<T,b>&);
    template<class T, bool b>
    friend bool operator==(const SeqIterator<T,b>&, const SeqIterator<T,b>&);
    template<class T, bool b>
    friend bool operator!=(const SeqIterator<T,b>&, const SeqIterator<T,b>&);


    // Constructors ************************************************************

    SeqIterator(cval_intern* p, const key_type& k, size_t v, cTable_t& t)
        : _ptr(p), _key(k), _ver(v), _tab(t) { }

    SeqIterator(const SeqIterator& r) = default;
    //     : ptr(r.ptr), key(r.key), ver(r.ver), tab(r.tab) { }
    SeqIterator& operator=(const SeqIterator& r) = default;
    // { ptr = r.ptr; key = r.key; ver = r.ver; tab = r.tab; return *this; }

    ~SeqIterator() = default;


    // Basic Iterator Functionality

    inline SeqIterator& operator++()
    {
        if (_tab._version != _ver) refresh();
        while ( _ptr < _tab._t + _tab._capacity && _ptr->is_empty()) _ptr++;
        if (_ptr == _tab._t+ _tab._capacity) { _ptr = nullptr; _key = key_type(); }
        else { _key = _ptr->getKey(); }
        return *this;
    }

    inline SeqIterator& operator++(int)
    {
        SeqIterator copy(*this);
        ++(*this);
        return copy;
    }

    inline reference operator* ()
    { if (_tab._version != _ver) refresh(); return *reinterpret_cast<value_type*>(_ptr); }

    inline pointer   operator->()
    { if (_tab._version != _ver) refresh(); return  reinterpret_cast<value_type*>(_ptr); }

    inline bool operator==(const SeqIterator& r) const { return _ptr == r._ptr; }
    inline bool operator!=(const SeqIterator& r) const { return _ptr != r._ptr; }

private:
    pointer_intern _ptr;
    key_type       _key;
    size_t         _ver;
    cTable_t&      _tab;

    inline void refresh()
    {
        SeqIterator it = _tab.find(_key);
        _ptr = it._ptr;
        _ver = it._ver;
    }

};


template<class E, class HashFct = utils_tm::hash_tm::default_hash,
         class A = std::allocator<E>>
class SeqCircular : public BaseCircular<E, HashFct, A>
{
private:
    using This_t             = SeqCircular<E,HashFct,A>;
    using Base_t             = BaseCircular   <E,HashFct,A>;

public:
    using value_intern       = E;

    using key_type           = typename Base_t::key_type;
    using mapped_type        = typename Base_t::mapped_type;
    using value_type         = typename Base_t::value_type;
    using iterator           = SeqIterator<This_t, false>;
    using const_iterator     = SeqIterator<This_t, true>;
    using size_type          = typename Base_t::size_type;
    using difference_type    = typename Base_t::difference_type;
    using reference          =       std::pair<const key_type, mapped_type>&;
    using const_reference    = const std::pair<const key_type, mapped_type>&;
    using mapped_reference       = mapped_type&;
    using const_mapped_reference = const mapped_type&;
    using insert_return_type = std::pair<iterator, bool>;


    using local_iterator       = void;
    using const_local_iterator = void;
    using node_type            = void;

private:
    using BaseCircular<E,HashFct,A>::_t;
    using BaseCircular<E,HashFct,A>::_bitmask;
    using BaseCircular<E,HashFct,A>::h;
    using BaseCircular<E,HashFct,A>::_capacity;
    using BaseCircular<E,HashFct,A>::_hash;
    using BaseCircular<E,HashFct,A>::_version;
    using BaseCircular<E,HashFct,A>::_right_shift;
    static constexpr double _max_fill_factor = 0.666;

    template<class, bool>
    friend class SeqIterator;

public:

    SeqCircular(size_t size )
        : BaseCircular<E,HashFct,A>::BaseCircular(size),
          _n_elem(0), _thresh(_capacity*_max_fill_factor) {}

    SeqCircular(size_t size, size_t version)
        : BaseCircular<E,HashFct,A>::BaseCircular(size, version),
          _n_elem(0), _thresh(_capacity*_max_fill_factor) {}

    // These are used for our tests, such that SeqCircular behaves like GrowTable
    using Handle = SeqCircular<E,HashFct,A>&;
    Handle get_handle() { return *this; }

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
    iterator make_it(value_intern* p, const key_type& k)
    { return iterator(p,k,_version,*this); }
    iterator make_cit(const value_intern* p, const key_type& k) const
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
        This_t temp(_capacity << 1, _version+1);
        migrate(temp);
        swap(temp);
    }


    void swap(SeqCircular & o)
    {
        std::swap(_capacity   , o._capacity);
        std::swap(_version    , o._version);
        std::swap(_bitmask    , o._bitmask);
        std::swap(_thresh     , o._thresh);
        std::swap(_t          , o._t);
        std::swap(_hash       , o._hash);
        std::swap(_right_shift, o._right_shift);
    }

    inline size_t migrate( SeqCircular& target )
    {
        std::fill( target._t ,target._t + target._capacity , E::get_empty() );

        auto count = 0u;

        for (size_t i = 0; i < _capacity; ++i)
        {
            auto curr = _t[i];
            if ( ! curr.is_empty() )
            {
                count++;
                //target.insert( curr );
                if (!target.insert(curr.key, curr.data).second)
                {
                    std::logic_error("Unsuccessful insert during sequential migration!");
                }
            }
        }
        return count;

    }
};

template<class E, class HF, class A>
inline typename SeqCircular<E,HF,A>::iterator
SeqCircular<E,HF,A>::begin()
{
    for (size_t i = 0; i < _capacity; ++i)
    {
        auto curr = _t[i];
        if (! curr.is_empty()) return make_it(&_t[i], curr.getKey());
    }
    return end();
}

template<class E, class HF, class A>
inline typename SeqCircular<E,HF,A>::iterator
SeqCircular<E,HF,A>::end()
{
    return make_it(nullptr, key_type());
}

template<class E, class HF, class A>
inline typename SeqCircular<E,HF,A>::const_iterator
SeqCircular<E,HF,A>::cbegin() const
{
    for (size_t i = 0; i < _capacity; ++i)
    {
        auto curr = _t[i];
        if (! curr.is_empty()) return make_cit(&_t[i], curr.getKey());
    }
    return cend();
}

template<class E, class HF, class A>
inline typename SeqCircular<E,HF,A>::const_iterator
SeqCircular<E,HF,A>::cend() const
{
    return make_cit(nullptr, key_type());
}

template<class E, class HF, class A>
inline typename SeqCircular<E,HF,A>::iterator
SeqCircular<E,HF,A>::find(const key_type & k)
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)  // i < htemp+MaDis
    {
        E curr(_t[i & _bitmask]);
        if (curr.compare_key(k)) return make_it(&_t[i&_bitmask], k);
        else if (curr.is_empty()) return end();
    }
}

template<class E, class HF, class A>
inline typename SeqCircular<E,HF,A>::const_iterator
SeqCircular<E,HF,A>::find(const key_type & k) const
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)
    {
        E curr(_t[i & _bitmask]);
        if (curr.compare_key(k)) return make_cit(&_t[i&_bitmask], k);
        else if (curr.is_empty()) return cend();
    }
}

template<class E, class HF, class A>
inline typename SeqCircular<E,HF,A>::insert_return_type
SeqCircular<E,HF,A>::insert(const key_type& k, const mapped_type& d)
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)
    {
        const size_t temp = i & _bitmask;
        E curr(_t[temp]);
        if (curr.compare_key(k)) return insert_return_type(make_it(&_t[temp], k), false); // already hashed
        else if (curr.is_empty())
        {
            if (inc_n()) { _n_elem--; return insert(k,d); }
            _t[temp] = E(k,d);
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

template<class E, class HF, class A>
template<class F, class ...Types>
inline typename SeqCircular<E,HF,A>::insert_return_type
SeqCircular<E,HF,A>::update(const key_type& k, F f, Types&& ... args)
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)
    {
        const size_t temp = i & _bitmask;
        E curr(_t[temp]);
        if (curr.compare_key(k))
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

template<class E, class HF, class A>
template<class F, class ...Types>
inline typename SeqCircular<E,HF,A>::insert_return_type
SeqCircular<E,HF,A>::insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)
    {
        const size_t temp = i & _bitmask;
        E curr(_t[temp]);
        if (curr.compare_key(k))
        {
            _t[temp].non_atomic_update(f, std::forward<Types>(args) ...);
            return insert_return_type(make_it(&_t[temp], k), false);
        }
        else if (curr.is_empty())
        {
            if (inc_n()) { _n_elem--; return insert(k,d); }
            _t[temp] = E(k,d);
            return insert_return_type(make_it(&_t[temp], k), true);
        }
        else if (curr.is_deleted())
        {
            //do something appropriate
        }
    }
}


template<class E, class HF, class A>
inline typename SeqCircular<E,HF,A>::size_type
SeqCircular<E,HF,A>::erase(const key_type & k)
{
    size_type i = h(k);
    for (;;++i)
    {
        E curr(_t[i & _bitmask]);
        if (curr.compare_key(k)) break;
        else if (curr.is_empty()) return 0;
    }
    i &= _bitmask;
    _t[i] = value_intern::get_empty();
    for (size_type j = i+1;; ++j)
    {
        E curr(_t[j & _bitmask]);
        if (curr.is_empty())
            return 1;
        else if (h(curr.getKey()) <= i)
        {
            _t[i] = curr;
            _t[j&_bitmask] = value_intern::get_empty();
            i = j & _bitmask;
            if (j > _bitmask) j &= _bitmask;
        }
    }
}


}

#endif // SEQCIRCULAR
