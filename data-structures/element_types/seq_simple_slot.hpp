
#pragma once

#include <stdlib.h>
#include <cstdint>
#include <functional>
#include <limits>
#include <tuple>
#include <string>

#include <atomic>

#include "utils/debug.hpp"
namespace debug = utils_tm::debug_tm;

#include "data-structures/returnelement.hpp"

namespace growt
{

template <class Key, class Data,
          Key delete_dummy=static_cast<Key>((1ull<<63) -1)>
class seq_simple_slot
{
public:
    using key_type    = Key;
    using mapped_type = Data;
    using value_type  = std::pair<const key_type, mapped_type>;

    static constexpr bool allows_marking               = false;
    static constexpr bool allows_deletions             = true;
    static constexpr bool allows_atomic_updates        = true;
    static constexpr bool allows_updates               = true;
    static constexpr bool allows_referential_integrity = false;
    static constexpr bool needs_cleanup                = false;

    class atomic_slot_type;

    // THIS IS AFTER THE ELEMENT IS READ (i.e. consistency within one query)
    class slot_type
    {
    private:
        key_type key;
        mapped_type data;

        friend class atomic_slot_type;
    public:
        slot_type(const key_type& k, const mapped_type& d, size_t hash);
        slot_type(const value_type& pair, size_t hash);
        slot_type(key_type&& k, mapped_type&& d, size_t hash);
        slot_type(value_type&& pair, size_t hash);

        slot_type(const slot_type& source) = default;
        slot_type(slot_type&& source) noexcept = default;
        slot_type& operator=(const slot_type& source) = default;
        slot_type& operator=(slot_type&& source) noexcept = default;
        ~slot_type() = default;

        inline key_type    get_key() const;
        inline mapped_type get_mapped() const;

        inline bool is_empty()   const;
        inline bool is_deleted() const;
        inline bool is_marked()  const;
        inline bool compare_key(const key_type & k, size_t hash) const;
        inline void cleanup() const { /* the simple version does not need cleanup */ }

        inline operator value_type() const;
        inline bool operator==(const slot_type& r) const;
        inline bool operator!=(const slot_type& r) const;
    };

    static_assert(sizeof(slot_type)==16,
                  "sizeof(slot_type) is unexpected (not 128 bit)");

    // THIS IS IN THE TABLE, IT ONLY HAS THE CAS+UPDATE STUFF
    // sequential dummy necessary, to offer the same interface the same interface
    class atomic_slot_type
    {
    private:
        std::pair<key_type, mapped_type> _pair;

    public:
        atomic_slot_type(const atomic_slot_type& source);
        atomic_slot_type& operator=(const atomic_slot_type& source);
        atomic_slot_type(const slot_type& source);
        atomic_slot_type& operator=(const slot_type& source);
        ~atomic_slot_type() = default;

        slot_type load() const;
        void      non_atomic_set(const slot_type& goal);
        bool cas(slot_type& expected, slot_type goal);
        bool atomic_delete(slot_type& expected);
        bool atomic_mark  (slot_type& expected);

        // same as non-atomic update
        template<class F, class ...Types>
        std::pair<mapped_type, bool> atomic_update(slot_type & expected,
                                                   F f, Types&& ... args);
        template<class F, class ...Types>
        std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ... args);
    };

    static constexpr slot_type   get_empty()
    { return slot_type(key_type()  , mapped_type(), 0); }
    static constexpr slot_type   get_deleted()
    { return slot_type(delete_dummy, mapped_type(), 0); }

    static std::string name()
    {
        return "seq_simple_slot";
    }
};


// SLOT_TYPE *******************************************************************
// *** constructors ************************************************************
    template <class K, class D, K dd>
    seq_simple_slot<K,D,dd>::slot_type::slot_type(const key_type& k,
                                                  const mapped_type& d,
                                                  [[maybe_unused]]size_t hash)
        : key(k), data(d) { }

    template <class K, class D, K dd>
    seq_simple_slot<K,D,dd>::slot_type::slot_type(const value_type& pair,
                                                  [[maybe_unused]]size_t hash)
        : key(pair.first), data(pair.second) { }

    template <class K, class D, K dd>
    seq_simple_slot<K,D,dd>::slot_type::slot_type(key_type&& k, mapped_type&& d,
                                                  [[maybe_unused]]size_t hash)
        : key(k), data(d) { }

    template <class K, class D, K dd>
    seq_simple_slot<K,D,dd>::slot_type::slot_type(value_type&& pair,
                                                  [[maybe_unused]]size_t hash)
        : key(pair.first), data(pair.second) { }


// *** getter ******************************************************************
    template <class K, class D, K dd>
    typename seq_simple_slot<K,D,dd>::key_type
    seq_simple_slot<K,D,dd>::slot_type::get_key() const
    {
        return key;
    }

    template <class K, class D, K dd>
    typename seq_simple_slot<K,D,dd>::mapped_type
    seq_simple_slot<K,D,dd>::slot_type::get_mapped() const
    {
        return data;
    }


// *** state *******************************************************************
    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::slot_type::is_empty() const
    {
        return key == 0;
    }

    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::slot_type::is_deleted() const
    {
        return key == seq_simple_slot::get_deleted().key;
    }

    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::slot_type::is_marked() const
    {
        return false;
    }

    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::slot_type::compare_key(const key_type& k,
                                                  [[maybe_unused]]size_t hash) const
    {
        return key == k;
    }


// *** operators ***************************************************************
    template <class K, class D, K dd>
    seq_simple_slot<K,D,dd>::slot_type::operator value_type() const
    {
        return std::make_pair(get_key(), get_mapped());
    }

    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::slot_type::operator==(const slot_type& r) const
    {
        return key == r.key;
    }

    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::slot_type::operator!=(const slot_type& r) const
    {
        return key != r.key;
    }



// ATOMIC_SLOT_TYPE ************************************************************
// *** constructors ************************************************************
    template <class K, class D, K dd>
    seq_simple_slot<K,D,dd>::atomic_slot_type::atomic_slot_type(const atomic_slot_type& source)
        : _pair(source._pair)
    { }

    template <class K, class D, K dd>
    typename seq_simple_slot<K,D,dd>::atomic_slot_type&
    seq_simple_slot<K,D,dd>::atomic_slot_type::operator=(const atomic_slot_type& source)
    {
        _pair = source._pair;
        return *this;
    }

    template <class K, class D, K dd>
    seq_simple_slot<K,D,dd>::atomic_slot_type::atomic_slot_type(const slot_type& source)
        : _pair(source._pair)
    { }

    template <class K, class D, K dd>
    typename seq_simple_slot<K,D,dd>::atomic_slot_type&
    seq_simple_slot<K,D,dd>::atomic_slot_type::operator=(const slot_type& source)
    {
        _pair.first  = source.key;
        _pair.second = source.data;
        return *this;
    }

// *** common atomics **********************************************************
    template <class K, class D, K dd>
    typename seq_simple_slot<K,D,dd>::slot_type
    seq_simple_slot<K,D,dd>::atomic_slot_type::load() const
    {
        return slot_type(_pair,0);
    }

    template <class K, class D, K dd>
    void
    seq_simple_slot<K,D,dd>::atomic_slot_type::non_atomic_set(const slot_type& goal)
    {
        _pair.first  = goal.key;
        _pair.second = goal.data;
    }


    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::atomic_slot_type::cas(
        [[maybe_unused]] slot_type& expected,
        slot_type goal)
    {
        return non_atomic_set(goal);
    }

    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::atomic_slot_type::atomic_delete(slot_type& expected)
    {
        return cas(expected, get_deleted());
    }

    template <class K, class D, K dd>
    bool
    seq_simple_slot<K,D,dd>::atomic_slot_type::atomic_mark(slot_type& expected)
    {
        return true;
    }

// *** functor style updates ***************************************************
    template <class K, class D, K dd> template<class F, class ...Types>
    std::pair<typename seq_simple_slot<K,D,dd>::mapped_type, bool>
    seq_simple_slot<K,D,dd>::atomic_slot_type::atomic_update([[maybe_unused]]slot_type& expected,
                                                               [[maybe_unused]]F f,
                                                               [[maybe_unused]]Types&& ... args)
    {
        // TODO not implemented
        if constexpr (! debug::debug_mode)
                         return std::make_pair(mapped_type(), false);
        static std::atomic_bool once = true;
        if (once.load())
            debug::if_debug("non-atomic update is not implemented in complex types",
                            once.exchange(false));
    }

    template <class K, class D, K dd> template<class F, class ...Types>
    std::pair<typename seq_simple_slot<K,D,dd>::mapped_type, bool>
    seq_simple_slot<K,D,dd>::atomic_slot_type::non_atomic_update([[maybe_unused]]F f,
                                                               [[maybe_unused]]Types&& ... args)
    {
        // TODO not implemented
        if constexpr (! debug::debug_mode)
                         return std::make_pair(mapped_type(), false);
        static std::atomic_bool once = true;
        if (once.load())
            debug::if_debug("non-atomic update is not implemented in complex types",
                            once.exchange(false));
    }
}
