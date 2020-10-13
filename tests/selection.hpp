
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

#ifndef SELECTION_H
#define SELECTION_H

// THIS HEADER USES DEFINES TO SELECT THE CORRECT
// HASHTABLE AT COMPILE TIME


#ifdef ALIGNED
#define ALLOCATOR growt::AlignedAllocator
#include "allocator/alignedallocator.hpp"
#endif

#ifdef POOL
#define ALLOCATOR growt::PoolAllocator
#include "allocator/poolallocator.hpp"
#endif

#ifdef NUMA_POOL
#define ALLOCATOR growt::NUMAPoolAllocator
#include "allocator/numapoolallocator.hpp"
#endif

#ifdef HTLB_POOL
#define ALLOCATOR growt::HTLBPoolAllocator
#include "allocator/poolallocator.hpp"
#endif




#ifdef SEQUENTIAL
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/seqcircular.hpp"
#define HASHTYPE growt::seq_circular<growt::simple_element, HASHFCT, \
                                    ALLOCATOR<> >
#endif // SEQUENTIAL




#ifdef FOLKLORE
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#define HASHTYPE growt::base_circular<growt::simple_element, HASHFCT, \
                                 ALLOCATOR<> >
#endif // FOLKLORE

#ifdef XFOLKLORE
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#define HASHTYPE growt::TSXCircular<growt::simple_element, HASHFCT, \
                                    ALLOCATOR<> >
#endif // XFOLKLORE




#ifdef UAGROW
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::base_circular<growt::markable_element, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratAsync>

#endif // UAGROW

#ifdef PAGROW
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::base_circular<growt::markable_element, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratAsync>
#endif // PAGROW

#ifdef USGROW
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::base_circular<growt::simple_element, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratSync>
#endif // USGROW

#ifdef PSGROW
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::base_circular<growt::simple_element, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratSync>
#endif // PSGROW

#ifdef USNGROW
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::base_circular<growt::simple_element, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratSyncNUMA>
#endif // USNGROW

#ifdef PSNGROW
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::base_circular<growt::simple_element, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratSyncNUMA>
#endif // PSNGROW




#ifdef UAXGROW
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::TSXCircular<growt::markable_element, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratAsync>
#endif // UAXGROW

#ifdef PAXGROW
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::TSXCircular<growt::markable_element, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratAsync>
#endif // PAXGROW

#ifdef USXGROW
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::TSXCircular<growt::simple_element, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratSync>
#endif // USXGROW

#ifdef PSXGROW
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::TSXCircular<growt::simple_element, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratSync>
#endif // PSXGROW

#ifdef USNXGROW
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::TSXCircular<growt::simple_element, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratSyncNUMA>
#endif // USNXGROW

#ifdef PSNXGROW
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/ancient_grow.hpp"
#define HASHTYPE growt::grow_table<growt::TSXCircular<growt::simple_element, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratSyncNUMA>
#endif // PSNXGROW



#ifdef FOLLY
#include "wrapper/folly_wrapper.hpp"
#define HASHTYPE FollyWrapper<HASHFCT>
#endif // FOLLY

#ifdef CUCKOO
#include "wrapper/cuckoo_wrapper.hpp"
#define HASHTYPE CuckooWrapper<HASHFCT>
#endif // CUCKOO

#ifdef TBBHM
#include "wrapper/tbb_hm_wrapper.hpp"
#define HASHTYPE TBBHMWrapper<HASHFCT>
#endif //TBBHM

#ifdef TBBUM
#include "wrapper/tbb_um_wrapper.hpp"
#define HASHTYPE TBBUMWrapper<HASHFCT>
#endif //TBBUM

#ifdef JUNCTION_LINEAR
#define JUNCTION_TYPE junction::ConcurrentMap_Linear
#include "wrapper/junction_wrapper.hpp"
#define HASHTYPE JunctionWrapper
//<junction::ConcurrentMap_Linear<turf::u64, turf::u64> >
#endif //JUNCTION_LINEAR

#ifdef JUNCTION_GRAMPA
#define JUNCTION_TYPE junction::ConcurrentMap_Grampa
#include "wrapper/junction_wrapper.hpp"
#define HASHTYPE JunctionWrapper
//<junction::ConcurrentMap_Linear<turf::u64, turf::u64> >
#endif //JUNCTION_GRAMPA

#ifdef JUNCTION_LEAPFROG
#define JUNCTION_TYPE junction::ConcurrentMap_Leapfrog
#include "wrapper/junction_wrapper.hpp"
#define HASHTYPE JunctionWrapper
//<junction::ConcurrentMap_Linear<turf::u64, turf::u64> >
#endif //JUNCTION_LEAPFROG

#ifdef UAGROWNEW
#include "data-structures/element_types/simple_slot.hpp"
#include "data-structures/base_linear.hpp"
#include "data-structures/newstrategies/wstrat_user.hpp"
#include "data-structures/newstrategies/estrat_async.hpp"
#include "data-structures/migration_table.hpp"
using HASHTYPE = growt::migration_table<growt::base_linear<
                                       growt::simple_slot<size_t, size_t, true>,
                                       HASHFCT,
                                       ALLOCATOR<>
                                       >
                                   , growt::wstrat_user, growt::estrat_async>;

#endif // UAGROWNEW


#endif // SELECTION
