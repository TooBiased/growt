
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

#ifndef ICPC
#include <xmmintrin.h>
using int128_t = __int128;
#else
using int128_t = __int128_t;
#endif

#include "data-structures/returnelement.hpp"

namespace growt
{

template <class Key, class Data, bool
          markable=false,
          Key delete_dummy=static_cast<Key>((1ull<<63) -1)>
class simple_slot
{
private:
    static constexpr size_t marked_bit = 1ull<<63;
    static constexpr size_t bitmask    = marked_bit-1;

public:
    using key_type = Key;
    using mapped_type = Data;
    using value_type = std::pair<const key_type, mapped_type>;

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
        slot_type(int128_t source);
        ~slot_type() = default;

        inline key_type    get_key() const;
        inline mapped_type get_mapped() const;

        inline bool is_empty()   const;
        inline bool is_deleted() const;
        inline bool is_marked()  const;
        inline bool compare_key(const key_type & k, size_t hash) const;
        inline void cleanup() const { /* the simple version does not need cleanup */ }

        inline operator value_type() const;
        inline operator int128_t() const;
        inline bool operator==(const slot_type& r) const;
        inline bool operator!=(const slot_type& r) const;
    };

    static_assert(sizeof(slot_type)==16,
                  "sizeof(slot_type) is unexpected (not 128 bit)");

    // THIS IS IN THE TABLE, IT ONLY HAS THE CAS+UPDATE STUFF
    class atomic_slot_type
    {
    private:
        int128_t _raw_data;

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

        template<class F, class ...Types>
        std::pair<mapped_type, bool> atomic_update(slot_type & expected,
                                                   F f, Types&& ... args);
        template<class F, class ...Types>
        std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ... args);
    };

    static constexpr slot_type   get_empty()
    { return slot_type(key_type(), mapped_type(), 0); }
    static constexpr slot_type   get_deleted()
    { return slot_type(delete_dummy, mapped_type(), 0); }

    static std::string name()
    {
        return "simple_slot";
    }
};


// SLOT_TYPE *******************************************************************
// *** constructors ************************************************************
    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::slot_type::slot_type(const key_type& k,
                                                const mapped_type& d,
                                                [[maybe_unused]]size_t hash)
        : key(k), data(d) { }

    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::slot_type::slot_type(const value_type& pair,
                                                [[maybe_unused]]size_t hash)
        : key(pair.first), data(pair.second) { }

    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::slot_type::slot_type(key_type&& k, mapped_type&& d,
                                                [[maybe_unused]]size_t hash)
        : key(k), data(d) { }

    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::slot_type::slot_type(value_type&& pair,
                                                [[maybe_unused]]size_t hash)
        : key(pair.first), data(pair.second) { }

    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::slot_type::slot_type(int128_t source)
    {
        *reinterpret_cast<int128_t*>(this) = source;
    }


// *** getter ******************************************************************
    template <class K, class D, bool m, K dd>
    typename simple_slot<K,D,m,dd>::key_type
    simple_slot<K,D,m,dd>::slot_type::get_key() const
    {
        if constexpr (!m) return key;
        return key & simple_slot::bitmask;
    }

    template <class K, class D, bool m, K dd>
    typename simple_slot<K,D,m,dd>::mapped_type
    simple_slot<K,D,m,dd>::slot_type::get_mapped() const
    {
        return data;
    }


// *** state *******************************************************************
    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::slot_type::is_empty() const
    {
        return key == 0;
    }

    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::slot_type::is_deleted() const
    {
        return key == simple_slot::get_deleted().key;
    }

    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::slot_type::is_marked() const
    {
        if constexpr (!m) return false;
        return key & marked_bit;
    }

    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::slot_type::compare_key(const key_type& k,
                                                  [[maybe_unused]]size_t hash) const
    {
        //return key == k;
        if constexpr (!m) return key == k;
        return (key & bitmask) == k;
    }


// *** operators ***************************************************************
    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::slot_type::operator value_type() const
    {
        return std::make_pair(get_key(), get_mapped());
    }

    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::slot_type::operator int128_t() const
    {
        return *reinterpret_cast<const int128_t*>(this);
    }

    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::slot_type::operator==(const slot_type& r) const
    {
        return key == r.key;
    }

    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::slot_type::operator!=(const slot_type& r) const
    {
        return key != r.key;
    }



// ATOMIC_SLOT_TYPE ************************************************************
// *** constructors ************************************************************
    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::atomic_slot_type::atomic_slot_type(const atomic_slot_type& source)
        : _raw_data(source.load())
    { }

    template <class K, class D, bool m, K dd>
    typename simple_slot<K,D,m,dd>::atomic_slot_type&
    simple_slot<K,D,m,dd>::atomic_slot_type::operator=(const atomic_slot_type& source)
    {
        _raw_data = source.load();
        return *this;
    }

    template <class K, class D, bool m, K dd>
    simple_slot<K,D,m,dd>::atomic_slot_type::atomic_slot_type(const slot_type& source)
        : _raw_data(source)
    { }

    template <class K, class D, bool m, K dd>
    typename simple_slot<K,D,m,dd>::atomic_slot_type&
    simple_slot<K,D,m,dd>::atomic_slot_type::operator=(const slot_type& source)
    {
        non_atomic_set(source);
        return *this;
    }

// *** common atomics **********************************************************
    template <class K, class D, bool m, K dd>
    typename simple_slot<K,D,m,dd>::slot_type
    simple_slot<K,D,m,dd>::atomic_slot_type::load() const
    {
        // Roman used: _mm_loadu_ps Think about using
        // _mm_load_ps because the memory should be aligned

        //as128i() = (int128_t) _mm_loadu_ps((float *) &e);
        auto temp = reinterpret_cast<int128_t>(
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(&_raw_data)));
        return slot_type(temp);
    }

    template <class K, class D, bool m, K dd>
    void
    simple_slot<K,D,m,dd>::atomic_slot_type::non_atomic_set(const slot_type& goal)
    {
        _raw_data = goal;
    }


    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::atomic_slot_type::cas(slot_type& expected,
                                                 slot_type goal)
    {
        return __sync_bool_compare_and_swap_16(&_raw_data,
                                               reinterpret_cast<int128_t&>(expected),
                                               goal);
    }

    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::atomic_slot_type::atomic_delete(slot_type& expected)
    {
        return cas(expected, get_deleted());
    }

    template <class K, class D, bool m, K dd>
    bool
    simple_slot<K,D,m,dd>::atomic_slot_type::atomic_mark(slot_type& expected)
    {
        auto temp = expected;
        temp.key |= marked_bit;
        return cas(expected, temp);
    }

// *** functor style updates ***************************************************
    template <class K, class D, bool m, K dd> template<class F, class ...Types>
    std::pair<typename simple_slot<K,D,m,dd>::mapped_type, bool>
    simple_slot<K,D,m,dd>::atomic_slot_type::atomic_update([[maybe_unused]]slot_type& expected,
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

    template <class K, class D, bool m, K dd> template<class F, class ...Types>
    std::pair<typename simple_slot<K,D,m,dd>::mapped_type, bool>
    simple_slot<K,D,m,dd>::atomic_slot_type::non_atomic_update([[maybe_unused]]F f,
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
