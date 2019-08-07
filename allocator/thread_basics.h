/*******************************************************************************
 * utils/test_coordination.h
 *
 * Some very basic thread functionalities (thread pinning and priorities)
 * using pthread
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef THREAD_BASICS_H
#define THREAD_BASICS_H

#include <pthread.h>

void pin_to_core(size_t core)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void set_thread_priority(uint pri)
{
    struct sched_param param;
    param.sched_priority = sched_get_priority_min(pri);

    pthread_setschedparam(pthread_self(), SCHED_RR, &param);
}

#endif // THREAD_BASICS_H
