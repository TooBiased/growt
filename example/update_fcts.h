#ifndef  UPDATE_FCTS_H
#define UPDATE_FCTS_H

namespace growt{
namespace example{

struct Increment
{
    void operator()(uint64_t& lhs, const uint64_t, const uint64_t rhs) const
    {
        lhs+=rhs;
    }

    // an atomic implementation can improve the performance of updates in .sGrow
    // this will be detected automatically
    void atomic    (uint64_t& lhs, const uint64_t, const uint64_t rhs) const
    {
        __sync_fetch_and_add(&lhs, rhs);
    }

    // Only necessary for JunctionWrapper (not needed)
    using junction_compatible = std::false_type;
};

struct Overwrite
{
    void operator()(uint64_t& lhs, const uint64_t, const uint64_t rhs) const
    {
        lhs = rhs;
    }

    // an atomic implementation can improve the performance of updates in .sGrow
    // this will be detected automatically
    void atomic    (uint64_t& lhs, const uint64_t, const uint64_t rhs) const
    {
        lhs = rhs;
    }

    // Only necessary for JunctionWrapper (not needed)
    using junction_compatible = std::true_type;
};

}
}

#endif // UPDATE_FCTS_H
