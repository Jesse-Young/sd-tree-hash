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

#ifndef _SPT_THREAD_H
#define _SPT_THREAD_H

#define SPT_BWMAP_ALL_ONLINE 0xfffffffffffffffful

typedef struct
{
    unsigned int thrd_id;
    unsigned int vec_cnt;
    unsigned int vec_list_cnt;
    unsigned int data_cnt;
    unsigned int data_list_cnt;
    unsigned int vec_free_in;
    unsigned int vec_alloc_out;
    unsigned int data_free_in;
    unsigned int data_alloc_out;
    unsigned int rsv_cnt;
    unsigned int rsv_list;
}spt_thrd_data;

typedef struct 
{
    unsigned int thrd_total;
    volatile unsigned int tick;
    volatile unsigned long long black_white_map;
    volatile unsigned long long online_map;
    spt_thrd_data thrd_data[0];
}spt_thrd_t;


extern spt_thrd_t *g_thrd_h;

spt_thrd_t *spt_thread_init(int thread_num);

int spt_thread_start(int thread);

void spt_thread_exit(int thread);

void spt_thread_wait(int n, int thread);

#endif
