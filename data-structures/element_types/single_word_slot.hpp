
#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <stdlib.h>
#include <string>
#include <tuple>

#include <atomic>

#include "../../utils/concurrency/memory_order.hpp"
#include "../../utils/debug.hpp"
namespace debug = utils_tm::debug_tm;

#include "../../data-structures/returnelement.hpp"

namespace growt
{

// template <class, bool> class _atomic_helper_type;

template <class Key,
          class Data,
          bool markable     = false,
          Key  delete_dummy = static_cast<Key>((1ull << 31) - 1)>
class single_word_slot
{
  private:
    using memo = utils_tm::concurrency_tm::standard_memory_order_policy;
    static constexpr size_t marked_bit = 1ull << 31;
    static constexpr size_t bitmask    = marked_bit - 1;

  public:
    using key_type    = Key;
    using mapped_type = Data;
    using value_type  = std::pair<const key_type, mapped_type>;

    static constexpr bool allows_marking               = markable;
    static constexpr bool allows_deletions             = true;
    static constexpr bool allows_atomic_updates        = !markable;
    static constexpr bool allows_updates               = true;
    static constexpr bool allows_referential_integrity = false;
    static constexpr bool needs_cleanup                = false;

    class atomic_slot_type;

    // THIS IS AFTER THE ELEMENT IS READ (i.e. consistency within one query)
    class slot_type
    {
      private:
        key_type    key;
        mapped_type data;

        friend class atomic_slot_type;

      public:
        slot_type(const key_type& k, const mapped_type& d, size_t hash = 0);
        slot_type(const value_type& pair, size_t hash = 0);
        slot_type(key_type&& k, mapped_type&& d, size_t hash = 0);
        slot_type(value_type&& pair, size_t hash = 0);
        // template <class ... Args>
        // slot_type(Args&& ... args);

        slot_type(const slot_type& source)                = default;
        slot_type(slot_type&& source) noexcept            = default;
        slot_type& operator=(const slot_type& source)     = default;
        slot_type& operator=(slot_type&& source) noexcept = default;
        slot_type(uint64_t source);
        ~slot_type() = default;

        inline key_type        get_key() const;
        inline const key_type& get_key_ref() const;
        inline mapped_type     get_mapped() const;
        inline void            set_mapped(const mapped_type& m);
        inline void            set_fingerprint(size_t) const;

        inline bool is_empty() const;
        inline bool is_deleted() const;
        inline bool is_marked() const;
        inline bool compare_key(const key_type& k, size_t hash) const;
        inline void cleanup() const
        { /* the simple version does not need cleanup */
        }

        inline      operator value_type() const;
        inline      operator uint64_t() const;
        inline      operator uint64_t&();
        inline bool operator==(const slot_type& r) const;
        inline bool operator!=(const slot_type& r) const;
    };

    static_assert(
        sizeof(slot_type) == 8,
        "sizeof(slot_type) in single_word_slot is unexpected (not 64 bit)");

    // THIS IS IN THE TABLE, IT ONLY HAS THE CAS+UPDATE STUFF
    class atomic_slot_type
    {
      private:
        std::atomic_uint64_t _raw_data;

        template <class, bool>
        friend class _atomic_helper_type;

      public:
        atomic_slot_type(const atomic_slot_type& source);
        atomic_slot_type& operator=(const atomic_slot_type& source);
        atomic_slot_type(const slot_type& source);
        atomic_slot_type& operator=(const slot_type& source);
        ~atomic_slot_type() = default;

        slot_type load() const;
        void      non_atomic_set(const slot_type& goal);
        bool cas(slot_type& expected, slot_type goal, bool release = false);
        bool atomic_delete(slot_type& expected);
        bool atomic_mark(slot_type& expected);

        template <class F, class... Types>
        std::pair<slot_type, bool>
        atomic_update(slot_type& expected, F f, Types&&... args);
        template <class F, class... Types>
        std::pair<slot_type, bool> non_atomic_update(F f, Types&&... args);
    };

    static constexpr slot_type get_empty()
    {
        return slot_type(key_type(), mapped_type(), 0);
    }
    static constexpr slot_type get_deleted()
    {
        return slot_type(delete_dummy, mapped_type(), 0);
    }

    static std::string name() { return "single_word_slot"; }
};


// SLOT_TYPE *******************************************************************
// *** constructors ************************************************************
template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::slot_type::slot_type(
    const key_type& k, const mapped_type& d, [[maybe_unused]] size_t hash)
    : key(k), data(d)
{
}

template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::slot_type::slot_type(
    const value_type& pair, [[maybe_unused]] size_t hash)
    : key(pair.first), data(pair.second)
{
}

template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::slot_type::slot_type(
    key_type&& k, mapped_type&& d, [[maybe_unused]] size_t hash)
    : key(k), data(d)
{
}

template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::slot_type::slot_type(
    value_type&& pair, [[maybe_unused]] size_t hash)
    : key(pair.first), data(pair.second)
{
}

// template <class K, class D, bool m, K dd> template <class ... Args>
// single_word_slot<K,D,m,dd>::slot_type::slot_type(Args&& ... args)
// {
//     using pair_type = std::pair<key_type, mapped_type>;
//     *reinterpret_cast<pair_type*>(this) =
//     pair_type(std::forward<Args>(args)...);
// }

template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::slot_type::slot_type(uint64_t source)
{
    *reinterpret_cast<uint64_t*>(this) = source;
}


// *** getter ******************************************************************
template <class K, class D, bool m, K dd>
typename single_word_slot<K, D, m, dd>::key_type
single_word_slot<K, D, m, dd>::slot_type::get_key() const
{
    if constexpr (!m) return key;
    return key & single_word_slot::bitmask;
}

template <class K, class D, bool m, K dd>
const typename single_word_slot<K, D, m, dd>::key_type&
single_word_slot<K, D, m, dd>::slot_type::get_key_ref() const
{
    return key;
}

template <class K, class D, bool m, K dd>
typename single_word_slot<K, D, m, dd>::mapped_type
single_word_slot<K, D, m, dd>::slot_type::get_mapped() const
{
    return data;
}

template <class K, class D, bool m, K dd>
void single_word_slot<K, D, m, dd>::slot_type::set_mapped(
    const mapped_type& mapped)
{
    data = mapped;
}

template <class K, class D, bool m, K dd>
void single_word_slot<K, D, m, dd>::slot_type::set_fingerprint(size_t) const
{
}

// *** state *******************************************************************
template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::slot_type::is_empty() const
{
    return (key & bitmask) == 0;
}

template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::slot_type::is_deleted() const
{
    return key == single_word_slot::get_deleted().key;
}

template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::slot_type::is_marked() const
{
    if constexpr (!m) return false;
    return key & marked_bit;
}

template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::slot_type::compare_key(
    const key_type& k, [[maybe_unused]] size_t hash) const
{
    // return key == k;
    if constexpr (!m) return key == k;
    return (key & bitmask) == k;
}


// *** operators ***************************************************************
template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::slot_type::operator value_type() const
{
    return std::make_pair(get_key(), get_mapped());
}

template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::slot_type::operator uint64_t() const
{
    return *reinterpret_cast<const uint64_t*>(this);
}

template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::slot_type::operator uint64_t&()
{
    return reinterpret_cast<uint64_t&>(*this);
}

template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::slot_type::operator==(
    const slot_type& r) const
{
    return key == r.key;
}

template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::slot_type::operator!=(
    const slot_type& r) const
{
    return key != r.key;
}



// ATOMIC_SLOT_TYPE ************************************************************
// *** constructors ************************************************************
template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::atomic_slot_type::atomic_slot_type(
    const atomic_slot_type& source)
    : _raw_data(source.load())
{
}

template <class K, class D, bool m, K dd>
typename single_word_slot<K, D, m, dd>::atomic_slot_type&
single_word_slot<K, D, m, dd>::atomic_slot_type::operator=(
    const atomic_slot_type& source)
{
    _raw_data = source.load();
    return *this;
}

template <class K, class D, bool m, K dd>
single_word_slot<K, D, m, dd>::atomic_slot_type::atomic_slot_type(
    const slot_type& source)
    : _raw_data(source)
{
}

template <class K, class D, bool m, K dd>
typename single_word_slot<K, D, m, dd>::atomic_slot_type&
single_word_slot<K, D, m, dd>::atomic_slot_type::operator=(
    const slot_type& source)
{
    non_atomic_set(source);
    return *this;
}

// *** common atomics **********************************************************
template <class K, class D, bool m, K dd>
typename single_word_slot<K, D, m, dd>::slot_type
single_word_slot<K, D, m, dd>::atomic_slot_type::load() const
{
    auto temp = _raw_data.load(memo::acquire);
    return slot_type(temp);
}

template <class K, class D, bool m, K dd>
void single_word_slot<K, D, m, dd>::atomic_slot_type::non_atomic_set(
    const slot_type& goal)
{
    // todo, this is not super transportable
    reinterpret_cast<uint64_t&>(_raw_data) = goal;
}


template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::atomic_slot_type::cas(
    slot_type& expected, slot_type goal, [[maybe_unused]] bool release)
{
    return _raw_data.compare_exchange_strong(expected, goal, memo::acq_rel);
}

template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::atomic_slot_type::atomic_delete(
    slot_type& expected)
{
    return cas(expected, get_deleted());
}

template <class K, class D, bool m, K dd>
bool single_word_slot<K, D, m, dd>::atomic_slot_type::atomic_mark(
    slot_type& expected)
{
    if constexpr (!m) return true;
    auto temp = expected;
    temp.key |= marked_bit;
    return cas(expected, temp);
}

// *** SFINAE based helper for atomic updates **********************************
// ***** (checks whether the update_functor has a *.atomic() function)

namespace _single_word_atomic_helper
{

// TESTS IF A GIVEN OBJECT HAS A FUNCTION NAMED atomic
template <typename TFunctor>
class _has_atomic_type
{
    using one = char;
    using two = long;

    template <class C>
    static one test(decltype(&C::atomic));
    template <class C>
    static two test(...);

  public:
    enum
    {
        value = sizeof(test<TFunctor>(0)) == sizeof(char)
    };
};

// USED IF f.atomic(...) EXISTS
template <class SlotType, bool TTrue>
class _atomic_helper_type
{
  public:
    template <class F, class... Types>
    inline static std::pair<typename SlotType::mapped_type, bool>
    execute(typename SlotType::atomic_slot_type* that,
            typename SlotType::slot_type&,
            F f,
            Types&&... args)
    {
        // UGLY (Same as non_atomic_update)
        auto temp   = reinterpret_cast<typename SlotType::value_type*>(that);
        auto result = f.atomic(temp->second, std::forward<Types>(args)...);
        return std::make_pair(result, true);
    }
};

// USED OTHERWISE
template <class SlotType>
class _atomic_helper_type<SlotType, false>
{
  public:
    template <class F, class... Types>
    inline static std::pair<typename SlotType::mapped_type, bool>
    execute(typename SlotType::atomic_slot_type* that,
            typename SlotType::slot_type&        expected,
            F                                    f,
            Types&&... args)
    {
        using pair_type          = typename SlotType::value_type;
        using mapped_type        = typename SlotType::mapped_type;
        using atomic_mapped_type = std::atomic<mapped_type>;
        auto temp                = reinterpret_cast<pair_type*>(that);
        auto atomapped = reinterpret_cast<atomic_mapped_type*>(&(temp->second));
        auto expmapped = expected.get_mapped();
        auto newmapped = expmapped;
        f(newmapped, std::forward<Types>(args)...);
        bool succ = atomapped.compare_exchange_strong(
            expmapped, newmapped, std::memory_order_relaxed);
        return std::make_pair(succ ? newmapped : expmapped, succ);
    }
};

} // namespace _single_word_atomic_helper

// *** functor style updates ***************************************************
template <class K, class D, bool m, K dd>
template <class F, class... Types>
std::pair<typename single_word_slot<K, D, m, dd>::slot_type, bool>
single_word_slot<K, D, m, dd>::atomic_slot_type::atomic_update(
    [[maybe_unused]] slot_type& expected,
    [[maybe_unused]] F          f,
    [[maybe_unused]] Types&&... args)
{
    if constexpr (m == true)
    {
        auto temp = expected;
        f(temp.data, std::forward<Types>(args)...);
        return std::make_pair(temp, cas(expected, temp, true));
    }
    else
    {
        auto temp = _single_word_atomic_helper::_atomic_helper_type<
            single_word_slot, _single_word_atomic_helper::_has_atomic_type<
                                  F>::value>::execute(this, expected, f,
                                                      std::forward<Types>(
                                                          args)...);
        return std::make_pair(slot_type(expected.get_key(), temp.first),
                              temp.second);
    }
}

template <class K, class D, bool m, K dd>
template <class F, class... Types>
std::pair<typename single_word_slot<K, D, m, dd>::slot_type, bool>
single_word_slot<K, D, m, dd>::atomic_slot_type::non_atomic_update(
    [[maybe_unused]] F f, [[maybe_unused]] Types&&... args)
{
    // THIS COULD BE PRETTIER, BUT NON-ATOMIC-UPDATES ARE INHERENTLY UNSAFE
    auto this_pair_view =
        reinterpret_cast<std::pair<key_type, mapped_type>*>(this);
    auto curr = this->load();
    return std::make_pair(
        slot_type(curr.get_key(),
                  f(this_pair_view->second, std::forward<Types>(args)...)),
        true);
}
} // namespace growt
