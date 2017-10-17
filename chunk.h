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

sd-tree cluster vector and data block management

Contact Information:
info-linux <info@baibantech.com.cn>
*/

#ifndef _SPLITTER_CLUSTER_H
#define _SPLITTER_CLUSTER_H

#include "spt_dep.h"
#include "spt_thread.h"
#define SPT_VEC_RIGHT 0
#define SPT_VEC_DATA 1
#define SPT_VEC_SIGNPOST 2
#define SPT_VEC_SYS_FLAG_DATA 1


#define SPT_VEC_VALID 0
#define SPT_VEC_INVALID 1
#define SPT_VEC_RAW 2


#define SPT_VEC_SIGNPOST_BIT 15

#define SPT_VEC_SIGNPOST_MASK ((1ul<<SPT_VEC_SIGNPOST_BIT)-1)

#if 0
#define SPT_VEC_POS_INDEX_BIT 8
#define SPT_VEC_POS_INDEX_MASK 0x3F00
#define SPT_VEC_POS_MULTI_MASK 0xFF

#define spt_get_pos_index(x) ((x&SPT_VEC_POS_INDEX_MASK)>>SPT_VEC_POS_INDEX_BIT)
#define spt_get_pos_multi(x) (x&SPT_VEC_POS_MULTI_MASK)
#define spt_set_pos(pos,index,multi) (pos = (index << SPT_VEC_POS_INDEX_BIT)|(multi))


#define POW256(x) (1<<(8*x))
#endif

#define spt_set_vec_invalid(x) (x.status = SPT_VEC_INVALID)
#define spt_set_right_flag(x) (x.type = SPT_VEC_RIGHT)
#define spt_set_data_flag(x) (x.type = SPT_VEC_DATA)

#define SPT_PTR_MASK (0x00000000fffffffful)
#define SPT_PTR_VEC (0ul)
#define SPT_PTR_DATA (1ul)

#define SPT_BUF_TICK_BITS 18
#define SPT_BUF_TICK_MASK ((1<<SPT_BUF_TICK_BITS)-1)

#define SPT_PER_THRD_RSV_CNT 5
#define SPT_BUF_VEC_WATERMARK 200
#define SPT_BUF_DATA_WATERMARK 100

#define spt_data_free_flag(x) ((x)->rsv&0x1)
#define spt_set_data_free_flag(x,y) ((x)->rsv |= y)
#define spt_set_data_not_free(x) ((x)->rsv &= 0xfffe)


#define SPT_SORT_ARRAY_SIZE (4096*8)
#define SPT_DVD_CNT_PER_TIME (100)
#define SPT_DVD_THRESHOLD_VA (1000000)
#define SPT_DVD_MOVE_TIMES (SPT_DVD_THRESHOLD_VA/(2*SPT_DVD_CNT_PER_TIME))
#define SPT_DATA_HIGH_WATER_MARK (1600000)

typedef char*(*spt_cb_get_key)(char *);
typedef void (*spt_cb_free)(char *);
typedef void (*spt_cb_end_key)(char *);
typedef char*(*spt_cb_construct)(char *);



typedef struct
{
    int idx;
    int size;
    int cnt;
    char *array[0];//pdh->pdata
}spt_sort_info;

typedef struct
{
//    unsigned long long flag:1;
    unsigned long long tick:18;
    unsigned long long id:23;
    unsigned long long next:23;
}spt_buf_list;

typedef struct block_head
{
    unsigned int magic;
    unsigned int next;
}block_head_t;

typedef struct db_head_t
{
    unsigned int magic;
    unsigned int next;
}db_head_t;

typedef struct spt_data_hd
{
    volatile int ref;
    u16 size;
    u16 rsv;
    char *pdata;
}spt_dh;


typedef struct spt_vec_t
{
    union
    {
        volatile unsigned long long val;
        struct 
        {
            volatile unsigned long long status:      2;
            volatile unsigned long long type:       1;
            volatile unsigned long long pos:        15;
            volatile unsigned long long down:       23;
            volatile unsigned long long rd:         23;    
        };
        struct 
        {
            volatile unsigned long long dummy_flag:         3;
            volatile unsigned long long ext_sys_flg:        6;
            volatile unsigned long long ext_usr_flg:        6;
            volatile unsigned long long idx:                24;
            volatile long long dummy_rd:           23;
        };
    };
}spt_vec;

typedef struct cluster_head
{
    struct list_head c_list;
    int vec_head;
    volatile unsigned int vec_free_head;
    volatile unsigned int dblk_free_head;
    volatile unsigned int blk_free_head;

    spt_vec *pstart;
    u64 startbit;
    u64 endbit;
    int is_bottom;
    volatile unsigned int data_total;

    unsigned int pg_num_max;
    unsigned int pg_num_total;
    unsigned int pg_cursor;
    unsigned int blk_per_pg_bits;
    unsigned int pg_ptr_bits;
    unsigned int blk_per_pg;
    unsigned int db_per_blk;
    unsigned int vec_per_blk;    
    
    unsigned int free_blk_cnt;
    unsigned int free_vec_cnt;
    unsigned int free_dblk_cnt;
    unsigned int used_vec_cnt;
    unsigned int used_dblk_cnt;
    unsigned int buf_db_cnt;
    unsigned int buf_vec_cnt;
    unsigned int thrd_total;
    
    spt_thrd_data *thrd_data;

    int status;
	int ins_mask;
    unsigned int debug;
    spt_cb_get_key get_key;
    spt_cb_get_key get_key_in_tree;
    //void (*freedata)(char *p, u8 flag);
    spt_cb_free freedata;
    spt_cb_end_key get_key_end;
    spt_cb_end_key get_key_in_tree_end;
    spt_cb_construct construct_data;
    volatile char *pglist[0];
}cluster_head_t;

typedef struct
{
    cluster_head_t *pdst_clst;
    cluster_head_t *puclst;
    int divided_times;
    int down_is_bottom;
    char **up_vb_arr;
    char **down_vb_arr;
}spt_divided_info;


typedef struct dh_extent_t
{
    unsigned int hang_vec;
    cluster_head_t *plower_clst;
    char *data;
}spt_dh_ext;


typedef struct vec_head
{
    unsigned int magic;
    unsigned int next;
}vec_head_t;

typedef struct vec_cmpret
{
    u64 smallfs;    /* small data firt set bit from the diff bit */
    u64 pos;        /* which bit is different */
    u32 finish;
}vec_cmpret_t;

typedef struct spt_query_info
{
    spt_vec *pstart_vec;        /* from which vector to start querying */
    u64 signpost;               /* not used now */
    char *data;                 /* data to be queried */
    u64 endbit;                 /* data end bit */
    u32 startid;                /* start vector id */
    u8 op;                      /* delete/find/insert */
    u8 data_type;               /* not used now */   
    u8 free_flag;               /* after deleting the data, whether it need to free by tree */
    char cmp_result;            /* return value,1 means >;0 means equal; -1 means < */
    u32 db_id;                  /* return value,the last compared data, when find return */
    u32 ref_cnt;                /* return value, the duplicate count*/
    int multiple;               /* insert/delete data's count*/
    u32 vec_id;                 /* return value,the last compared vector, when find return;the hang vec,when insert return */
    spt_cb_get_key get_key;     /* if NULL, use the default callback function*/
    spt_cb_end_key get_key_end; /* if NULL, use the default callback function*/
}query_info_t;


typedef struct spt_insert_info
{
    spt_vec *pkey_vec;
    u64 key_val;
    u64 signpost;
    u64 startbit;
    u64 fs;
    u64 cmp_pos;
    u64 endbit;         /* not include */
    u32 dataid;
    int ref_cnt;
    u32 key_id;
    u32 hang_vec;
    /* for debug */
    char *pcur_data;
    vec_cmpret_t cmpres;
}insert_info_t;

typedef struct spt_stack_st
{
    void **p_top;
    void **p_bottom; 
    int stack_size;
}spt_stack;

typedef  struct spt_vec_full_t
{
    int down;
    int right;
    int data;
    long long pos;
}spt_vec_f;

typedef struct spt_traversal_info_st
{
    spt_vec_f vec_f;
    long long signpost;
}travl_info;
#if 0
typedef struct spt_xy_st
{
    long long x;
    long long y;
}spt_xy;

typedef struct spt_outer_travl_info_st
{
    int pre;
    int cur;
    int data;
    spt_xy xy_pre;
    struct spt_outer_travl_info_st *next;
}travl_outer_info;

typedef struct spt_travl_q_st
{
    travl_outer_info *head;
    travl_outer_info *tail;
}travl_q;

typedef struct spt_test_file_head
{
    int data_size;
    int data_num;
    char rsv[24];
}test_file_h;
#define spt_get_hash_ind_index(x, used_bit,total_bit) ( \
    (x & ((1<<SPT_HASH_TBL_IND_BITS)-1)) \
    | ((1<<(total_bit-used_bit))-1) \
    )
#endif
#define SPT_HASH_CODE_RES_BITS 3
#define SPT_HASH_TBL_NDIR_BITS  14
#define SPT_HASH_TBL_IND_BITS  15

#define SPT_HASH_TBL_NDIR_OFFSET (SPT_HASH_CODE_RES_BITS+SPT_HASH_TBL_IND_BITS)
#define SPT_HASH_TBL_IND_OFFSET SPT_HASH_CODE_RES_BITS

#define SPT_HASH_TBL_NDIR_LABELS (1 << SPT_HASH_TBL_NDIR_BITS)
#define SPT_HASH_TBL_IND_UNITS (1 << SPT_HASH_TBL_IND_BITS)

#define spt_get_hash_ind_index(x, used_bit,total_bit) ( \
    (x | ((1<<(total_bit-used_bit))-1)) \
       & ((1<<SPT_HASH_TBL_IND_BITS)-1) \
    )

#define spt_get_hash_ndir_index(x) ( \
    (u32)x >> SPT_HASH_TBL_IND_BITS  \
    )
    
typedef struct 
{
    cluster_head_t *pclst;
    int hang_vec;
}spt_hash_tag;

typedef struct
{
    int used_bit;
    int total_bit;
    spt_hash_tag **pptbl;
}spt_hash_info;

#define INITIAL_TAG 0
#define EXPAND_TAG  1

//#define DBLK_BITS 3
#define DATA_SIZE g_data_size
#define RSV_SIZE 2
#define DBLK_SIZE (sizeof(spt_dh))
#define VBLK_BITS 3
#define VBLK_SIZE (1<<VBLK_BITS)
#define DATA_BIT_MAX (DATA_SIZE*8)

//#define vec_id_2_ptr(pchk, id)    ((char *)pchk+id*VBLK_SIZE);

unsigned int vec_alloc(cluster_head_t *pclst, spt_vec **vec);
void vec_free(cluster_head_t *pcluster, int id);
void vec_list_free(cluster_head_t *pcluster, int id);
void db_free(cluster_head_t *pcluster, int id);

int spt_get_errno(void);
extern cluster_head_t *pgclst;
extern spt_hash_info *pghash;

//DECLARE_PER_CPU(u32,local_thrd_id);
//DECLARE_PER_CPU(int,local_thrd_errno);
//DECLARE_PER_CPU(int,process_enter_check);
//#define g_thrd_id     per_cpu(local_thrd_id,smp_processor_id())
//#define g_thrd_errno  per_cpu(local_thrd_errno,smp_processor_id())



#define CLST_PG_NUM_MAX ((1<<14) ) /*cluster max = 64m*/
//#define CHUNK_SIZE (1<<16)
#define PG_BITS 12
#define PG_SIZE (1<<PG_BITS)
#define BLK_BITS 5
#define BLK_SIZE (1<<BLK_BITS)

#define CLST_NDIR_PGS 320        //how much

#define CLST_IND_PG (CLST_NDIR_PGS)    //index
#define CLST_DIND_PG (CLST_IND_PG+1)    //index
#define CLST_N_PGS (CLST_DIND_PG+1)//pclst->pglist[] max


//#define CLST_TIND_PGS        (CLST_DIND_PGS+1)


extern char* blk_id_2_ptr(cluster_head_t *pclst, unsigned int id);
extern char* db_id_2_ptr(cluster_head_t *pclst, unsigned int id);
extern char* vec_id_2_ptr(cluster_head_t *pclst, unsigned int id);

#define SPT_NULL 0x7fffff
#define SPT_INVALID 0x7ffffe

#define SPT_DIR_START 0
#define SPT_RIGHT 1
#define SPT_DOWN 2

#define SPT_OP_FIND 1
#define SPT_OP_DELETE 2
#define SPT_OP_INSERT 3


#define CLT_FULL 3
#define CLT_NOMEM 2
#define CLT_ERR 1
#define SPT_OK 0
#define SPT_ERR -1
#define SPT_NOMEM -2
#define SPT_WAIT_AMT -3
#define SPT_DO_AGAIN -4
#define SPT_MASKED -5
#define SPT_NOT_FOUND 1

unsigned int db_alloc(cluster_head_t *pclst, spt_dh **db);
cluster_head_t * cluster_init(int is_bottom, 
                              u64 startbit, 
                              u64 endbit, 
                              int thread_num,
                              spt_cb_get_key pf,
                              spt_cb_end_key pf2,
                              spt_cb_free pf_free,
                              spt_cb_construct pf_con);
int vec_free_to_buf(cluster_head_t *pclst, int id, int thread_id);
int db_free_to_buf(cluster_head_t *pcluster, int id, int thread_id);
unsigned int vec_alloc_combo(cluster_head_t *pclst, int thread_id, spt_vec **vec);
unsigned int data_alloc_combo(cluster_head_t *pclst, int thread_id, spt_dh **db);
void vec_free_to_buf_simple(cluster_head_t *pclst, int id, int thread_id);
void db_free_to_buf_simple(cluster_head_t *pclst, int id, int thread_id);
int fill_in_rsv_list(cluster_head_t *pclst, int nr, int thread_id);
int rsv_list_fill_cnt(cluster_head_t *pclst, int thread_id);
int fill_in_rsv_list_simple(cluster_head_t *pclst, int nr, int thread_id);
void cluster_destroy(cluster_head_t *pclst);
void free_data(char *p);
void default_end_get_key(char *p);
void vec_buf_free(cluster_head_t *pclst, int thread_id);
void db_buf_free(cluster_head_t *pclst, int thread_id);
spt_hash_info* spt_hash_tbl_init();
int spt_insert_initial_tag(cluster_head_t *pclst);

void debug_data_print(char *pdata);
extern int g_data_size;

char* insert_data(cluster_head_t *pclst,char* pdata);
int delete_data(cluster_head_t *pclst,char* pdata);
void set_data_size(int size);

cluster_head_t *spt_cluster_init(u64 startbit, 
                              u64 endbit, 
                              int thread_num,
                              spt_cb_get_key pf,
                              spt_cb_end_key pf2,
                              spt_cb_free pf_free,
                              spt_cb_construct pf_con);


spt_thrd_t *spt_thread_init(int thread_num);
void spt_set_thrd_id(int val);

int spt_thread_start(int thread);
void spt_thread_exit(int thread);
int spt_divided_scan(cluster_head_t *pclst);
#endif

