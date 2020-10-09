#pragma once

#include <atomic>
#include <tuple>

#include "data-structures/returnelement.hpp"

template <class Key, class Data, bool markable>
class complex_slot
{

    template <bool default_true>
    struct ptr_splitter
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
                  "complex slot union size is unexpected");
    static_assert(std::atomic<ptr_union>::is_always_lock_free,
                  "complex slot atomic is not lock free");

    static constexpr size_t fingerprint_mask = (markable) & (1ull<<15)-1
                                                          : (1ull<<16)-1;
    inline static constexpr size_t fingerprint(size_t hash) const
    {
        return hash & fingerprint_mask;
    }

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


    // THIS IS AFTER THE SLOT IS READ (i.e. consistency within one query)
    class slot_type
    {
    private:
        ptr_union _mfptr;

    public:
        slot_type(const key_type& k, const mapped_type& d, size_t hash);
        slot_type(const value_type& pair, size_t hash);
        slot_type(key_type&& k, mapped_type&& d, size_t hash);
        slot_type(value_type&& pair, size_t hash);
        slot_type(const slot_type& source);
        slot_type(slot_type&& source);
        slot_type& operator=(const slot_type& source);
        slot_type& operator=(slot_type&& source);
        slot_type(ptr_union source);

        inline key_type    get_key() const;
        inline mapped_type get_mapped() const;

        inline bool is_empty()   const;
        inline bool is_deleted() const;
        inline bool is_marked()  const;
        inline bool compare_key(const key_type & k, size_t hash) const;

        inline operator value_type() const;
        inline bool operator==(const complex_slot& r) const;
        inline bool operator!=(const complex_slot& r) const;

        inline void cleanup();
    };


    // THIS IS IN THE TABLE, IT ONLY HAS THE CAS+UPDATE STUFF
    class atomic_slot_type
    {
    private:
        std::atomic<uint64_t> _aptr;

    public:
        slot_type load() const;
        bool cas(slot_type& expected, const slot_type& goal);
        bool atomic_delete(slot_type& expected);
        bool atomic_mark  (slot_type& expected);

        template<class F, class ...Types>
        std::pair<mapped_type, bool> atomic_update(complex_slot & expected,
                                                   F f, Types&& ... args);
        template<class F, class ...Types>
        std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ... args);
    };

    static slot_type   get_empty();
    static slot_type   get_deleted();
    static value_type* allocate();
    static void        deallocate(value_type* ptr);
};


// SLOT_TYPE *******************************************************************
// *** constructors ************************************************************
template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type(const key_type& k,
                                             const mapped_type& d, size_t hash)
{
    auto ptr = allocate();
    new (ptr) value_type(k,d);
    _mfptr.split.pointer     = ptr;
    _mfptr.split.fingerprint = complex_slot::fingerprint(hash);
}


template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type(const value_type& pair, size_t hash)
{
    auto ptr = allocate();
    new (ptr) value_type(pair);
    _mfptr.split.pointer     = ptr;
    _mfptr.split.fingerprint = complex_slot::fingerprint(hash);
}

template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type(key_type&& k, mapped_type&& d,
                                          size_t hash)
{
    auto ptr = allocate();
    new (ptr) value_type(std::move(k), std::move(d));
    _mfptr.split.pointer     = ptr;
    _mfptr.split.fingerprint = complex_slot::fingerprint(hash);
}

template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type(value_type&& pair, size_t hash)
{
    auto ptr = allocate();
    new (ptr) value_type(std::move(pair));
    _mfptr.split.pointer     = ptr;
    _mfptr.split.fingerprint = complex_slot::fingerprint(hash);

}

template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type(const slot_type& source)
    : _mfptr(source._mfptr) { }

template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type(slot_type&& source)
    : _mfptr(source._mfptr) { }

template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type& operator=(const slot_type& source)
{
    _mptr = source._mptr;
}

template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type& operator=(slot_type&& source);
{
    _mptr = source._mptr;
}

template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::slot_type(ptr_union source)
    : _mfptr(source) { }


// *** getter ******************************************************************
template <class K, class D, bool m>
typename complex_slot<K,D,m>::key_type
complex_slot<K,D,m>::slot_type::get_key() const
{
    auto ptr = static_cast<value_type*>(_mfptr.split.pointer);
    return ptr->first;
}

template <class K, class D, bool m>
typename complex_slot<K,D,m>::mapped_type
complex_slot<K,D,m>::slot_type::get_mapped() const
{
    auto ptr = static_cast<value_type*>(_mfptr.split.pointer);
    return ptr->second;
}


// *** state *******************************************************************
template <class K, class D, bool m>
bool
complex_slot<K,D,m>::slot_type::is_empty() const
{
    return !_mfptr.split.pointer;
}

template <class K, class D, bool m>
bool
complex_slot<K,D,m>::slot_type::is_deleted() const
{
    return _mfptr.full == complex_slot::get_deleted()._mfptr.full;
}

template <class K, class D, bool m>
bool
complex_slot<K,D,m>::slot_type::is_marked() const
{
    if constexpr (!m) return false;
    return _mfptr.split.mark;
}

template <class K, class D, bool m>
bool
complex_slot<K,D,m>::slot_type::compare_key(const key& k, size_t hash) const
{
    if (fingerprint(hash) != _mfptr.split.fingerprint) return false;
    auto ptr = static_cast<value_type*>(_mfptr.split.pointer);
    return ptr->first == k;
}


// *** operators ***************************************************************
template <class K, class D, bool m>
complex_slot<K,D,m>::slot_type::operator value_type() const
{

    auto ptr = static_cast<value_type*>(_mfptr.split.pointer);
    return *ptr;
}

template <class K, class D, bool m>
bool
complex_slot<K,D,m>::slot_type::operator==(const slot_type& r) const
{
    if (_mfptr.fingerprint != r._mfptr.fingerprint) return false;
    auto ptr0 = static_cast<value_type*>(_mfptr.split.pointer);
    auto ptr1 = static_cast<value_type*>(r._mfptr.split.pointer);
    return ptr0->key == ptr1->key;
}

template <class K, class D, bool m>
bool
complex_slot<K,D,m>::slot_type::operator!=(const slot_type& r) const
{
    if (_mfptr.fingerprint != r._mfptr.fingerprint) return false;
    auto ptr0 = static_cast<value_type*>(_mfptr.split.pointer);
    auto ptr1 = static_cast<value_type*>(r._mfptr.split.pointer);
    return ptr0->key == ptr1->key;
}


// *** cleanup *****************************************************************
template <class K, class D, bool m>
void
complex_slot<K,D,m>::slot_type::cleanup()
{
    auto ptr = static_cast<value_type*>(_mfptr.plit.pointer);
    complex_slot::deallocate(ptr);
}



// ATOMIC_SLOT_TYPE ************************************************************
// *** common atomics **********************************************************
template <class K, class D, bool m>
typename complex_slot<K,D,m>::slot_type
complex_slot<K,D,m>::atomic_slot_type::load() const
{
    ptr_union pu;
    pu.full = _aptr.load(std::memory_order_relaxed);
    return pu;
}

template <class K, class D, bool m>
bool
complex_slot<K,D,m>::atomic_slot_type::cas(slot_type& expected,
                                              const slot_type& goal)
{
    return _aptr.compare_exchange_strong(expected._mfptr.full,
                                         goal._mfptr.full,
                                         std::memory_order_relaxed);
}

template <class K, class D, bool m>
bool
complex_slot<K,D,m>::atomic_slot_type::atomic_delete(slot_type& expected)
{
    return _aptr.compare_exchange_strong(expected._mfptr.full,
                                         get_deleted._mfptr.full,
                                         std::memory_order_relaxed);
}

template <class K, class D, bool m>
bool
complex_slot<K,D,m>::atomic_slot_type::atomic_mark  (slot_type& expected)
{
    if constexpr (!m) return true;
    ptr_union pu = expected._mfptr;
    pu.mark = true;
    return _aptr.compare_exchange_strong(expected._mfptr.full,
                                         pu.full,
                                         std::memory_order_relaxed);
}

// *** functor style updates ***************************************************
template<class F, class ...Types>
std::pair<typename complex_slot<K,D,m>::mapped_type, bool>
complex_slot<K,D,m>::atomic_slot_type::atomic_update(complex_slot & expected,
                                           F f, Types&& ... args)
{
    if constexpr (! debug::debug_mode) return false;
    static std::atomic_bool once = true;
    if (once.load())
        debug::if_debug("atomic update is not implemented in complex types",
                        once.exchange(false));
}

template<class F, class ...Types>
std::pair<typename complex_slot<K,D,m>::mapped_type, bool>
complex_slot<K,D,m>::atomic_slot_type::non_atomic_update(F f, Types&& ... args);
{
    if constexpr (! debug::debug_mode) return false;
    static std::atomic_bool once = true;
    if (once.load())
        debug::if_debug("non-atomic update is not implemented in complex types",
                        once.exchange(false));
}
