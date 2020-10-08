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

#include "data-structures/seqcircular.hpp"

#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/grow_table.hpp"

namespace growt {

template<class HashFct = std::hash<size_t>, class Allocator = std::allocator<char> >
using SequentialTable = seq_circular<simple_element, HashFct, Allocator>;


template<class HashFct = std::hash<typename simple_element::key_type>, class Allocator = std::allocator<char> >
using folklore    = base_circular<simple_element, HashFct, Allocator>;


template<class E, class HashFct = std::hash<E>, class Allocator = std::allocator<E> >
using NoGrow      = base_circular<E, HashFct, Allocator>;

template<class HashFct    = std::hash<typename markable_element::key_type>,
         class Allocator  = std::allocator<char> >
using uaGrow  = grow_table<NoGrow<markable_element, HashFct, Allocator>, WStratUser, EStratAsync>;

template<class HashFct    = std::hash<typename simple_element::key_type>,
         class Allocator  = std::allocator<char> >
using usGrow  = grow_table<NoGrow<markable_element, HashFct, Allocator>, WStratUser, EStratSync>;

template<class HashFct    = std::hash<typename simple_element::key_type>,
         class Allocator  = std::allocator<char> >
using usnGrow = grow_table<NoGrow<markable_element, HashFct, Allocator>, WStratUser, EStratSyncNUMA>;


template<class HashFct    = std::hash<typename markable_element::key_type>,
         class Allocator  = std::allocator<char> >
using paGrow  = grow_table<NoGrow<markable_element, HashFct, Allocator>, WStratPool, EStratAsync>;

template<class HashFct    = std::hash<typename simple_element::key_type>,
         class Allocator  = std::allocator<char> >
using psGrow  = grow_table<NoGrow<markable_element, HashFct, Allocator>, WStratPool, EStratSync>;

template<class HashFct   = std::hash<typename simple_element::key_type>,
         class Allocator = std::allocator<char> >
using psnGrow = grow_table<NoGrow<markable_element, HashFct, Allocator>, WStratPool, EStratSyncNUMA>;

}

#endif // DEFINITIONS_H
