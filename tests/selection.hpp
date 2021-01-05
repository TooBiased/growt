
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

template <class Key, class Data, class HashFct, class Alloc,
          hmod ... Mods>
using table_config = typename growt::table_config<Key,
                                                  Data,
                                                  HashFct,
                                                  Alloc,
                                                  dynamic,
                                                  estrat,
                                                  wstrat,
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
#include "wrapper/junction_wrapper.hpp"
#define CONFIG junction_config
#endif

#if defined(JUNCTION_LEAPFROG)
#define JUNCTION_TYPE junction::ConcurrentMap_Leapfrog
#include "wrapper/junction_wrapper.hpp"
#define CONFIG junction_config
#endif

#if defined(JUNCTION_GRAMPA)
#define JUNCTION_TYPE junction::ConcurrentMap_Grampa
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


// !!! ADAPTER FOR LEGACY IMPLEMENTATIONS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#if defined(FOLKLOREOLD)  ||                    \
    defined(UAGROWOLD)    ||                    \
    defined(USGROWOLD)    ||                    \
    defined(PAGROWOLD)    ||                    \
    defined(PSGROWOLD)    ||                    \
    defined(USNGROWOLD)   ||                    \
    defined(PSNGROWOLD)   ||                    \
    defined(XFOLKLOREOLD) ||                    \
    defined(UAXGROWOLD)   ||                    \
    defined(USXGROWOLD)   ||                    \
    defined(PAXGROWOLD)   ||                    \
    defined(PSXGROWOLD)   ||                    \
    defined(USNXGROWOLD)  ||                    \
    defined(PSNXGROWOLD)
#define OLD_ADAPTER
#endif

#ifdef OLD_ADAPTER
template <template<class, class> class OldType, hmod ... PMods>
class old_config_factory
{
public:
    template <class Key, class Data, class HashFct, class Alloc,
              hmod ... Mods>
    class table_config
    {
    public:
        using table_type = OldType<HashFct, Alloc>;

    private:
        static constexpr bool is_viable = std::is_same<Key , size_t>::value
            && std::is_same<Data, size_t>::value;
        static_assert(is_viable,
                      "legacy table does not support data types");

        using pmods = mod_aggregator<PMods...>;
        static_assert(pmods::template all<Mods... >(),
                      "legacy table does not support all hmods");
    };
};
#endif



// !!! LEGACY IMPLEMENTATIONS !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#ifdef FOLKLOREOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
template <class HashFct, class Alloc>
using old_table = growt::base_circular<growt::simple_element, HashFct, Alloc>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // FOLKLOREOLD

#ifdef UAGROWOLD
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::base_circular<growt::markable_element,
                                                         HashFct,
                                                         Alloc>,
                                    growt::WStratUser, growt::EStratAsync>;

template<class Key, class Data, class HashFct, class Alloc, hmod ... Mods>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<Key, Data, HashFct, Alloc, Mods...>;
#endif // UAGROWOLD

#ifdef USGROWOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::base_circular<growt::markable_element,
                                                         HashFct,
                                                         Alloc>,
                                    growt::WStratUser, growt::EStratSync>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::sync,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // USGROWOLD

#ifdef PAGROWOLD
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::base_circular<growt::markable_element,
                                                         HashFct,
                                                         Alloc>,
                                    growt::WStratPool, growt::EStratAsync>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::pool,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // PAGROWOLD

#ifdef PSGROWOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::base_circular<growt::simple_element,
                                                         HashFct,
                                                         Alloc>,
                                    growt::WStratPool, growt::EStratSync>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::sync,
                                        hmod::pool,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // PSGROWOLD

#ifdef USNGROWOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::base_circular<growt::markable_element,
                                                         HashFct,
                                                         Alloc>,
                                    growt::WStratUser, growt::EStratSyncNUMA>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::sync,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // USNGROWOLD

#ifdef PSNGROWOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/base_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::base_circular<growt::simple_element,
                                                         HashFct,
                                                         Alloc>,
                                    growt::WStratPool, growt::EStratSyncNUMA>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::sync,
                                        hmod::pool,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // PSNGROWOLD



// !!! LEGACY TSX VARIANT !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#ifdef XFOLKLOREOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
template <class HashFct, class Alloc>
using old_table = growt::TSXCircular<growt::simple_element, HashFct, Alloc>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // XFOLKLORE

#ifdef UAXGROWOLD
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::TSXCircular<growt::markable_element,
                                                       HashFct,
                                                       Alloc>,
                                    growt::WStratUser, growt::EStratAsync>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // UAXGROWOLD

#ifdef USXGROWOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::TSXCircular<growt::markable_element,
                                                       HashFct,
                                                       Alloc>,
                                    growt::WStratUser, growt::EStratSync>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::sync,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // USXGROWOLD

#ifdef PAXGROWOLD
#include "data-structures/element_types/markable_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_async.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::TSXCircular<growt::markable_element,
                                                       HashFct,
                                                       Alloc>,
                                    growt::WStratPool, growt::EStratAsync>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::pool,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // PAXGROWOLD

#ifdef PSXGROWOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_sync.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::TSXCircular<growt::simple_element,
                                                       HashFct,
                                                       Alloc>,
                                    growt::WStratPool, growt::EStratSync>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::sync,
                                        hmod::pool,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // PSXGROWOLD

#ifdef USNXGROWOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_user.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::TSXCircular<growt::markable_element,
                                                       HashFct,
                                                       Alloc>,
                                    growt::WStratUser, growt::EStratSyncNUMA>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::sync,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // USNXGROWOLD

#ifdef PSNXGROWOLD
#include "data-structures/element_types/simple_element.hpp"
#include "data-structures/tsx_circular.hpp"
#include "data-structures/strategy/wstrat_pool.hpp"
#include "data-structures/strategy/estrat_sync_alt.hpp"
#include "data-structures/grow_table.hpp"
//#include "data-structures/ancient_grow.hpp"
template <class HashFct, class Alloc>
using old_table = growt::grow_table<growt::TSXCircular<growt::simple_element,
                                                       HashFct,
                                                       Alloc>,
                                    growt::WStratPool, growt::EStratSyncNUMA>;

template<class K, class D, class HF, class AL, hmod ... M>
using table_config = old_config_factory<old_table,
                                        hmod::growable,
                                        hmod::deletion,
                                        hmod::sync,
                                        hmod::pool,
                                        hmod::circular_map,
                                        hmod::circular_prob
                                        >::template table_config<K,D,HF,AL,M...>;
#endif // PSNXGROWOLD
