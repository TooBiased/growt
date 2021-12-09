q /*******************************************************************************
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

#ifndef TSXDEFINITIONS_H
#define TSXDEFINITIONS_H

#include "data-structures/seqcircular.hpp"

#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/growtable.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/tsxcircular.hpp"

    namespace growt
{

    template <class HashFct   = std::hash<typename simple_element::Key>,
              class Allocator = std::allocator<char> >
    using folklore = TSXCircular<simple_element, HashFct, Allocator>;


    template <class E, class HashFct = std::hash<E>,
              class Allocator = std::allocator<E> >
    using NoGrow = TSXCircular<E, HashFct, Allocator>;

    template <class HashFct   = std::hash<typename markable_element::Key>,
              class Allocator = std::allocator<char> >
    using uaxGrow = grow_table<NoGrow<markable_element, HashFct, Allocator>,
                               WStratUser, EStratAsync>;

    template <class HashFct   = std::hash<typename simple_element::Key>,
              class Allocator = std::allocator<char> >
    using usxGrow = grow_table<NoGrow<markable_element, HashFct, Allocator>,
                               WStratUser, EStratSync>;

    template <class HashFct   = std::hash<typename simple_element::Key>,
              class Allocator = std::allocator<char> >
    using usnxGrow = grow_table<NoGrow<markable_element, HashFct, Allocator>,
                                WStratUser, EStratSyncNUMA>;


    template <class HashFct   = std::hash<typename markable_element::Key>,
              class Allocator = std::allocator<char> >
    using paxGrow = grow_table<NoGrow<markable_element, HashFct, Allocator>,
                               WStratPool, EStratAsync>;

    template <class HashFct   = std::hash<typename simple_element::Key>,
              class Allocator = std::allocator<char> >
    using psxGrow = grow_table<NoGrow<markable_element, HashFct, Allocator>,
                               WStratPool, EStratSync>;

    template <class HashFct   = std::hash<typename simple_element::Key>,
              class Allocator = std::allocator<char> >
    using psnxGrow = grow_table<NoGrow<markable_element, HashFct, Allocator>,
                                WStratPool, EStratSyncNUMA>;
}

#endif // DEFINITIONS_H
