#pragma once

#include <atomic>
#include <tuple>
#include <string>

#include "utils/debug.hpp"
namespace debug = utils_tm::debug_tm;

#include "data-structures/returnelement.hpp"

namespace growt
{

template <class Key, class Data>
class seq_complex_slot
{
    struct ptr_split
    {
        uint64_t fingerprint : 16;
        uint64_t pointer     : 48;
    };
    union ptr_union
    {
        uint64_t  full;
        ptr_split split;
    };

    static_assert(sizeof(ptr_union)==8,
                  "complex slot union size is unexpected");

    static constexpr size_t fingerprint_mask = (1ull<<16)-1;
    inline static constexpr size_t fingerprint(size_t hash)
    {
        return hash & fingerprint_mask;
    }

public:
    using key_type = Key;
    using mapped_type = Data;
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr bool allows_marking               = false;
    static constexpr bool allows_deletions             = false;
    static constexpr bool allows_atomic_updates        = false;
    static constexpr bool allows_updates               = false;
    static constexpr bool allows_referential_integrity = true;
    static constexpr bool needs_cleanup                = true;

    class atomic_slot_type;

    // THIS IS AFTER THE SLOT IS READ (i.e. consistency within one query)
    class slot_type
    {
    private:
        ptr_union _mfptr;

        friend atomic_slot_type;
    public:
        slot_type(const key_type& k, const mapped_type& d, size_t hash);
        slot_type(const value_type& pair, size_t hash);
        slot_type(key_type&& k, mapped_type&& d, size_t hash);
        slot_type(value_type&& pair, size_t hash);
        template <class ... Args>
        slot_type(Args&& ... args);
        slot_type(ptr_union source);

        slot_type(const slot_type& source) = default;
        slot_type(slot_type&& source) = default;
        slot_type& operator=(const slot_type& source) = default;
        slot_type& operator=(slot_type&& source) = default;
        ~slot_type() = default;

        inline key_type          get_key() const;
        inline const key_type&   get_key_ref() const;
        inline mapped_type       get_mapped() const;
        inline       value_type* get_pointer();
        inline const value_type* get_pointer() const;
        inline void              set_fingerprint(size_t hash);

        inline bool is_empty()   const;
        inline bool is_deleted() const;
        inline bool is_marked()  const;
        inline bool compare_key(const key_type& k, size_t hash) const;

        inline operator value_type() const;
        inline bool operator==(const slot_type& r) const;
        inline bool operator!=(const slot_type& r) const;

        inline void cleanup();
    };


    // THIS IS IN THE TABLE, IT ONLY HAS THE CAS+UPDATE STUFF
    class atomic_slot_type
    {
    private:
        uint64_t _sptr;

    public:
        atomic_slot_type(const atomic_slot_type& source);
        atomic_slot_type& operator=(const atomic_slot_type& source);
        atomic_slot_type(const slot_type& source);
        atomic_slot_type& operator=(const slot_type& source);
        ~atomic_slot_type() = default;

        slot_type load() const;
        void      non_atomic_set(const slot_type& source);
        // not actually compare and swap
        bool      cas(slot_type& expected, const slot_type& goal);
        // not actually delete
        bool      atomic_delete(slot_type& expected);
        // definitely not markable
        bool      atomic_mark  (slot_type& expected);

        // updates are not implemented for complex slots
        template<class F, class ...Types>
        std::pair<mapped_type, bool> atomic_update(slot_type & expected,
                                                   F f, Types&& ... args);
        template<class F, class ...Types>
        std::pair<mapped_type, bool> non_atomic_update(F f, Types&& ... args);
    };

    // static constexpr slot_type empty{ptr_union{uint64_t(0)}};
    // static constexpr slot_type deleted{ptr_union{uint64_t(1ull<<62)}};

    static constexpr slot_type get_empty()
    {
        ptr_union r = {0};
        return slot_type(r);
    }
    static constexpr slot_type get_deleted()
    {
        ptr_union r = {1ull<<48};
        return slot_type(r);
    }

    static value_type* allocate()
    { return static_cast<value_type*>(malloc(sizeof(value_type)));}
    static void        deallocate(value_type* ptr) { free(ptr); }

    static std::string name()
    {
        return "seq_complex_slot";
    }
};



// SLOT_TYPE *******************************************************************
// *** statics *****************************************************************
// template <class K, class D>
// static seq_complex_slot<K,D>::empty = seq_complex_slot<K,D>::slot_type(
//     typename seq_complex_slot<K,D>::ptr_union{uint64_t(0)});

// *** constructors ************************************************************
template <class K, class D>
seq_complex_slot<K,D>::slot_type::slot_type(const key_type& k,
                                            const mapped_type& d, size_t hash)
    : _mfptr(ptr_union{0})
{
    auto ptr = allocate();
    new (ptr) value_type(k,d);
    _mfptr.split.pointer     = uint64_t(ptr);
    _mfptr.split.fingerprint = seq_complex_slot::fingerprint(hash);
}


template <class K, class D>
seq_complex_slot<K,D>::slot_type::slot_type(const value_type& pair, size_t hash)
    : _mfptr(ptr_union{0})
{
    auto ptr = allocate();
    new (ptr) value_type(pair);
    _mfptr.split.pointer     = uint64_t(ptr);
    _mfptr.split.fingerprint = seq_complex_slot::fingerprint(hash);
}

template <class K, class D>
seq_complex_slot<K,D>::slot_type::slot_type(key_type&& k, mapped_type&& d,
                                            size_t hash)
    : _mfptr(ptr_union{0})
{
    auto ptr = allocate();
    new (ptr) value_type(std::move(k), std::move(d));
    _mfptr.split.pointer     = uint64_t(ptr);
    _mfptr.split.fingerprint = seq_complex_slot::fingerprint(hash);
}

template <class K, class D>
seq_complex_slot<K,D>::slot_type::slot_type(value_type&& pair, size_t hash)
    : _mfptr(ptr_union{0})
{
    auto ptr = allocate();
    new (ptr) value_type(std::move(pair));
    _mfptr.split.pointer     = uint64_t(ptr);
    _mfptr.split.fingerprint = seq_complex_slot::fingerprint(hash);

}

template <class K, class D> template <class ... Args>
seq_complex_slot<K,D>::slot_type::slot_type(Args&& ... args)
    : _mfptr(ptr_union{0})
{
    auto ptr = allocate();
    new (ptr) value_type(std::forward<Args>(args)...);
    _mfptr.split.pointer     = uint64_t(ptr);
    // the fingerprint cannot be computed without the hash function
    // this has to be fixed with the set fingerprint function
    //_mfptr.split.fingerprint = complex_slot::fingerprint(hash);
}

template <class K, class D>
seq_complex_slot<K,D>::slot_type::slot_type(ptr_union source)
    : _mfptr(source) { }

// *** getter ******************************************************************
template <class K, class D>
typename seq_complex_slot<K,D>::key_type
seq_complex_slot<K,D>::slot_type::get_key() const
{
    auto ptr = reinterpret_cast<value_type*>(_mfptr.split.pointer);
    if (!ptr)
    {
        debug::if_debug("getting key from empty slot");
        return key_type();
    }
    return ptr->first;
}

template <class K, class D>
const typename seq_complex_slot<K,D>::key_type&
seq_complex_slot<K,D>::slot_type::get_key_ref() const
{
    auto ptr = reinterpret_cast<value_type*>(_mfptr.split.pointer);
    if (!ptr)
    {
        debug::if_debug("getting key from empty slot");
    }
    return ptr->first;
}

template <class K, class D>
typename seq_complex_slot<K,D>::mapped_type
seq_complex_slot<K,D>::slot_type::get_mapped() const
{
    auto ptr = reinterpret_cast<value_type*>(_mfptr.split.pointer);
    if (!ptr)
    {
        debug::if_debug("getting mapped from empty slot");
        return mapped_type();
    }
    return ptr->second;
}

template <class K, class D>
const typename seq_complex_slot<K,D>::value_type*
seq_complex_slot<K,D>::slot_type::get_pointer() const
{
    return reinterpret_cast<value_type*>(_mfptr.split.pointer);
}

template <class K, class D>
typename seq_complex_slot<K,D>::value_type*
seq_complex_slot<K,D>::slot_type::get_pointer()
{
    return reinterpret_cast<value_type*>(_mfptr.split.pointer);
}

template <class K, class D>
void
seq_complex_slot<K,D>::slot_type::set_fingerprint(size_t hash)
{
    _mfptr.split.fingerprint = seq_complex_slot::fingerprint(hash);
}


// *** state *******************************************************************
template <class K, class D>
bool
seq_complex_slot<K,D>::slot_type::is_empty() const
{
    return _mfptr.full == 0;
}

template <class K, class D>
bool
seq_complex_slot<K,D>::slot_type::is_deleted() const
{
    return _mfptr.full == seq_complex_slot::get_deleted()._mfptr.full;
}

template <class K, class D>
bool
seq_complex_slot<K,D>::slot_type::is_marked() const
{
    return false;
}

template <class K, class D>
bool
seq_complex_slot<K,D>::slot_type::compare_key(const key_type& k, size_t hash) const
{
    if (fingerprint(hash) != _mfptr.split.fingerprint) return false;
    auto ptr = reinterpret_cast<value_type*>(_mfptr.split.pointer);
    if (ptr == nullptr)
    {
        // debug::if_debug("comparison with an empty slot");
        return false;
    }
    return ptr->first == k;
}

// template <class K, class D>
// void
// seq_complex_slot<K,D>::slot_type::cleanup()
// {
//     auto ptr = reinterpret_cast<value_type*>(_mfptr.split.pointer);
//     if (ptr != nullptr)
//     {
//         deallocate(ptr);
//     }
// }

// *** operators ***************************************************************
template <class K, class D>
seq_complex_slot<K,D>::slot_type::operator value_type() const
{
    auto ptr = reinterpret_cast<value_type*>(_mfptr.split.pointer);
    if (ptr == nullptr)
    {
        debug::if_debug("getting value_type from empty slot");
    }
    return *ptr;
}

template <class K, class D>
bool
seq_complex_slot<K,D>::slot_type::operator==(const slot_type& r) const
{
    if (_mfptr.fingerprint != r._mfptr.fingerprint) return false;
    auto ptr0 = reinterpret_cast<value_type*>(_mfptr.split.pointer);
    auto ptr1 = reinterpret_cast<value_type*>(r._mfptr.split.pointer);
    if (!ptr0)
    {
        // debug::if_debug("comparing an empty slot");
        return (!ptr1) ? true : false;
    }
    if (!ptr1)
    {
        // debug::if_debug("comparing an empty slot");
        return false;
    }
    return ptr0->key == ptr1->key;
}

template <class K, class D>
bool
seq_complex_slot<K,D>::slot_type::operator!=(const slot_type& r) const
{
    if (_mfptr.fingerprint != r._mfptr.fingerprint) return false;
    auto ptr0 = reinterpret_cast<value_type*>(_mfptr.split.pointer);
    auto ptr1 = reinterpret_cast<value_type*>(r._mfptr.split.pointer);
    if (!ptr0)
    {
        // debug::if_debug("comparing an empty slot");
        return (!ptr1) ? false : true;
    }
    if (!ptr1)
    {
        // debug::if_debug("comparing an empty slot");
        return true;
    }
    return ptr0->key != ptr1->key;
}


// *** cleanup *****************************************************************
template <class K, class D>
void
seq_complex_slot<K,D>::slot_type::cleanup()
{
    auto ptr = reinterpret_cast<value_type*>(_mfptr.split.pointer);
    if (!ptr)
    {
        return;
    }
    seq_complex_slot::deallocate(ptr);
}



// ATOMIC_SLOT_TYPE ************************************************************
// *** constructors

template <class K, class D>
seq_complex_slot<K,D>::atomic_slot_type::atomic_slot_type(const atomic_slot_type& source)
    : _sptr(source._sptr)
{ }

template <class K, class D>
typename seq_complex_slot<K,D>::atomic_slot_type&
seq_complex_slot<K,D>::atomic_slot_type::operator=(const atomic_slot_type& source)
{
    non_atomic_set(source.load());
    return *this;
}

template <class K, class D>
seq_complex_slot<K,D>::atomic_slot_type::atomic_slot_type(const slot_type& source)
    : _sptr(source._mfptr.full)
{ }

template <class K, class D>
typename seq_complex_slot<K,D>::atomic_slot_type&
seq_complex_slot<K,D>::atomic_slot_type::operator=(const slot_type& source)
{
    non_atomic_set(source);
    return *this;
}

// *** common atomics **********************************************************
template <class K, class D>
typename seq_complex_slot<K,D>::slot_type
seq_complex_slot<K,D>::atomic_slot_type::load() const
{
    ptr_union pu;
    pu.full = _sptr;
    return pu;
}

template <class K, class D>
void
seq_complex_slot<K,D>::atomic_slot_type::non_atomic_set(const slot_type& source)
{
    _sptr = source._mfptr.full;
}

template <class K, class D>
bool
seq_complex_slot<K,D>::atomic_slot_type::cas([[maybe_unused]] slot_type& expected,
                                              const slot_type& goal)
{
    non_atomic_set(goal);
    return true;
}

template <class K, class D>
bool
seq_complex_slot<K,D>::atomic_slot_type::atomic_delete(slot_type& expected)
{
    return _sptr = get_deleted._mfptr.full;
}

template <class K, class D>
bool
seq_complex_slot<K,D>::atomic_slot_type::atomic_mark  (slot_type& expected)
{
    return true;
}

// *** functor style updates ***************************************************
template <class K, class D> template<class F, class ...Types>
std::pair<typename seq_complex_slot<K,D>::mapped_type, bool>
seq_complex_slot<K,D>::atomic_slot_type::atomic_update(slot_type & expected,
                                                     F f, Types&& ... args)
{
    auto key_value = load().get_pointer();
    auto result    = f(key_value->second, std::forward<Types>(args)...);
    return std::make_pair(result, true);
}

template <class K, class D> template<class F, class ...Types>
std::pair<typename seq_complex_slot<K,D>::mapped_type, bool>
seq_complex_slot<K,D>::atomic_slot_type::non_atomic_update(F f, Types&& ... args)
{
    auto key_value = load().get_pointer();
    auto result    = f(key_value->second, std::forward<Types>(args)...);
    return std::make_pair(result, true);
}

}
