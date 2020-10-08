#pragma once

#include <atomic>
#include <tuple>

#include "data-structures/returnelement.hpp"

template <class Key, class Data, bool markable>
class complex_element
{
public:
    using key_type = Key;
    using mapped_type = Data;
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr bool allows_marking               = markable;
    static constexpr bool allows_deletions             = false;
    static constexpr bool allows_atomic_updates        = false;
    static constexpr bool allows_updates               = false;
    static constexpr bool allows_referential_integrity = true;
    static constexpr bool needs_cleanup                = true;


    complex_element();
    complex_element(const key_type& k, const mapped_type& d);
    complex_element(const value_type& pair);
    complex_element(const complex_element& e);
    complex_element& operator=(const complex_element& e);
    complex_element(complex_element &&e);
    complex_element& operator=(complex_element &&e);

    static complex_element get_empty();

    key_type    key;
    mapped_type data;
    static complex_element get_empty() { return complex_element(); }

    bool is_empty()   const;
    bool is_deleted() const;
    bool is_marked()  const;
    bool compare_key(const key_type & k) const;
    bool atomic_mark(complex_element& expected);
    key_type    get_key()  const;
    mapped_type get_data() const;
    bool cas(complex_element & expected, const complex_element & desired);
    bool atomic_delete(const complex_element & expected);

    template<class F, class ...Types>
    std::pair<mapped_type, bool> atomic_update(   complex_element & expected,F f, Types&& ... args);
    template<class F, class ...Types>
    std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ... args);


    inline operator value_type() const;
    inline bool operator==(complex_element& r) { return (key == r.key); }
    inline bool operator!=(complex_element& r) { return (key != r.key); }

private:

    template <bool markable>
    struct ptr_splitter;

    template <>
    struct ptr_splitter<true>
    {
        bool     mark        : 1;
        uint64_t fingerprint : 15;
        uint64_t pointer     : 48;
    };

    template <>
    struct ptr_splitter<false>
    {
        static constexpr bool mark = false;
        uint64_t fingerprint : 16;
        uint64_t pointer     : 48;
    };

    using ptr_split = ptr_splitter<markable>;

    union ptr_union
    {
        uint64_t  full;
        ptr_split split;
    };
    static_assert(sizeof(ptr_union)==8,
                  "complex element union size is unexpected");
    static_assert(std::atomic<ptr_union>::is_always_lock_free,
                  "complex element atomic is not lock free");


    std::atomic<ptr_union> _ptr;
};