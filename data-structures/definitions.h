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
#include "data-structures/circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/wstrat_pool.h"
#include "data-structures/strategy/estrat_async.h"
#include "data-structures/strategy/estrat_sync.h"
#include "data-structures/strategy/estrat_sync_alt.h"
#include "data-structures/growtable.h"

namespace growt {

template<class HashFct = std::hash<size_t>, template<typename> class Allocator = std::allocator>
using SequentialTable = SeqCircular<SimpleElement, HashFct, Allocator<SimpleElement> >;


template<class HashFct = std::hash<typename SimpleElement::Key>, template<typename> class Allocator = std::allocator>
using folklore    = Circular<SimpleElement, HashFct, Allocator<SimpleElement> >;


template<class E, class HashFct = std::hash<E>, template<typename> class Allocator = std::allocator>
using NoGrow      = Circular<E, HashFct, Allocator<E> >;

template<class                    HashFct    = std::hash<typename MarkableElement::Key>,
         template<typename> class Allocator  = std::allocator>
using uaGrow  = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratUser, EStratAsync>;

template<class                    HashFct    = std::hash<typename SimpleElement::Key>,
         template<typename> class Allocator  = std::allocator>
using usGrow  = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratUser, EStratSync>;

template<class                    HashFct    = std::hash<typename SimpleElement::Key>,
         template<typename> class Allocator  = std::allocator>
using usnGrow = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratUser, EStratSyncNUMA>;


template<class                    HashFct    = std::hash<typename MarkableElement::Key>,
         template<typename> class Allocator  = std::allocator>
using paGrow  = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratPool, EStratAsync>;

template<class                    HashFct    = std::hash<typename SimpleElement::Key>,
         template<typename> class Allocator  = std::allocator>
using psGrow  = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratPool, EStratSync>;

template<class                    HashFct   = std::hash<typename SimpleElement::Key>,
         template<typename> class Allocator = std::allocator>
using psnGrow = GrowTable<NoGrow<MarkableElement, HashFct, Allocator>, WStratPool, EStratSyncNUMA>;

}

#endif // DEFINITIONS_H
