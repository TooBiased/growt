#pragma once

#include "cstddef"

enum class hmod : size_t
{
growable      = 1,
deletion      = 2,
ref_integrity = 4,
sync          = 8,
pool          = 16,
circular_map  = 32,
circular_prob = 64
};

template <hmod ... Mods>
class mod_aggregator
{
public:
    static constexpr size_t mod_descriptor =
        ( static_cast<size_t>(Mods) | ... );

public:
    template <hmod Ask>
    static constexpr bool is()
    {
        auto ask = static_cast<size_t>(Ask);
        return mod_descriptor & ask;
    }
};
