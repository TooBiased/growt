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
        : ptr(p), key(k), ver(v), tab(t) { }

    SeqIterator(const SeqIterator& r) = default;
    //     : ptr(r.ptr), key(r.key), ver(r.ver), tab(r.tab) { }
    SeqIterator& operator=(const SeqIterator& r) = default;
    // { ptr = r.ptr; key = r.key; ver = r.ver; tab = r.tab; return *this; }

    ~SeqIterator() = default;


    // Basic Iterator Functionality

    inline SeqIterator& operator++(int = 0)
    {
        if (tab._version != ver) refresh();
        while ( ptr < tab.t + tab._capacity && ptr->isEmpty()) ptr++;
        if (ptr == tab.t+ tab._capacity) { ptr = nullptr; key = key_type(); }
        else { key = ptr->getKey(); }
        return *this;
    }

    inline reference operator* ()
    { if (tab._version != ver) refresh(); return *reinterpret_cast<value_type*>(ptr); }

    inline pointer   operator->()
    { if (tab._version != ver) refresh(); return  reinterpret_cast<value_type*>(ptr); }

    inline bool operator==(const SeqIterator& r) const { return ptr == r.ptr; }
    inline bool operator!=(const SeqIterator& r) const { return ptr != r.ptr; }

private:
    pointer_intern ptr;
    key_type       key;
    size_t         ver;
    cTable_t&      tab;

    inline void refresh()
    {
        SeqIterator it = tab.find(key);
        ptr = it.ptr;
        ver = it.ver;
    }

};


template<class E, class HashFct = std::hash<typename E::key_type>,
         class A = std::allocator<E>,
         size_t MaDis = 128, size_t MiSt = 200>
class SeqCircular : public BaseCircular<E, HashFct, A, MaDis,MiSt>
{
private:
    using This_t             = SeqCircular<E,HashFct,A,MaDis,MiSt>;
    using Base_t             = BaseCircular   <E,HashFct,A,MaDis,MiSt>;

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
    using BaseCircular<E,HashFct,A,MaDis,MiSt>::t;
    using BaseCircular<E,HashFct,A,MaDis,MiSt>::_bitmask;
    using BaseCircular<E,HashFct,A,MaDis,MiSt>::h;
    using BaseCircular<E,HashFct,A,MaDis,MiSt>::_capacity;
    using BaseCircular<E,HashFct,A,MaDis,MiSt>::hash;
    using BaseCircular<E,HashFct,A,MaDis,MiSt>::_version;
    using BaseCircular<E,HashFct,A,MaDis,MiSt>::_right_shift;

    template<class, bool>
    friend class SeqIterator;

public:

    SeqCircular(size_t size_ )
        : BaseCircular<E,HashFct,A,MaDis,MiSt>::BaseCircular(size_),
          n_elem(0), thresh(_capacity*max_fill_factor) {}

    SeqCircular(size_t size_, size_t _version )
        : BaseCircular<E,HashFct,A,MaDis,MiSt>::BaseCircular(size_, _version),
          n_elem(0), thresh(_capacity*max_fill_factor) {}

    // These are used for our tests, such that SeqCircular behaves like GrowTable
    using Handle = SeqCircular<E,HashFct,A,MaDis,MiSt>&;
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

    template<class F, class ... Types>
    insert_return_type update(const key_type& k, F f, Types&& ... args);
    template<class F, class ... Types>
    insert_return_type insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args);

private:
    iterator make_it(value_intern* p, const key_type& k)
    { return iterator(p,k,_version,*this); }
    iterator make_cit(const value_intern* p, const key_type& k) const
    { return const_iterator(p,k,_version,*this); }

    double max_fill_factor = 0.6;
    size_t n_elem;
    size_t thresh;

    inline bool inc_n()
    {
        n_elem += 1;
        if (n_elem > thresh)
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
        std::swap(_capacity, o._capacity);
        std::swap(_version, o._version);
        std::swap(_bitmask, o._bitmask);
        std::swap(thresh, o.thresh);
        std::swap(t, o.t);
        std::swap(hash, o.hash);
        std::swap(_right_shift, o._right_shift);
    }

    inline size_t migrate( SeqCircular& target )
    {
        std::fill( target.t ,target.t + target._capacity , E::getEmpty() );

        auto count = 0u;

        for (size_t i = 0; i < _capacity; ++i)
        {
            auto curr = t[i];
            if ( ! curr.isEmpty() )
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

template<class E, class HF, class A, size_t MD, size_t MS>
inline typename SeqCircular<E,HF,A,MD,MS>::iterator
SeqCircular<E,HF,A,MD,MS>::begin()
{
    for (size_t i = 0; i < _capacity; ++i)
    {
        auto curr = t[i];
        if (! curr.isEmpty()) return make_it(&t[i], curr.getKey());
    }
    return end();
}

template<class E, class HF, class A, size_t MD, size_t MS>
inline typename SeqCircular<E,HF,A,MD,MS>::iterator
SeqCircular<E,HF,A,MD,MS>::end()
{
    return make_it(nullptr, key_type());
}

template<class E, class HF, class A, size_t MD, size_t MS>
inline typename SeqCircular<E,HF,A,MD,MS>::const_iterator
SeqCircular<E,HF,A,MD,MS>::cbegin() const
{
    for (size_t i = 0; i < _capacity; ++i)
    {
        auto curr = t[i];
        if (! curr.isEmpty()) return make_cit(&t[i], curr.getKey());
    }
    return cend();
}

template<class E, class HF, class A, size_t MD, size_t MS>
inline typename SeqCircular<E,HF,A,MD,MS>::const_iterator
SeqCircular<E,HF,A,MD,MS>::cend() const
{
    return make_cit(nullptr, key_type());
}

template<class E, class HF, class A, size_t MD, size_t MS>
inline typename SeqCircular<E,HF,A,MD,MS>::iterator
SeqCircular<E,HF,A,MD,MS>::find(const key_type & k)
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)  // i < htemp+MaDis
    {
        E curr(t[i & _bitmask]);
        if (curr.compareKey(k)) return make_it(&t[i&_bitmask], k);
        else if (curr.isEmpty()) return end();
    }
}

template<class E, class HF, class A, size_t MD, size_t MS>
inline typename SeqCircular<E,HF,A,MD,MS>::const_iterator
SeqCircular<E,HF,A,MD,MS>::find(const key_type & k) const
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)
    {
        E curr(t[i & _bitmask]);
        if (curr.compareKey(k)) return make_cit(&t[i&_bitmask], k);
        else if (curr.isEmpty()) return cend();
    }
}

template<class E, class HF, class A, size_t MD, size_t MS>
inline typename SeqCircular<E,HF,A,MD,MS>::insert_return_type
SeqCircular<E,HF,A,MD,MS>::insert(const key_type& k, const mapped_type& d)
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)
    {
        const size_t temp = i & _bitmask;
        E curr(t[temp]);
        if (curr.compareKey(k)) return insert_return_type(make_it(&t[temp], k), false); // already hashed
        else if (curr.isEmpty())
        {
            if (inc_n()) { n_elem--; return insert(k,d); }
            t[temp] = E(k,d);
            return insert_return_type(make_it(&t[temp], k), true);
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
    // grow();
    // return insert(e);
}

template<class E, class HF, class A, size_t MD, size_t MS>
template<class F, class ...Types>
inline typename SeqCircular<E,HF,A,MD,MS>::insert_return_type
SeqCircular<E,HF,A,MD,MS>::update(const key_type& k, F f, Types&& ... args)
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)
    {
        const size_t temp = i & _bitmask;
        E curr(t[temp]);
        if (curr.compareKey(k))
        {
            t[temp].nonAtomicUpdate(f, std::forward<Types>(args)...);
            // return ReturnCode::SUCCESS_UP;
            return insert_return_type(make_it(&t[temp], k), true);
        }
        else if (curr.isEmpty())
        {
            return insert_return_type(end(), false);
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
}

template<class E, class HF, class A, size_t MD, size_t MS>
template<class F, class ...Types>
inline typename SeqCircular<E,HF,A,MD,MS>::insert_return_type
SeqCircular<E,HF,A,MD,MS>::insertOrUpdate(const key_type& k, const mapped_type& d, F f, Types&& ... args)
{
    size_t htemp = h(k);
    for (size_t i = htemp;;++i)
    {
        const size_t temp = i & _bitmask;
        E curr(t[temp]);
        if (curr.compareKey(k))
        {
            t[temp].nonAtomicUpdate(f, std::forward<Types>(args) ...);
            return insert_return_type(make_it(&t[temp], k), false);
        }
        else if (curr.isEmpty())
        {
            if (inc_n()) { n_elem--; return insert(k,d); }
            t[temp] = E(k,d);
            return insert_return_type(make_it(&t[temp], k), true);
        }
        else if (curr.isDeleted())
        {
            //do something appropriate
        }
    }
}


template<class E, class HF, class A, size_t MD, size_t MS>
inline typename SeqCircular<E,HF,A,MD,MS>::size_type
SeqCircular<E,HF,A,MD,MS>::erase(const key_type & k)
{
    size_type i = h(k);
    for (;;++i)
    {
        E curr(t[i & _bitmask]);
        if (curr.compareKey(k)) break;
        else if (curr.isEmpty()) return 0;
    }
    i &= _bitmask;
    t[i] = value_intern::getEmpty();
    for (size_type j = i+1;; ++j)
    {
        E curr(t[j & _bitmask]);
        if (curr.isEmpty())
            return 1;
        else if (h(curr.getKey()) <= i)
        {
            t[i] = curr;
            t[j&_bitmask] = value_intern::getEmpty();
            i = j & _bitmask;
            if (j > _bitmask) j &= _bitmask;
        }
    }
}


}

#endif // SEQCIRCULAR
