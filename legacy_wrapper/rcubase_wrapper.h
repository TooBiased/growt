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
#include "wrapper/stupid_iterator.h"


template<class Hasher>
class RCUBaseWrapper
{
private:
    typedef long long ll;

    struct mynode {
        ll key;
        ll value;

        struct cds_lfht_node node;	/* Chaining in hash table */
        struct rcu_head rcu_head;	/* For call_rcu() */
    };

    static inline size_t h(ll key)
    {
        return Hasher()(key);
    }

    static inline int match(struct cds_lfht_node *ht_node, const void *_key)
    {
        mynode *node = caa_container_of(ht_node, struct mynode, node);
        ll *key = (ll*) _key;
        return *key == node->key;
    }

    static inline void free_node(struct rcu_head *head)
    {
        struct mynode *node = caa_container_of(head, struct mynode, rcu_head);
        free(node);
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

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = StupidIterator<size_t, size_t>;
    using insert_return_type = std::pair<iterator, bool>;

    RCUBaseWrapper() = default;
    RCUBaseWrapper(size_t capacity_)
    {
        size_t initial_size = 1;
        while (initial_size < capacity_) initial_size <<= 1;
        hash_table = cds_lfht_new(initial_size, initial_size, 0,
            CDS_LFHT_AUTO_RESIZE | CDS_LFHT_ACCOUNTING, //0,
            NULL);
    }

    RCUBaseWrapper(const RCUBaseWrapper&) = delete;
    RCUBaseWrapper& operator=(const RCUBaseWrapper&) = delete;

    // I know this is really ugly, but it works for my benchmarks (all elements are forgotten)
    RCUBaseWrapper(RCUBaseWrapper&& rhs) : hash_table(rhs.hash_table)
    {
        rhs.hash_table = nullptr;
    }

    // I know this is even uglier, but it works for my benchmarks (all elements are forgotten)
    RCUBaseWrapper& operator=(RCUBaseWrapper&& rhs)
    {
        hash_table = rhs.hash_table;
        rhs.hash_table = nullptr;
        return *this;
    }

    ~RCUBaseWrapper() { /* SHOULD DO SOMETHING */ }


    using Handle = RCUBaseWrapper&;
    Handle getHandle()
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
        ll key_copy = k;
        cds_lfht_iter iter;
        mynode* node;
        ll value = 0;
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
        mynode* node = (mynode*)malloc(sizeof(*node));
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

#endif // RCU_BASE_WRAPPER
