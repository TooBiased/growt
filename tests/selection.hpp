
/*******************************************************************************
 * tests/selection.h
 *
 * This file uses compile time constants, to include the correct files and
 * set HASHTYPE and ALLOCATOR defines for our tests/benchmarks
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/
#pragma once

// THIS HEADER USES DEFINES TO SELECT THE CORRECT
// HASHTABLE AT COMPILE TIME

#include "data-structures/hash_table_mods.hpp"


#include "allocator/alignedallocator.hpp"
using allocator_type = growt::AlignedAllocator<>;

// !!! OUR IMPLEMENTATIONS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#ifdef SEQUENTIAL
#include "data-structures/seq_table_config.hpp"
template <class Key, class Data, class HashFct, class Alloc, hmod... Mods>
using table_config =
    growt::seq_table_config<Key, Data, HashFct, Alloc, Mods...>;
#endif // SEQUENTIAL



#if defined(FOLKLORE) || defined(UAGROW) || defined(USGROW) || \
    defined(PAGROW) || defined(PSGROW)
#include "data-structures/table_config.hpp"
#if defined(FOLKLORE)
constexpr hmod dynamic = hmod::neutral;
#else
constexpr hmod dynamic = hmod::growable;
#endif
#if defined(PAGROW) || defined(PSGROW)
constexpr hmod wstrat = hmod::pool;
#else // UXGROW
constexpr hmod wstrat  = hmod::neutral;
#endif
#if defined(USGROW) || defined(PSGROW)
constexpr hmod estrat = hmod::sync;
#else // XSGROW
constexpr hmod estrat  = hmod::neutral;
#endif

#if defined(CMAP)
constexpr hmod cmap = hmod::circular_map;
#else
constexpr hmod cmap    = hmod::neutral;
#endif

#if defined(CPROB)
constexpr hmod cprob = hmod::circular_prob;
#else
constexpr hmod cprob   = hmod::neutral;
#endif

template <class Key, class Data, class HashFct, class Alloc, hmod... Mods>
using table_config = typename growt::table_config<Key,
                                                  Data,
                                                  HashFct,
                                                  Alloc,
                                                  dynamic,
                                                  estrat,
                                                  wstrat,
                                                  cmap,
                                                  cprob,
                                                  Mods...>;
#endif
