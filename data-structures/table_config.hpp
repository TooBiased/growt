#pragma once

#include "data-structures/element_types/complex_slot.hpp"
#include "data-structures/element_types/simple_slot.hpp"

#include "data-structures/newstrategies/estrat_async.hpp"
#include "data-structures/newstrategies/wstrat_user.hpp"

#include "data-structures/base_linear.hpp"
#include "data-structures/migration_table.hpp"

namespace growt
{



// namespace hash_mod
// {
//     using growable      = mod_config<true ,false,false,false,false>;
//     using deletion      = mod_config<false,true ,false,false,false>;
//     using ref_integrity = mod_config<false,false,true ,false,false>;
//     using sync          = mod_config<false,false,false,true ,false>;
//     using pool          = mod_config<false,false,false,false,true >;
//     using circular_mapping = mod_config<false,false,false,false,false>;
//     using circular_probing = mod_config<circular_probing>;
//     // using atomic_update      = mod_config<false,false,false,true ,false>;
//     // using non_atomic_updates = mod_config<false,false,false,false,true >;
// }

// template <bool Grow, bool Del, bool RefInteg, bool Sync, bool Pool>
// class mod_config
// {
// public:
//     static constexpr bool needs_growing               = Grow;
//     static constexpr bool needs_deletions             = Del;
//     static constexpr bool needs_referential_integrity = RefInteg;
//     // currently not used
//     // static constexpr bool needs_atomic_updates        = AtomicUp;
//     // static constexpr bool needs_non_atomic_updates    = NonAtomicUp;
//     static constexpr bool needs_no_marking            = Sync;
//     static constexpr bool needs_threadpool            = Pool;
// }

// template <class ... OperationCodes>
// class mod_aggregator
// {
// public:
//     static constexpr bool needs_growing =
//         (OperationCodes::needs_growing || ...);
//     static constexpr bool needs_deletions =
//         (OperationCodes::needs_deletions || ...);
//     static constexpr bool needs_referential_integrity =
//         (OperationCodes::needs_referential_integrity || ...);
//     // currently not used
//     // static constexpr bool needs_atomic_updates =
//     //     (OperationCodes::needs_atomic_updates || ...);
//     // static constexpr bool needs_non_atomic_updates =
//     //     (OperationCodes::needs_non_atomic_updates || ...);
//     static constexpr bool needs_no_marking =
//         needs_growing && (OperationCodes::needs_no_marking || ...);
//     static constexpr bool needs_threadpool =
//         needs_growing && (OperationCodes::needs_threadpool || ...);
// }

enum class hash_mod : size_t
{
growable      = 1,
deletion      = 2,
ref_integrity = 4,
sync          = 8,
pool          = 16,
circular_map  = 32,
circular_prob = 64
};

template <hash_mod ... Mods>
class mod_aggregator
{
    static constexpr size_t mod_descriptor =
        ( static_cast<size_t>(Mods) | ... );

    template <hash_mod Ask>
    static constexpr bool is()
    {
        auto ask = static_cast<size_t>(Ask);
        return (mod_descriptor & ask) == ask;
    }
};


template <class Key, class Data, class HashFct, class Allocator,
          hash_mod ... Mods>
class table_config
{
public:
    // INPUT TYPES
    using key_type       = Key;
    using mapped_type    = Data;
    using hash_fct_type  = HashFct;
    using allocator_type = Allocator;

    using mods = mod_aggregator<Mods ...>;

    // DERIVED TYPES
    using value_type     = std::pair<const key_type, mapped_type>;

    using slot_config    = typename std::conditional<
        (sizeof(value_type)==16
         && sizeof(key_type)==8
         && !(mods::is<hash_mod::ref_integrity>()
              && mods::is<hash_mod::growable>())),
        simple_slot <key_type,
                     mapped_type,
                     mods::is<hash_mod::growable>()
                     && !(mods::is<hash_mod::sync>())>,
        complex_slot<key_type,
                     mapped_type,
                     mods::is<hash_mod::growable>()
                     && !(mods::is<hash_mod::sync>())>>::type;

    using base_table_type = base_linear<
        slot_config,
        hash_fct_type,
        allocator_type,
        pow2_mapper<mods::is<mods::circular_map>(),
                    mods::is<mods::circular_prob>()>>;
    using table_type = typename std::conditional<
        !mods::is<mods::growable>(),
        base_table_type,
        migration_table<base_table_type,
                        typename std::conditional<mods::is<hash_mod::pool>(),
                                                  wstrat_pool,
                                                  wstrat_user>::type,
                        typename std::conditional<mods::is<hash_mod::sync>(),
                                                  estrat_sync,
                                                  estrat_async>::type>>;
};
}
