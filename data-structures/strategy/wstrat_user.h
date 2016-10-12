/*******************************************************************************
 * data-structures/strategy/wstrat_user.h
 * 
 * see below
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef WSTRAT_USER_H
#define WSTRAT_USER_H

#include <atomic>

/*******************************************************************************
 *
 * This is a worker strategy for our growtable.
 *
 * Every worker strategy has to implement the following
 *  - subclass: global_data_t      (is stored at the growtable object)
 *  - subclass: local_data_t       (is stored at each handle)
 *     - init(...)
 *     - deinit()
 *     - execute_migration(...)
 *
 * This specific strategy uses user-threads for the migration.
 * Whenever the table is growing, all new operations help migrating the
 * old table, before the operation is executed. This is a very simple
 * technique therefore, nothing has to be saved/initialized
 *
 ******************************************************************************/

namespace growt {

template <class Parent>
class WStratUser
{
public:

    // The enslavement strategy, does not actually need any global data
    class global_data_t
    {
    public:
        global_data_t() {}
        global_data_t(const global_data_t& source) = delete;
        global_data_t& operator=(const global_data_t& source) = delete;
        ~global_data_t() = default;
    };


    // No initialization or deinitialization needed.
    class local_data_t
    {
    public:
        local_data_t(Parent &parent) : parent(parent) { }
        local_data_t(const local_data_t& source) = delete;
        local_data_t& operator=(const local_data_t& source) = delete;
        local_data_t(local_data_t&& source) = default;
        local_data_t& operator=(local_data_t&& source) = default;
        ~local_data_t() = default;

        Parent &parent;

        template<class EStrat>
        inline void init(EStrat&) { }
        inline void deinit() {}

        template<class ESLocal>
        inline void execute_migration(ESLocal &estrat, uint)
        {
            estrat.migrate();
        }
    };
};

}

#endif // WSTRAT_USER_H
