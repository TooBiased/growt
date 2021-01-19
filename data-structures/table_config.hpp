#pragma once

#include <type_traits>
#include <sstream>

#include "data-structures/element_types/complex_slot.hpp"
#include "data-structures/element_types/simple_slot.hpp"

#include "data-structures/newstrategies/estrat_async.hpp"
#include "data-structures/newstrategies/estrat_sync.hpp"
#include "data-structures/newstrategies/wstrat_user.hpp"
#include "data-structures/newstrategies/wstrat_pool.hpp"

#include "data-structures/hash_table_mods.hpp"
#include "data-structures/base_linear.hpp"
#include "data-structures/migration_table.hpp"

namespace growt
{


template <bool B>
struct slot_config
{
    template <class K, class M, bool NM>
    using templ = complex_slot<K,M,NM>;
};

template<>
struct slot_config<false>
{
    template <class K, class M, bool NM>
    using templ = simple_slot<K, M, NM>;
};


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
    static constexpr bool needs_migration =
        mods::template is<hmod::growable>()
        || mods::template is<hmod::deletion>();
    // template <class K, class M, bool NM>
    // using slot_config    = typename template_conditional<needs_complex_slot,
    //                                                      complex_slot,
    //                                                      simple_slot>::template templ<K,M,NM>;

    using base_table_config = base_linear_config<typename slot_config<needs_complex_slot>::
                                                 template templ<key_type, mapped_type, needs_marking>,
                                                 HashFct,
                                                 Allocator,
                                                 mods::template  is<hmod::circular_map>(),
                                                 mods::template  is<hmod::circular_prob>(),
                                                 !mods::template is<hmod::growable>()>;

    using base_table_type = base_linear<base_table_config>;

    template <class P>
    using workerstrat = typename std::conditional<!mods::template is<hmod::pool>(),
                                                  wstrat_user<P>,
                                                  wstrat_pool<P>
                                                  >::type;
    template <class P>
    using exclstrat   = typename std::conditional<!mods::template is<hmod::sync>(),
                                                  estrat_async<P>,
                                                  estrat_sync<P>
                                                  >::type;



    using table_type = typename std::conditional<
        needs_migration,
        migration_table<base_table_type,workerstrat,exclstrat>,
        base_table_type>::type;


    static std::string name()
    {
        // std::stringstream name;
        // // table type
        // if constexpr (std::is_same<table_type,
        //                            migration_table<base_table_type,
        //                                            workerstrat,
        //                                            exclstrat>>::value)
        // {
        //     name << "mig_table<";
        // }
        // if constexpr (std::is_same<table_type, base_table_type>::value)
        // {
        //     name << "base_table<";
        // }
        // // slot
        // if constexpr (std::is_same<typename base_table_type::slot_config,
        //               simple_slot<key_type, mapped_type, needs_marking>>::value)
        // {
        //     name << "simple";
        // }
        // if constexpr (std::is_same<typename base_table_type::slot_config,
        //               complex_slot<key_type, mapped_type, needs_marking>>::value)
        // {
        //     name << "complex";
        // }
        // // worker strat
        // if constexpr (needs_migration)
        // {
        //     name << ",";
        //     if constexpr (std::is_same<typename base_table_type::)
        //     {

        //     }
        // }
        return table_type::name();
    }
};

}
