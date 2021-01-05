#pragma once

#include "data-structures/element_types/seq_simple_slot.hpp"
#include "data-structures/element_types/seq_complex_slot.hpp"
#include "data-structures/hash_table_mods.hpp"
#include "data-structures/seq_linear.hpp"

namespace growt
{

template <class Key, class Data, class HashFct, class Allocator,
          hmod ... Mods>
class seq_table_config
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

    static constexpr bool needs_complex_slot =
        !(sizeof(value_type)==16
          && sizeof(key_type)==8
          && !(mods::template is<hmod::ref_integrity>()
               && mods::template is<hmod::growable>()));


    using slot_config    = typename std::conditional<
        needs_complex_slot,
        seq_complex_slot<key_type, mapped_type>,
        seq_simple_slot <key_type, mapped_type>>::type;

    using seq_table_parameters = seq_linear_parameters<slot_config,
                                                       HashFct,
                                                       Allocator,
                                                       mods::template is<hmod::circular_map>(),
                                                       mods::template is<hmod::circular_prob>(),
                                                       true /*Needs Cleanup*/>;

    using table_type = seq_linear<seq_table_parameters>;
};

};
