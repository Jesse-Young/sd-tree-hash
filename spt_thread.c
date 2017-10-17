/*
This file is provided under a dual BSD/GPLv2 license. When using or
redistributing this file, you may do so under either license.

GPL LICENSE SUMMARY
Copyright(c) 2016 Baibantech Corporation.
This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

BSD LICENSE
Copyright(c) 2016 Baibantech Corporation.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ONE(Object Non-duplicate Engine) thread management

Contact Information:
info-linux <info@baibantech.com.cn>
*/

#include "spt_dep.h"
#include "spt_thread.h"

__thread u32 g_thrd_id;
__thread int g_thrd_errno;

spt_thrd_t *g_thrd_h;
u64 thrd_cycle_rec[49] = {0};
u64 thrd_cycle_max[49] = {0};
/**
 * spt_thread_init - init thread head structure
 * @thread_num: how many thread will  parallel process 
 *
 * thread head structure record the thread black_white map,
 * online map and thread tick
 */
spt_thrd_t *spt_thread_init(int thread_num)
{
    g_thrd_h = spt_malloc(sizeof(spt_thrd_t));
    if(g_thrd_h == NULL)
    {
        spt_debug("OOM\r\n");
        return NULL;
    }
    g_thrd_h->thrd_total= thread_num;
    g_thrd_h->black_white_map = SPT_BWMAP_ALL_ONLINE;
    g_thrd_h->online_map = 0;
    g_thrd_h->tick = 0;
   
    return g_thrd_h;
}

void spt_atomic64_set_bit(int nr, atomic64_t *v)
{
    u64 tmp,old_val, new_val;

    tmp = 1ull<<nr;
    do
    {
        old_val = atomic64_read(v);
        new_val = tmp|old_val;

    }while(old_val != atomic64_cmpxchg(v, old_val, new_val));

    return;
}
u64 spt_atomic64_clear_bit_return(int nr, atomic64_t *v)
{
    u64 tmp,old_val, new_val;

    tmp = ~(1ull<<nr);
    do
    {
        old_val = atomic64_read(v);
        new_val = tmp&old_val;

    }while(old_val != atomic64_cmpxchg(v, old_val, new_val));

    return new_val;
}

/**
 * spt_thread_start - when work thread starts working, call it first
 * @thread: work thread's thread id
 *
 * thread start, set the corresponding bit in online map
 */
int spt_thread_start(int thread)
{
    thrd_cycle_rec[thread] = rdtsc();
    spt_atomic64_set_bit(thread, (atomic64_t *)&g_thrd_h->online_map);
    smp_mb();
    return 0;
}
/**
 * spt_thread_start - when work thread ends working, call it at the end
 * @thread: work thread's thread id
 *
 * thread exit, clear the corresponding bit in online map
 */
void spt_thread_exit(int thread)
{
    u64 olmap, bwmap, new_val;
    u64 cycle;

    smp_mb();
    olmap = spt_atomic64_clear_bit_return(thread,(atomic64_t *)&g_thrd_h->online_map);
    do
    {
        bwmap = atomic64_read((atomic64_t *)&g_thrd_h->black_white_map);
        if(bwmap == 0)
            return;
        new_val = bwmap & olmap;
    }while(bwmap != atomic64_cmpxchg((atomic64_t *)&g_thrd_h->black_white_map, bwmap, new_val));
    /*
     * black_white map equal 0 means from the last tick, all threads that have been entered
     * exited at least one time.
     * when black_white map equal 0, update tick
     */
    if(new_val == 0)
    {
        atomic_add(1, (atomic_t *)&g_thrd_h->tick);
        if(0 != atomic64_cmpxchg((atomic64_t *)&g_thrd_h->black_white_map, 
                                    0, SPT_BWMAP_ALL_ONLINE))
        {
            spt_debug("@@@@@@@@@@@@@@@@@@Err\r\n");
            spt_assert(0);
        }
    }
    cycle = rdtsc()-thrd_cycle_rec[thread];
    if(cycle > thrd_cycle_max[thread])
        thrd_cycle_max[thread] = cycle;
    return;
}

void spt_thread_wait(int n, int thread)
{
    u32 tick;
    u64 start, end;
    start = rdtsc();
    tick = atomic_read((atomic_t *)&g_thrd_h->tick);
    while(atomic_read((atomic_t *)&g_thrd_h->tick) - tick < n)
    {
        spt_thread_start(thread);
        spt_thread_exit(thread);
    }
    end = rdtsc()-start;
    if( end > 1000000000)
    {
        spt_print("spt_thread_wait too long, wait cycle:%llu\r\n", end);
        spt_print("blackmap:%llu\tonlinemap:%llu\r\n",g_thrd_h->black_white_map, g_thrd_h->online_map);
    }
    return;
}

void spt_thread_map_print(void)
{
    spt_print("blackmap:%llu\tonlinemap:%llu\r\n",g_thrd_h->black_white_map, g_thrd_h->online_map);
}

void spt_thread_cycle_print(void)
{
    int i;
    spt_print("========thread cycle print=========\r\n");
    for(i=0;i<49;i++)
    {
        spt_print("thread[%d]\tcycle max:%llu\r\n", i, thrd_cycle_max[i]);
    }
    spt_print("===================================\r\n");
}

