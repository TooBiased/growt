#pragma once

#include <type_traits>

#include "data-structures/element_types/complex_slot.hpp"
#include "data-structures/element_types/simple_slot.hpp"

#include "data-structures/newstrategies/estrat_async.hpp"
#include "data-structures/newstrategies/wstrat_user.hpp"

#include "data-structures/hash_table_mods.hpp"
#include "data-structures/base_linear.hpp"
#include "data-structures/migration_table.hpp"

namespace growt
{

template <class Key, class Data, class HashFct, class Allocator,
          hmod ... Mods>
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


    static constexpr bool needs_marking =
        mods::template is<hmod::growable>()
        && !(mods::template is<hmod::sync>());
    static constexpr bool needs_complex_slot =
        !(sizeof(value_type)==16
          && sizeof(key_type)==8
          && !(mods::template is<hmod::ref_integrity>()
               && mods::template is<hmod::growable>()));


    using slot_config    = typename std::conditional<
        needs_complex_slot,
        complex_slot<key_type, mapped_type, needs_marking>,
        simple_slot <key_type, mapped_type, needs_marking>>::type;

    using base_table_config = base_linear_config<slot_config,
                                                 HashFct,
                                                 Allocator,
                                                 mods::template is<hmod::circular_map>(),
                                                 mods::template is<hmod::circular_prob>(),
                                                 !mods::template is<hmod::growable>()>;


    using base_table_type = base_linear<base_table_config>;

    template <class P>
    using workerstrat = wstrat_user<P>;
    template <class P>
    using exclstrat   = estrat_async<P>;

    using table_type = typename std::conditional<
        !mods::template is<hmod::growable>(),
        base_table_type,
        migration_table<base_table_type,workerstrat,exclstrat>>::type;
};
}
