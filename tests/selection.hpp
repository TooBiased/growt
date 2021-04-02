
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


#ifdef ALIGNED
#include "allocator/alignedallocator.hpp"
using allocator_type = growt::AlignedAllocator<>;
#endif

#ifdef POOL
#include "allocator/poolallocator.hpp"
using allocator_type = growt::PoolAllocator<>;
#endif

#ifdef NUMA_POOL
#include "allocator/numapoolallocator.hpp"
using allocator_type = growt::NUMAPoolAllocator<>;
#endif

#ifdef HTLB_POOL
#include "allocator/poolallocator.hpp"
using allocator_type = growt::HTLBPoolAllocator<>;
#endif

#ifdef TBB_ALIGNED
#include "tbb/scalable_allocator.h"
using allocator_type = tbb::scalable_allocator<void>;
#endif








// !!! OUR IMPLEMENTATIONS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#ifdef SEQUENTIAL
#include "data-structures/seq_table_config.hpp"
template <class Key, class Data, class HashFct, class Alloc,
          hmod ... Mods>
using table_config = growt::seq_table_config<Key, Data, HashFct, Alloc, Mods... >;
#endif // SEQUENTIAL



#if defined(FOLKLORE) ||                        \
    defined(UAGROW)   ||                        \
    defined(USGROW)   ||                        \
    defined(PAGROW)   ||                        \
    defined(PSGROW)
#include "data-structures/table_config.hpp"
#if defined(FOLKLORE)
constexpr hmod dynamic = hmod::neutral;
#else
constexpr hmod dynamic = hmod::growable;
#endif
#if defined(PAGROW) || defined(PSGROW)
constexpr hmod wstrat = hmod::pool;
#else // UXGROW
constexpr hmod wstrat = hmod::neutral;
#endif
#if defined(USGROW) || defined(PSGROW)
constexpr hmod estrat = hmod::sync;
#else // XSGROW
constexpr hmod estrat = hmod::neutral;
#endif

#if defined(CMAP)
constexpr hmod cmap = hmod::circular_map;
#else
constexpr hmod cmap = hmod::neutral;
#endif

#if defined(CPROB)
constexpr hmod cprob = hmod::circular_prob;
#else
constexpr hmod cprob = hmod::neutral;
#endif

template <class Key, class Data, class HashFct, class Alloc,
          hmod ... Mods>
using table_config = typename growt::table_config<Key,
                                                  Data,
                                                  HashFct,
                                                  Alloc,
                                                  dynamic,
                                                  estrat,
                                                  wstrat,
                                                  cmap,
                                                  cprob,
                                                  Mods ...>;
#endif



// !!! THIRD PARTY IMPLEMENTATIONS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#if defined(FOLLY)             ||               \
    defined(CUCKOO)            ||               \
    defined(TBBHM)             ||               \
    defined(TBBUM)             ||               \
    defined(JUNCTION_LINEAR)   ||               \
    defined(JUNCTION_LEAPFROG) ||               \
    defined(JUNCTION_GRAMPA)

#if defined(FOLLY)
#include "wrapper/folly_wrapper.hpp"
#define CONFIG folly_config
#endif

#if defined(CUCKOO)
#include "wrapper/cuckoo_wrapper.hpp"
#define CONFIG cuckoo_config
#endif

#if defined(TBBHM)
#include "wrapper/tbb_hm_wrapper.hpp"
#define CONFIG tbb_hm_config
#endif

#if defined(TBBUM)
#include "wrapper/tbb_um_wrapper.hpp"
#define CONFIG tbb_um_config
#endif

#if defined(JUNCTION_LINEAR)
#define JUNCTION_TYPE junction::ConcurrentMap_Linear
#define JUNCTION_NAME "junction_linear"
#include "wrapper/junction_wrapper.hpp"
#define CONFIG junction_config
#endif

#if defined(JUNCTION_LEAPFROG)
#define JUNCTION_TYPE junction::ConcurrentMap_Leapfrog
#define JUNCTION_NAME "junction_leapfrog"
#include "wrapper/junction_wrapper.hpp"
#define CONFIG junction_config
#endif

#if defined(JUNCTION_GRAMPA)
#define JUNCTION_TYPE junction::ConcurrentMap_Grampa
#define JUNCTION_NAME "junction_grampa"
#include "wrapper/junction_wrapper.hpp"
#define CONFIG junction_config
#endif

template <class Key, class Data, class HashFct, class Alloc,
          hmod ... Mods>
using table_config = CONFIG<Key,
                            Data,
                            HashFct,
                            Alloc,
                            Mods ...>;
#endif


#if defined(HOPSCOTCH)         ||               \
    defined(LEAHASH)           ||               \
    defined(SHUNHASH)          ||               \
    defined(RCU_BASE)

#if defined(HOPSCOTCH)
#include "legacy_wrapper/hopscotch_wrapper.h"
#define CONFIG hopscotch_config
#endif

#if defined(LEAHASH)
#include "legacy_wrapper/leahash_wrapper.h"
#define CONFIG leahash_config
#endif

#if defined(SHUNHASH)
#include "legacy_wrapper/shun_wrapper.h"
#define CONFIG shun_config
#endif

#if defined(RCU_BASE)
#include "legacy_wrapper/rcubase_wrapper.h"
#define CONFIG rcu_config
#endif


template <class Key, class Data, class HashFct, class Alloc,
          hmod ... Mods>
using table_config = CONFIG<Key,
                            Data,
                            HashFct,
                            Alloc,
                            Mods ...>;
#endif
