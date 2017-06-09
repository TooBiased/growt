/*******************************************************************************
 * data-structures/definitions.h
 *
 * Convenience file that defines some often used hash table variants.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include "data-structures/seqcircular.h"

#include "data-structures/simpleelement.h"
#include "data-structures/markableelement.h"
#include "data-structures/base_circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/wstrat_pool.h"
#include "data-structures/strategy/estrat_async.h"
#include "data-structures/strategy/estrat_sync.h"
#include "data-structures/strategy/estrat_sync_alt.h"
#include "data-structures/grow_table.h"

namespace growt {

template<class HashFct = std::hash<size_t>, class Allocator = std::allocator<char> >
using SequentialTable = SeqCircular<SimpleElement, HashFct, Allocator>;


template<class HashFct = std::hash<typename SimpleElement::key_type>, class Allocator = std::allocator<char> >
using folklore    = BaseCircular<SimpleElement, HashFct, Allocator>;


template<class E, class HashFct = std::hash<E>, class Allocator = std::allocator<E> >
using NoGrow      = BaseCircular<E, HashFct, Allocator>;

template<class HashFct    = std::hash<typename MarkableElement::key_type>,
         class Allocator  = std::allocator<char> >
using uaGrow  = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratUser, EStratAsync>;

template<class HashFct    = std::hash<typename SimpleElement::key_type>,
         class Allocator  = std::allocator<char> >
using usGrow  = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratUser, EStratSync>;

template<class HashFct    = std::hash<typename SimpleElement::key_type>,
         class Allocator  = std::allocator<char> >
using usnGrow = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratUser, EStratSyncNUMA>;


template<class HashFct    = std::hash<typename MarkableElement::key_type>,
         class Allocator  = std::allocator<char> >
using paGrow  = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratPool, EStratAsync>;

template<class HashFct    = std::hash<typename SimpleElement::key_type>,
         class Allocator  = std::allocator<char> >
using psGrow  = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratPool, EStratSync>;

template<class HashFct   = std::hash<typename SimpleElement::key_type>,
         class Allocator = std::allocator<char> >
using psnGrow = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratPool, EStratSyncNUMA>;

}

#endif // DEFINITIONS_H
