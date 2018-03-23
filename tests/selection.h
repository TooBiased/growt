
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
#include "utils/alignedallocator.h"
#endif

#ifdef POOL
#define ALLOCATOR growt::PoolAllocator
#include "utils/poolallocator.h"
#endif

#ifdef NUMA_POOL
#define ALLOCATOR growt::NUMAPoolAllocator
#include "utils/numapoolallocator.h"
#endif

#ifdef HTLB_POOL
#define ALLOCATOR growt::HTLBPoolAllocator
#include "utils/poolallocator.h"
#endif




#ifdef SEQUENTIAL
#include "data-structures/simpleelement.h"
#include "data-structures/seqcircular.h"
#define HASHTYPE growt::SeqCircular<growt::SimpleElement, HASHFCT, \
                                    ALLOCATOR<> >
#endif // SEQUENTIAL




#ifdef FOLKLORE
#include "data-structures/simpleelement.h"
#include "data-structures/base_circular.h"
#define HASHTYPE growt::BaseCircular<growt::SimpleElement, HASHFCT, \
                                 ALLOCATOR<> >
#endif // FOLKLORE

#ifdef XFOLKLORE
#include "data-structures/simpleelement.h"
#include "data-structures/tsx_circular.h"
#define HASHTYPE growt::TSXCircular<growt::SimpleElement, HASHFCT, \
                                    ALLOCATOR<> >
#endif // XFOLKLORE




#ifdef UAGROW
#include "data-structures/markableelement.h"
#include "data-structures/base_circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/estrat_async.h"
//#include "data-structures/grow_table.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::BaseCircular<growt::MarkableElement, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratAsync>

#endif // UAGROW

#ifdef PAGROW
#include "data-structures/markableelement.h"
#include "data-structures/base_circular.h"
#include "data-structures/strategy/wstrat_pool.h"
#include "data-structures/strategy/estrat_async.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::BaseCircular<growt::MarkableElement, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratAsync>
#endif // PAGROW

#ifdef USGROW
#include "data-structures/simpleelement.h"
#include "data-structures/base_circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/estrat_sync.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::BaseCircular<growt::SimpleElement, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratSync>
#endif // USGROW

#ifdef PSGROW
#include "data-structures/simpleelement.h"
#include "data-structures/base_circular.h"
#include "data-structures/strategy/wstrat_pool.h"
#include "data-structures/strategy/estrat_sync.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::BaseCircular<growt::SimpleElement, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratSync>
#endif // PSGROW

#ifdef USNGROW
#include "data-structures/simpleelement.h"
#include "data-structures/base_circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/estrat_sync_alt.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::BaseCircular<growt::SimpleElement, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratSyncNUMA>
#endif // USNGROW

#ifdef PSNGROW
#include "data-structures/simpleelement.h"
#include "data-structures/base_circular.h"
#include "data-structures/strategy/wstrat_pool.h"
#include "data-structures/strategy/estrat_sync_alt.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::BaseCircular<growt::SimpleElement, \
                                                  HASHFCT, \
                                                  ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratSyncNUMA>
#endif // PSNGROW




#ifdef UAXGROW
#include "data-structures/markableelement.h"
#include "data-structures/tsx_circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/estrat_async.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::TSXCircular<growt::MarkableElement, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratAsync>
#endif // UAXGROW

#ifdef PAXGROW
#include "data-structures/markableelement.h"
#include "data-structures/tsx_circular.h"
#include "data-structures/strategy/wstrat_pool.h"
#include "data-structures/strategy/estrat_async.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::TSXCircular<growt::MarkableElement, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratAsync>
#endif // PAXGROW

#ifdef USXGROW
#include "data-structures/simpleelement.h"
#include "data-structures/tsx_circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/estrat_sync.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::TSXCircular<growt::SimpleElement, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratSync>
#endif // USXGROW

#ifdef PSXGROW
#include "data-structures/simpleelement.h"
#include "data-structures/tsx_circular.h"
#include "data-structures/strategy/wstrat_pool.h"
#include "data-structures/strategy/estrat_sync.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::TSXCircular<growt::SimpleElement, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratSync>
#endif // PSXGROW

#ifdef USNXGROW
#include "data-structures/simpleelement.h"
#include "data-structures/tsx_circular.h"
#include "data-structures/strategy/wstrat_user.h"
#include "data-structures/strategy/estrat_sync_alt.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::TSXCircular<growt::SimpleElement, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratUser, growt::EStratSyncNUMA>
#endif // USNXGROW

#ifdef PSNXGROW
#include "data-structures/simpleelement.h"
#include "data-structures/tsx_circular.h"
#include "data-structures/strategy/wstrat_pool.h"
#include "data-structures/strategy/estrat_sync_alt.h"
#include "data-structures/ancient_grow.h"
#define HASHTYPE growt::GrowTable<growt::TSXCircular<growt::SimpleElement, \
                                                     HASHFCT, \
                                                     ALLOCATOR<> >, \
                                  growt::WStratPool, growt::EStratSyncNUMA>
#endif // PSNXGROW



#ifdef FOLLY
#include "wrapper/folly_wrapper.h"
#define HASHTYPE FollyWrapper<HASHFCT>
#endif // FOLLY

#ifdef CUCKOO
#include "wrapper/cuckoo_wrapper.h"
#define HASHTYPE CuckooWrapper<HASHFCT>
#endif // CUCKOO

#ifdef TBBHM
#include "wrapper/tbb_hm_wrapper.h"
#define HASHTYPE TBBHMWrapper<HASHFCT>
#endif //TBBHM

#ifdef TBBUM
#include "wrapper/tbb_um_wrapper.h"
#define HASHTYPE TBBUMWrapper<HASHFCT>
#endif //TBBUM

#ifdef JUNCTION_LINEAR
#define JUNCTION_TYPE junction::ConcurrentMap_Linear
#include "wrapper/junction_wrapper.h"
#define HASHTYPE JunctionWrapper
//<junction::ConcurrentMap_Linear<turf::u64, turf::u64> >
#endif //JUNCTION_LINEAR

#ifdef JUNCTION_GRAMPA
#define JUNCTION_TYPE junction::ConcurrentMap_Grampa
#include "wrapper/junction_wrapper.h"
#define HASHTYPE JunctionWrapper
//<junction::ConcurrentMap_Linear<turf::u64, turf::u64> >
#endif //JUNCTION_GRAMPA

#ifdef JUNCTION_LEAPFROG
#define JUNCTION_TYPE junction::ConcurrentMap_Leapfrog
#include "wrapper/junction_wrapper.h"
#define HASHTYPE JunctionWrapper
//<junction::ConcurrentMap_Linear<turf::u64, turf::u64> >
#endif //JUNCTION_LEAPFROG


#endif // SELECTION
