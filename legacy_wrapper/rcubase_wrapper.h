/*******************************************************************************
 * wrapper/shun_wrapper.h
 *
 * Wrapper to use the Hash Tables programmed by Julian Shun as part of his
 * Dissertation with Prof. Guy Blelloch.
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef RCU_BASE_WRAPPER
#define RCU_BASE_WRAPPER

#ifndef QSBR
#include <urcu.h>
#else
#include <urcu-qsbr.h>
#endif

#include <urcu/rculfhash.h>
#include <urcu/compiler.h>

#include <ctime>
#include <iostream>
#include "wrapper/stupid_iterator.hpp"


template<class Hasher, class Allocator>
class rcu_wrapper
{
private:
    using uint64 = uint64_t;
    struct mynode {
        uint64 key;
        uint64 value;

        struct cds_lfht_node node;	/* Chaining in hash table */
        struct rcu_head rcu_head;	/* For call_rcu() */
    };
    using allocator_type     = typename Allocator::template rebind<mynode>::other;


    static inline size_t h(uint64 key)
    {
        return Hasher()(key);
    }

    static inline int match(struct cds_lfht_node *ht_node, const void *_key)
    {
        mynode *node = caa_container_of(ht_node, struct mynode, node);
        uint64 *key = (uint64*) _key;
        return *key == node->key;
    }

    static inline void free_node(struct rcu_head *head)
    {
        struct mynode *node = caa_container_of(head, struct mynode, rcu_head);
        //free(node);
        allocator_type().destroy(node);
    }

    static inline void qsbr_state()
    {
        #ifdef QSBR
        // static thread_local alignas(64) size_t counter = 0;
        // if (counter++ >= 64)
        // {
        rcu_quiescent_state();
        // }
        #endif
    }

    cds_lfht *hash_table;
    allocator_type allocator;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<size_t, size_t>;
    using insert_return_type = std::pair<iterator, bool>;

    rcu_wrapper() = default;
    rcu_wrapper(size_t capacity_)
    {
        size_t initial_size = 1;
        while (initial_size < capacity_) initial_size <<= 1;
        hash_table = cds_lfht_new(initial_size, initial_size, 0,
            CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING, //0,
            NULL);
    }

    rcu_wrapper(const rcu_wrapper&) = delete;
    rcu_wrapper& operator=(const rcu_wrapper&) = delete;

    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    rcu_wrapper(rcu_wrapper&& rhs) : hash_table(rhs.hash_table)
    {
        rhs.hash_table = nullptr;
    }

    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    rcu_wrapper& operator=(rcu_wrapper&& rhs)
    {
        hash_table = rhs.hash_table;
        rhs.hash_table = nullptr;
        return *this;
    }

    ~rcu_wrapper() { /* SHOULD DO SOMETHING */ }


    using handle_type = rcu_wrapper&;
    handle_type get_handle()
    {
        static thread_local bool already_registered = false;
        if (! already_registered)
        {
            rcu_register_thread();
            already_registered = true;
        }
        return *this;
    }


    inline iterator find(const key_type& k)
    {
        uint64 key_copy = k;
        cds_lfht_iter iter;
        mynode* node;
        uint64 value = 0;
        unsigned long hash = h(k);//jhash(&key_copy, sizeof(ll), seed);

        rcu_read_lock();
        cds_lfht_lookup(hash_table, hash, match, &key_copy, &iter);
        //std::cout << iter << " " << std::flush;
        cds_lfht_node *ht_node = cds_lfht_iter_get_node(&iter);

        if (ht_node) {

            node = caa_container_of(ht_node, struct mynode, node);
            value = node->value;
        }
        rcu_read_unlock();

        qsbr_state();

        if (ht_node) return iterator(k,value);
        else         return end();
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
    {
        //mynode* node = (mynode*)malloc(sizeof(*node));
        mynode* node = allocator.allocate(1);
        cds_lfht_node_init(&node->node);
        node->key   = k;
        node->value = d;
        unsigned long hash = h(k);//jhash(&(node->key), sizeof(ll), seed);

        rcu_read_lock();
        cds_lfht_add(hash_table, hash, &node->node);
        rcu_read_unlock();

        qsbr_state();

        return std::make_pair(iterator(k,d), true);
    }


    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k,
                                     F f, Types&& ... args)
    { return std::make_pair(iterator(), false); }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate(const key_type& k,
                                             const mapped_type& d,
                                             F f, Types&& ... args)
    { return std::make_pair(iterator(), false); }

    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k,
                                            F f, Types&& ... args)
    { return std::make_pair(iterator(), false); }

    template<class F, class ... Types>
    inline insert_return_type insertOrUpdate_unsafe(const key_type& k,
                                                    const mapped_type& d,
                                                    F f, Types&& ... args)
    { return std::make_pair(iterator(), false); }


    inline size_t erase(const key_type& k)
    {
        return 0;
    }

    inline iterator end() { return iterator(); }
};

template <class Key, class Data, class HashFct, class Allocator, hmod ... Mods>
class rcu_config
{
public:
    using key_type = Key;
    using mapped_type = Data;
    using hash_fct_type = HashFct;
    using allocator_type = Allocator;

    using mods = mod_aggregator<Mods ...>;

    // derived Types
    using value_type = std::pair<const key_type, mapped_type>;

    static constexpr bool is_viable = true;

    static_assert(is_viable, "rcu wrapper does not support the chosen flags");

    using table_type = rcu_wrapper<hash_fct_type, allocator_type>;

    static std::string name()
    {
        #ifdef QSBR
        return "rcu_qsbr";
        #else
        return "rcu";
        #endif
    }
};

#endif // RCU_BASE_WRAPPER
