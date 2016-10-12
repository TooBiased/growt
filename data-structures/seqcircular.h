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

#include "data-structures/circular.h"

namespace growt {

template<class E, class HashFct = std::hash<typename E::Key>,
         class A = std::allocator<E>,
         size_t MaDis = 128, size_t MiSt = 200>
class SeqCircular: public Circular<E, HashFct, A, MaDis,MiSt>
{
public:
    using Key  = typename Circular<E,HashFct,A,MaDis,MiSt>::Key;
    using Data = typename Circular<E,HashFct,A,MaDis,MiSt>::Data;
    using Circular<E,HashFct,A,MaDis,MiSt>::t;
    using Circular<E,HashFct,A,MaDis,MiSt>::bitmask;
    using Circular<E,HashFct,A,MaDis,MiSt>::h;
    using Circular<E,HashFct,A,MaDis,MiSt>::size;
    using Circular<E,HashFct,A,MaDis,MiSt>::hash;
    using Circular<E,HashFct,A,MaDis,MiSt>::version;
    using Circular<E,HashFct,A,MaDis,MiSt>::right_shift;

    // These are used for our tests, such that SeqCircular behaves like GrowTable
    using Handle = SeqCircular<E,HashFct,A,MaDis,MiSt>&;
    Handle getHandle() { return *this; }

    inline void inc_n()
    {
        ++n_elem;
        if (n_elem > size*max_fill_factor)
        {
            grow();
        }
    }

    inline void grow()
    {
        SeqCircular temp(size << 1, version+1);
        migrate(temp);
        swap(temp);
    }

    double max_fill_factor = 0.6;
    size_t   n_elem;

    SeqCircular(size_t size_ )
        : Circular<E,HashFct,A,MaDis,MiSt>::Circular(size_),
          n_elem(0) {}

    SeqCircular(size_t size_, size_t version )
        : Circular<E,HashFct,A,MaDis,MiSt>::Circular(size_, version),
          n_elem(0) {}

    inline ReturnElement find(const Key & k)
    {
        size_t htemp = h(k);
        for (size_t i = htemp; i < htemp+MaDis; ++i)
        {
            E curr(t[i & bitmask]);
            if (curr.compareKey(k)) return curr;
            else if (curr.isEmpty()) return ReturnElement::getEmpty();
        }
        return ReturnElement::getEmpty();
    }

    inline ReturnCode insert(const Key k, const Data d)
    {   return insert(E(k,d));  }

    inline ReturnCode insert(const E & e)
    {
        const Key k = e.getKey();

        size_t htemp = h(k);
        for (size_t i = htemp; i < htemp+MaDis; ++i)
        {
            const size_t temp = i & bitmask;
            E curr(t[temp]);
            if (curr.compareKey(k)) return ReturnCode::UNSUCCESS_ALREADY_USED; // already hashed
            else if (curr.isEmpty())
            {
                t[temp] = e;
                inc_n();
                return ReturnCode::SUCCESS_IN;
            }
            else if (curr.isDeleted())
            {
               //do something appropriate
            }
        }
        grow();
        return insert(e);
   }

    template<class F>
    inline ReturnCode update(const Key k, const Data d, F f)
    {   return update(E(k,d), f);  }

    template<class F>
    inline ReturnCode update(const E & e, F f)
    {
        const Key k = e.getKey();

        size_t htemp = h(k);
        for (size_t i = htemp; i < htemp+MaDis; ++i)
        {
            const size_t temp = i & bitmask;
            E curr(t[temp]);
            if (curr.compareKey(k))
            {
                t[temp].nonAtomicUpdate(curr, e, f);
                return ReturnCode::SUCCESS_UP;
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
        return ReturnCode::UNSUCCESS_NOT_FOUND; // also full, but this is not important
    }

    template<class F>
    inline ReturnCode insertOrUpdate(const Key k, const Data d, F f)
    {   return insertOrUpdate(E(k,d), f);  }

    template<class F>
    inline ReturnCode insertOrUpdate(const E & e, F f)
    {
        const Key k = e.getKey();

        size_t htemp = h(k);
        for (size_t i = htemp; i < htemp+MaDis; ++i)
        {
            const size_t temp = i & bitmask;
            E curr(t[temp]);
            if (curr.compareKey(k))
            {
                t[temp].nonAtomicUpdate(curr, e, f);
                return ReturnCode::SUCCESS_UP;
            }
            else if (curr.isEmpty())
            {
                t[temp] = e;
                inc_n();
                return ReturnCode::SUCCESS_IN;
            }
            else if (curr.isDeleted())
            {
               //do something appropriate
            }
        }
        grow();
        return insertOrUpdate(e,f);
    }

   void swap(SeqCircular & o)
    {
        std::swap(size, o.size);
        std::swap(version, o.version);
        std::swap(bitmask, o.bitmask);
        std::swap(t, o.t);
        std::swap(hash, o.hash);
        std::swap(right_shift, o.right_shift);
    }

    inline size_t migrate( SeqCircular& target )
    {
        std::fill( target.t ,target.t + target.size , E::getEmpty() );

        auto count = 0u;

        for (size_t i = 0; i < size; ++i)
        {
            auto curr = t[i];
            if ( ! curr.isEmpty() )
            {
                count++;
                //target.insert( curr );
                if (!successful(target.insert(curr)))
                {
                    std::logic_error("Unsuccessful insert during sequential migration!");
                }
            }
        }
        return count;

    }
};

}

#endif // SEQCIRCULAR
