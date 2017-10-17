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

ONE(Object Non-duplicate Engine)  adaptation layer 

Contact Information:
info-linux <info@baibantech.com.cn>
*/

#ifndef _SPT_DEP_H
#define _SPT_DEP_H
#include <assert.h>
#include <atomic_user.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "sdtree_perf_stat.h"

#define spt_assert(expr) \
            if(!(expr)) \
                assert(0);

#define spt_print printf

#define spt_debug(f, a...)	{ \
					spt_print ("LFORD DEBUG (%s, %d): %s:", \
						__FILE__, __LINE__, __func__); \
				  	spt_print (f, ## a); \
					}


void *spt_malloc(unsigned long size);

void spt_free(void *ptr);

void *spt_vmalloc(unsigned long size);

void spt_vfree(void *ptr);

char* spt_alloc_zero_page(void);

void spt_free_page(void *page);

void *spt_realloc(void *mem_address, unsigned long newsize);

void spt_schedule(void);


#define spt_preempt_disable()  
#define spt_preempt_enable()  


extern unsigned int sd_thrd_errno[128];

extern __thread u32 g_thrd_id;
extern __thread int g_thrd_errno;

#endif

