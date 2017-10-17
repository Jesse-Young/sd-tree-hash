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

sd-tree vector and data block management

Contact Information:
info-linux <info@baibantech.com.cn>
*/

#include "chunk.h"

void vec_buf_free(cluster_head_t *pclst, int thread_id);
void db_buf_free(cluster_head_t *pclst, int thread_id);
int fill_in_rsv_list_simple(cluster_head_t *pclst, int nr, int thread_id);
int g_data_size;
/**
 * blk_id_2_ptr - called by db_id_2_ptr & vec_id_2_ptr
 * @pclst: pointer of sd tree cluster head
 * @id: block id in this cluster
 * return the address of the block
 * a block consist of data block or vector
 */
char* blk_id_2_ptr(cluster_head_t *pclst, unsigned int id)
{
    int ptrs_bit = pclst->pg_ptr_bits;
    int ptrs = (1 << ptrs_bit);
    u64 direct_pgs = CLST_NDIR_PGS;
    u64 indirect_pgs = 1<<ptrs_bit;
    u64 double_pgs = 1<<(ptrs_bit*2);
    int pg_id = id >> pclst->blk_per_pg_bits;
    int offset;
    char *page, **indir_page, ***dindir_page;

    if(pg_id < direct_pgs)
    {
        page = (char *)pclst->pglist[pg_id];
        while(page == 0)
        {
            smp_mb();
            page = (char *)atomic64_read((atomic64_t *)&pclst->pglist[pg_id]);
        }
    }
    else if((pg_id -= direct_pgs) < indirect_pgs)
    {
        indir_page = (char **)pclst->pglist[CLST_IND_PG];
        while(indir_page == NULL)
        {
            smp_mb();
            indir_page = (char **)atomic64_read((atomic64_t *)&pclst->pglist[CLST_IND_PG]);
        }
        offset = pg_id;
        page = indir_page[offset];
        while(page == 0)
        {
            smp_mb();
            page = (char *)atomic64_read((atomic64_t *)&indir_page[offset]);
        }        
    }
    else if((pg_id -= indirect_pgs) < double_pgs)
    {
        dindir_page = (char ***)pclst->pglist[CLST_DIND_PG];
        while(dindir_page == NULL)
        {
            smp_mb();
            dindir_page = (char ***)atomic64_read((atomic64_t *)&pclst->pglist[CLST_DIND_PG]);
        }
        offset = pg_id >> ptrs_bit;
        indir_page = dindir_page[offset];
        while(indir_page == 0)
        {
            smp_mb();
            indir_page = (char **)atomic64_read((atomic64_t *)&dindir_page[offset]);
        }         
        offset = pg_id & (ptrs-1);
        page = indir_page[offset];
        while(page == 0)
        {
            smp_mb();
            page = (char *)atomic64_read((atomic64_t *)&indir_page[offset]);
        }
    }
    else
    {
        spt_debug("warning: id is too big\r\n");
        while(1);
        return 0;
    }
    
    offset = id & (pclst->blk_per_pg-1);
    return    page + (offset << BLK_BITS); 
    
}
/* data block ptr = block ptr + db offset in the block */
char* db_id_2_ptr(cluster_head_t *pclst, unsigned int id)
{
    return blk_id_2_ptr(pclst, id/pclst->db_per_blk) + id%pclst->db_per_blk * DBLK_SIZE;
    
}
/* vector ptr = block ptr + vector offset in the block */
u64 g_vec=0;
char* vec_id_2_ptr(cluster_head_t *pclst, unsigned int id)
{
	g_vec++;
    return blk_id_2_ptr(pclst, id/pclst->vec_per_blk) + id%pclst->vec_per_blk * VBLK_SIZE;
    
}

void set_data_size(int size)
{
    g_data_size = size;
}
void free_data(char *p)
{
    spt_free(p);
}
void default_end_get_key(char *p)
{
    return;
}
/**
 * upper_get_key - call back function called by up-level cluster to access the data 
 * @pdata: pointer of data in the cluster leaf node
 * 
 * return the address of the actual data address
 */
char *upper_get_key(char *pdata)
{
    spt_dh_ext *ext_head;

    ext_head = (spt_dh_ext *)pdata;
    return ext_head->data;
}
/**
 * cluster_add_page - cluster alloc one page ,and cut it into block
 * @pclst: pointer of sd tree cluster head
 * 
 *a cluster use a page as the smallest unit to alloc for memory
 * a cluster maximum memory overhead is 64M
 */
int cluster_add_page(cluster_head_t *pclst)
{
    char *page, **indir_page, ***dindir_page;
    u32 old_head;
    int i, total, id, pg_id, offset, offset2;
    block_head_t *blk;
    int ptrs_bit = pclst->pg_ptr_bits;
    int ptrs = (1 << ptrs_bit);
    u64 direct_pgs = CLST_NDIR_PGS;
    u64 indirect_pgs = 1<<ptrs_bit;
    u64 double_pgs = 1<<(ptrs_bit*2);
    

    pg_id = atomic_add_return(1, (atomic_t *)&pclst->pg_cursor);
    if(pclst->pg_num_max <= pg_id)
    {
        atomic_sub(1, (atomic_t *)&pclst->pg_cursor);
        return CLT_FULL;
    }

    page = spt_alloc_zero_page();
    if(page == NULL)
        return CLT_NOMEM;
  
    pg_id--;

    id = pg_id;
    if(id < direct_pgs)
    {
        pclst->pglist[id] = page;
        smp_mb();
    }
    else if((id -= direct_pgs) < indirect_pgs)
    {
        offset = id;
        if(offset == 0)
        {
            indir_page = (char **)spt_alloc_zero_page();
            if(indir_page == NULL)
            {
                return CLT_NOMEM;
            }
            pclst->pglist[CLST_IND_PG] = (char *)indir_page;
        }
        else
        {
            while(atomic64_read((atomic64_t *)&pclst->pglist[CLST_IND_PG]) == 0)
            {
                smp_mb();
            }                
        }
        indir_page = (char **)pclst->pglist[CLST_IND_PG];
        indir_page[offset] = page;
        smp_mb();
    }
    else if((id -= indirect_pgs) < double_pgs)
    {
        offset = id >> ptrs_bit;
        offset2 = id & (ptrs-1);
        if(id == 0)
        {
            dindir_page = (char ***)spt_alloc_zero_page();
            if(dindir_page == NULL)
            {
                return CLT_NOMEM;
            }
            pclst->pglist[CLST_DIND_PG] = (char *)dindir_page;
        }
        else
        {
            while(atomic64_read((atomic64_t *)&pclst->pglist[CLST_DIND_PG]) == 0)
            {
                smp_mb();
            }

        }
        dindir_page = (char ***)pclst->pglist[CLST_DIND_PG];
        
        if(offset2 == 0)
        {
            indir_page = (char **)spt_alloc_zero_page();
            if(indir_page == NULL)
            {
                return CLT_NOMEM;
            }
            else
            {

            }
            dindir_page[offset] = indir_page;
        }
        else
        {
            while(atomic64_read((atomic64_t *)&dindir_page[offset]) == 0)
            {
                smp_mb();
            }
        }
        indir_page = dindir_page[offset];        
        indir_page[offset2] = page;
        smp_mb();
    }
    else
    {
        spt_debug("warning: id is too big");
        return CLT_ERR;
    }
    
    id = (pg_id<< pclst->blk_per_pg_bits);
//    pclst->pg_cursor++;
    total = pclst->blk_per_pg;
    blk = (block_head_t *)page;

    for(i=1; i<total; i++)
    {
        blk->magic = 0xdeadbeef;
        blk->next = id + i;
        blk = (block_head_t *)(page + (i << BLK_BITS));
    }
    blk->magic = 0xdeadbeef;
    do
    {
        old_head = atomic_read((atomic_t *)&pclst->blk_free_head);
        blk->next = old_head;
//        pclst->blk_free_head = id;
    }while(old_head != atomic_cmpxchg((atomic_t *)&pclst->blk_free_head, old_head, id));

    atomic_add(total, (atomic_t *)&pclst->free_blk_cnt);
    return SPT_OK;
}

void cluster_destroy(cluster_head_t *pclst)
{
    spt_debug("\r\n");
}

cluster_head_t * cluster_init(int is_bottom, 
                              u64 startbit, 
                              u64 endbit, 
                              int thread_num,
                              spt_cb_get_key pf,
                              spt_cb_end_key pf2,
                              spt_cb_free pf_free,
                              spt_cb_construct pf_con)
{
    cluster_head_t *phead;
    int ptr_bits, i;
    u32 vec;
    spt_vec *pvec;

    phead = (cluster_head_t *)spt_alloc_zero_page();
    if(phead == NULL)
        return 0;

    memset(phead, 0, 4096);

    if(sizeof(char *) == 4)
    {
        ptr_bits = 2;
    }
    if(sizeof(char *) == 8)
    {
        ptr_bits = 3;
    }
    phead->pg_num_max = CLST_PG_NUM_MAX;
//    phead->pg_num_total = (sizeof(cluster_head_t)>>ptr_bits);
    phead->blk_per_pg_bits = PG_BITS - BLK_BITS;
//    phead->db_per_pg_bits= PG_BITS - DBLK_BITS;
//    phead->vec_per_pg_bits = PG_BITS - VBLK_BITS;
    phead->pg_ptr_bits= PG_BITS - ptr_bits;
    phead->blk_per_pg = PG_SIZE/BLK_SIZE;
    phead->db_per_blk= BLK_SIZE/DBLK_SIZE; 
    phead->vec_per_blk = BLK_SIZE/VBLK_SIZE;
    phead->vec_free_head = -1;
    phead->blk_free_head = -1;
    phead->dblk_free_head = -1;
    
    phead->debug = 1;
    phead->is_bottom = is_bottom;
    phead->startbit = startbit;
    phead->endbit = endbit;
    phead->thrd_total = thread_num;
    ///TODO:
    phead->freedata = pf_free;
    phead->construct_data = pf_con;
    if(is_bottom)
    {
        phead->get_key = pf;
        phead->get_key_in_tree = pf;
        phead->get_key_end = pf2;
        phead->get_key_in_tree_end = pf2;
    }
    else
    {
        phead->get_key = pf;
        phead->get_key_in_tree = upper_get_key;
        phead->get_key_end = pf2;
        phead->get_key_in_tree_end = default_end_get_key;
    }

    phead->thrd_data = (spt_thrd_data *)spt_malloc(sizeof(spt_thrd_data)*thread_num);
    if(phead->thrd_data == NULL)
    {
        cluster_destroy(phead);
        return NULL;
    }
    for(i=0;i<thread_num;i++)
    {
        phead->thrd_data[i].thrd_id = i;
        phead->thrd_data[i].vec_cnt = 0;
        phead->thrd_data[i].vec_list_cnt = 0;
        phead->thrd_data[i].data_cnt = 0;
        phead->thrd_data[i].data_list_cnt = 0;
        phead->thrd_data[i].vec_free_in = SPT_NULL;
        phead->thrd_data[i].vec_alloc_out = SPT_NULL;
        phead->thrd_data[i].data_free_in = SPT_NULL;
        phead->thrd_data[i].data_alloc_out = SPT_NULL;
        phead->thrd_data[i].rsv_cnt = 0;
        phead->thrd_data[i].rsv_list = SPT_NULL;
        fill_in_rsv_list_simple(phead, SPT_PER_THRD_RSV_CNT, i);
    }
    
    vec = vec_alloc(phead, &pvec);
    if(pvec == 0)
    {
        cluster_destroy(phead);
        return NULL;
    }
    pvec->val = 0;
    pvec->type = SPT_VEC_DATA;
    pvec->pos = startbit - 1;
    pvec->down = SPT_NULL;
    pvec->rd = SPT_NULL;
    phead->vec_head = vec;
    phead->pstart = pvec;

    return phead;
}
/**
 * blk_alloc - alloc block
 * @pcluster: pointer of sd tree cluster head
 * @blk: return value, store the addr of the block
 *
 * return block id
 */
int blk_alloc(cluster_head_t *pcluster, char **blk)
{
    u32 blk_id, new_head;
    block_head_t *pblk;
    int try_cnts = 0;

    do
    {
        while((blk_id = atomic_read((atomic_t *)&pcluster->blk_free_head)) == -1 )
        {
            if(SPT_OK != cluster_add_page(pcluster))
            {                
                if(try_cnts != 0)
                {
                    *blk = NULL;
                    return -1;
                }
                try_cnts++;
                continue;
            }
        }
        pblk = (block_head_t *)blk_id_2_ptr(pcluster,blk_id);
        new_head = pblk->next;
    }while(blk_id != atomic_cmpxchg((atomic_t *)&pcluster->blk_free_head, blk_id, new_head));
    atomic_sub(1, (atomic_t *)&pcluster->free_blk_cnt);
    *blk = (char *)pblk;

    return blk_id;    
}
/**
 * db_add_blk - alloc a block and cut it into data block
 * @pclst: pointer of sd tree cluster head
 */
int db_add_blk(cluster_head_t *pclst)
{
    char *pblk;
    int i, total;
    u32 id, blkid, old_head;
    db_head_t *db;

    blkid = blk_alloc(pclst, &pblk);
    if(pblk == NULL)
        return CLT_NOMEM;

    total = pclst->db_per_blk;
    id = (blkid*total);
    db = (db_head_t *)pblk;

    for(i=1; i<total; i++)
    {
        db->magic = 0xdeadbeef;
        db->next = id + i;
        db = (db_head_t *)(pblk + (i * DBLK_SIZE));
    }
    db->magic = 0xdeadbeef;

    do
    {
        old_head = atomic_read((atomic_t *)&pclst->dblk_free_head);
        db->next = old_head;
    }while(old_head != atomic_cmpxchg((atomic_t *)&pclst->dblk_free_head, old_head, id));

    atomic_add(total, (atomic_t *)&pclst->free_dblk_cnt);

    return SPT_OK;
}
/**
 * vec_add_blk - alloc a block and cut it into vectors
 * @pclst: pointer of sd tree cluster head
 */
int vec_add_blk(cluster_head_t *pclst)
{
    char *pblk;
    int i, total;
    u32 id, blkid, old_head;
    vec_head_t *vec;

    blkid = blk_alloc(pclst, &pblk);
    if(pblk == NULL)
        return CLT_NOMEM;

    total = pclst->vec_per_blk;
    id = (blkid*total);
    vec = (vec_head_t *)pblk;

    for(i=1; i<total; i++)
    {
        vec->magic = 0xdeadbeef;
        vec->next = id + i;
        vec = (vec_head_t *)(pblk + (i << VBLK_BITS));
    }

    vec->magic = 0xdeadbeef;

    do
    {
        old_head = atomic_read((atomic_t *)&pclst->vec_free_head);
        vec->next = old_head;
        smp_mb();
    }while(old_head != atomic_cmpxchg((atomic_t *)&pclst->vec_free_head, old_head, id));

    atomic_add(total, (atomic_t *)&pclst->free_vec_cnt);
    
    return SPT_OK;
}

/**
 * db_alloc - alloc data block from freelist
 * @pclst: pointer of sd tree cluster head
 * @db: return value, store the addr of the data block
 *
 * return data block id
 */
unsigned int db_alloc(cluster_head_t *pclst, spt_dh **db)
{
    u32 db_id;
    db_head_t *pdb;
    u32 try_cnts, new_head;
    try_cnts = 0;
    do
    {
        while((db_id = atomic_read((atomic_t *)&pclst->dblk_free_head)) == -1 )
        {
            if(SPT_OK != db_add_blk(pclst))
            {
                if(try_cnts != 0)
                {
                    *db = NULL;
                    return -1;
                }
                try_cnts++;
                continue;                
            }
        }
        pdb = (db_head_t *)db_id_2_ptr(pclst,db_id);
        smp_mb();
        new_head = pdb->next;
    }while(db_id != atomic_cmpxchg((atomic_t *)&pclst->dblk_free_head, db_id, new_head));
    atomic_sub(1, (atomic_t *)&pclst->free_dblk_cnt);
    atomic_add(1, (atomic_t *)&pclst->used_dblk_cnt);
    *db = (spt_dh *)pdb;
    (*db)->rsv = 0;
    (*db)->pdata = NULL;
    return db_id;
}
/**
 * db_free - free data block to freelist
 * @pcluster: pointer of sd tree cluster head
 * @id: data block id
 */
void db_free(cluster_head_t *pcluster, int id)
{
    db_head_t *pdb;
    u32 old_head;
    
    pdb = (db_head_t *)db_id_2_ptr(pcluster,id);
    do
    {
        old_head = atomic_read((atomic_t *)&pcluster->dblk_free_head);
        pdb->next = old_head;
        smp_mb();
    }while(old_head != atomic_cmpxchg((atomic_t *)&pcluster->dblk_free_head, old_head, id));
    
    atomic_add(1, (atomic_t *)&pcluster->free_dblk_cnt);
    atomic_sub(1, (atomic_t *)&pcluster->used_dblk_cnt);

    return ;
}

/**
 * vec_alloc - alloc vector from freelist
 * @pclst: pointer of sd tree cluster head
 * @vec: return value, store the addr of the vector
 *
 * return vector id
 */
unsigned int vec_alloc(cluster_head_t *pclst, spt_vec **vec)
{
    u32 vec_id;
    vec_head_t *pvec;
    u32 try_cnts, new_head;

    do
    {
        while((vec_id = atomic_read((atomic_t *)&pclst->vec_free_head)) == -1 )
        {
            if(SPT_OK != vec_add_blk(pclst))
            {
                try_cnts++;
                if(try_cnts != 0)
                {
                    *vec = NULL;
                    return -1;
                }
            }
        }
        pvec = (vec_head_t *)vec_id_2_ptr(pclst,vec_id);
        smp_mb();
        new_head = pvec->next;
    }while(vec_id != atomic_cmpxchg((atomic_t *)&pclst->vec_free_head, vec_id, new_head));
    atomic_sub(1, (atomic_t *)&pclst->free_vec_cnt);
    atomic_add(1, (atomic_t *)&pclst->used_vec_cnt);
    *vec = (spt_vec *)pvec;
    
    return vec_id;
}
/**
 * vec_free - free data block to freelist
 * @pcluster: pointer of sd tree cluster head
 * @id: vector id
 */
void vec_free(cluster_head_t *pcluster, int id)
{
    vec_head_t *pvec;
    u32 old_head;

    pvec = (vec_head_t *)vec_id_2_ptr(pcluster,id);
    do
    {
        old_head = atomic_read((atomic_t *)&pcluster->vec_free_head);
        pvec->next = old_head;
        smp_mb();
    }while(old_head != atomic_cmpxchg((atomic_t *)&pcluster->vec_free_head, old_head, id));
    
    atomic_add(1, (atomic_t *)&pcluster->free_vec_cnt);
    atomic_sub(1, (atomic_t *)&pcluster->used_vec_cnt);

    return ;
}
/**
 * db_alloc_from_buf - alloc data block from thread buffer list
 * @pclst: pointer of sd tree cluster head
 * @thread_id: thread id
 * @db: return value, store the addr of the data block
 *
 * return data block id
 */
unsigned int db_alloc_from_buf(cluster_head_t *pclst, int thread_id, spt_dh **db)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 list_vec_id, ret_id;
    unsigned int tick;
    spt_buf_list *pnode;
    
    pnode = 0;
    list_vec_id = pthrd_data->data_alloc_out;

    if(list_vec_id == SPT_NULL)
    {
        *db = NULL;
        return -1;
    }
    pnode = (spt_buf_list *)vec_id_2_ptr(pclst,list_vec_id);
    tick = atomic_add_return(0, (atomic_t *)&g_thrd_h->tick) & SPT_BUF_TICK_MASK;
    if(tick <  pnode->tick)
    {
        tick = tick | (1<<SPT_BUF_TICK_BITS);
    }
    if(tick-pnode->tick < 2)
    {
        *db = NULL;
        return -1;
    }
    ret_id = pnode->id;
    *db = (spt_dh *)db_id_2_ptr(pclst,ret_id);
    if((*db)->pdata != NULL)
    {
        if(spt_data_free_flag(*db))
        {
            pclst->freedata((*db)->pdata);
        }
    }
    (*db)->rsv = 0;
    pnode->id = SPT_NULL;
    if(pthrd_data->data_alloc_out == pthrd_data->data_free_in)
    {
        pthrd_data->data_alloc_out = pthrd_data->data_free_in = SPT_NULL;
    }
    else
    {
        pthrd_data->data_alloc_out = pnode->next;
    }
    pthrd_data->data_cnt--;
    pthrd_data->data_list_cnt--;
    vec_free(pclst, list_vec_id);
    pthrd_data->vec_cnt --;
    return ret_id;
} 
/**
 * vec_alloc_from_buf - alloc vector from thread buffer list
 * @pclst: pointer of sd tree cluster head
 * @thread_id: thread id
 * @vec: return value, store the addr of the vector
 *
 * return vector id
 */
unsigned int vec_alloc_from_buf(cluster_head_t *pclst, int thread_id, spt_vec **vec)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 list_vec_id, ret_id;
    u32 tick;
    spt_buf_list *pnode;
    
    pnode = 0;
    list_vec_id = pthrd_data->vec_alloc_out;

    if(list_vec_id == SPT_NULL)
    {
        *vec = NULL;
        return -1;
    }
    pnode = (spt_buf_list *)vec_id_2_ptr(pclst,list_vec_id);
    if(pnode->id == SPT_NULL)
    {
        if(pthrd_data->vec_alloc_out == pthrd_data->vec_free_in)
        {
            pthrd_data->vec_alloc_out = pthrd_data->vec_free_in = SPT_NULL;
        }
        else
        {
            pthrd_data->vec_alloc_out = pnode->next;
        }
        ret_id = list_vec_id;
        *vec = (spt_vec *)pnode;
        pthrd_data->vec_cnt--;
        pthrd_data->vec_list_cnt--;
        return ret_id;
    }

    tick = atomic_add_return(0, (atomic_t *)&g_thrd_h->tick) & SPT_BUF_TICK_MASK;
    if(tick <  pnode->tick)
    {
        tick = tick | (1<<SPT_BUF_TICK_BITS);
    }
    if(tick-pnode->tick < 2)
    {
        *vec = NULL;
        return -1;
    }
    ret_id = pnode->id;
    *vec = (spt_vec *)vec_id_2_ptr(pclst,ret_id);
    pnode->id = SPT_NULL;
    pthrd_data->vec_cnt--;
    return ret_id;
}
/**
 * in some scenes ,vector alloc must succeed.
 * every thread reserve several vector for data block and vector recycling.
 */
int rsv_list_fill_cnt(cluster_head_t *pclst, int thread_id)
{
    return SPT_PER_THRD_RSV_CNT - pclst->thrd_data[thread_id].rsv_cnt;
}
/**
 * alloc can not fail
 */
unsigned int vec_alloc_from_rsvlist(cluster_head_t *pclst, int thread_id, spt_vec **vec)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 vec_id;
    vec_head_t *pvec;

    vec_id = pthrd_data->rsv_list;
    pvec = (vec_head_t *)vec_id_2_ptr( pclst,vec_id);

    pthrd_data->rsv_list = pvec->next;
    pthrd_data->rsv_cnt--;
    *vec = (spt_vec *)pvec;
    return vec_id;
}
/* add vector into reserved list
 * if buffer list exceed water mark, free the vector&db that can be freed
 */
int fill_in_rsv_list(cluster_head_t *pclst, int nr, int thread_id)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];

    if(pthrd_data->vec_list_cnt > SPT_BUF_VEC_WATERMARK)
    {
        vec_buf_free(pclst, thread_id);
    }
    if(pthrd_data->data_list_cnt > SPT_BUF_DATA_WATERMARK)
    {
        db_buf_free(pclst, thread_id);
    }
    /* if buffer list still exceed water mark, return SPT_WAIT_AMT
     * when tick refresh twice, the buffer list can be freed
     */
    if(atomic_read((atomic_t *)&pthrd_data->vec_list_cnt) > SPT_BUF_VEC_WATERMARK
        || (atomic_read((atomic_t *)&pthrd_data->data_list_cnt) > SPT_BUF_DATA_WATERMARK))
    {
        fill_in_rsv_list_simple(pclst, nr, thread_id);
        pclst->status = SPT_WAIT_AMT;
        return SPT_WAIT_AMT;
    }
    else
    {
        return fill_in_rsv_list_simple(pclst, nr, thread_id);
    }
}

/* add vector into reserved list */
int fill_in_rsv_list_simple(cluster_head_t *pclst, int nr, int thread_id)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 vec_id;
    vec_head_t *pvec;

    while(nr>0)
    {
        vec_id = vec_alloc_from_buf(pclst, thread_id, (spt_vec **)&pvec);
        if(pvec == NULL)
        {
            vec_id = vec_alloc(pclst, (spt_vec **)&pvec);
            if(pvec == NULL)
                return SPT_NOMEM;
        }
        pvec->next = pthrd_data->rsv_list;
        pthrd_data->rsv_list = vec_id;
        pthrd_data->rsv_cnt++;
        nr--;
    }
    return SPT_OK;
}
/**
 * free the vector in the buffer list into free list
 * the vector can be freed only if it is freed to buffer list 2 tick ago
 */
void vec_buf_free(cluster_head_t *pclst, int thread_id)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 list_vec_id, tmp_id;
    u32 tick;
    spt_buf_list *pnode;

    tick = atomic_add_return(0, (atomic_t *)&g_thrd_h->tick) & SPT_BUF_TICK_MASK;
    list_vec_id = pthrd_data->vec_alloc_out;

    while(list_vec_id != pthrd_data->vec_free_in)
    {
        pnode = (spt_buf_list *)vec_id_2_ptr(pclst,list_vec_id);
        if(tick <  pnode->tick)
        {
            tick = tick | (1<<SPT_BUF_TICK_BITS);
        }
        if(tick - pnode->tick < 2)
        {
            pthrd_data->vec_alloc_out = list_vec_id;
            return;
        }
        if(pnode->id != SPT_NULL)
        {
            vec_free(pclst, pnode->id);
            pthrd_data->vec_cnt--;
        }
        tmp_id = list_vec_id;
        list_vec_id = pnode->next;
        vec_free(pclst, tmp_id);
        pthrd_data->vec_list_cnt--;
        pthrd_data->vec_cnt--;
    }
    pthrd_data->vec_alloc_out = list_vec_id;
    return;
}
/* free vector into buffer list
 * if buffer list exceed water mark, free the vector that can be freed
 * supplement vector into reserved list
 */
int vec_free_to_buf(cluster_head_t *pclst, int id, int thread_id)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 list_vec_id;
    u32 tick;
    spt_buf_list *pnode;
    spt_vec *pvec;

    pvec = (spt_vec *)vec_id_2_ptr(pclst, id);
    pvec->status = SPT_VEC_RAW;
    list_vec_id = vec_alloc_from_rsvlist(pclst, thread_id, (spt_vec **)&pnode);
    tick = atomic_add_return(0, (atomic_t *)&g_thrd_h->tick) & SPT_BUF_TICK_MASK;
    pnode->tick = tick;
    pnode->id = id;
    pnode->next = SPT_NULL;
    if(pthrd_data->vec_free_in == SPT_NULL)
    {
        pthrd_data->vec_free_in = pthrd_data->vec_alloc_out = list_vec_id;
    }
    else
    {
        pnode = (spt_buf_list *)vec_id_2_ptr(pclst, pthrd_data->vec_free_in);
        pnode->next = list_vec_id;
        pthrd_data->vec_free_in = list_vec_id;
    }
    pthrd_data->vec_list_cnt++;
    pthrd_data->vec_cnt += 2;

    if(pthrd_data->vec_list_cnt > SPT_BUF_VEC_WATERMARK)
    {
        vec_buf_free(pclst, thread_id);
    }
    if(atomic_read((atomic_t *)&pthrd_data->vec_list_cnt) > SPT_BUF_VEC_WATERMARK)
    {
        fill_in_rsv_list_simple(pclst, 1, thread_id);
        pclst->status = SPT_WAIT_AMT;
        return SPT_WAIT_AMT;
    }
    else
    {
        return fill_in_rsv_list_simple(pclst, 1, thread_id);
    }
}
/**
 * free the db in the buffer list into free list
 * the db can be freed only if it is freed to buffer list 2 tick ago
 */
void db_buf_free(cluster_head_t *pclst, int thread_id)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 list_vec_id, tmp_id;
    u32 tick;
    spt_buf_list *pnode;
    spt_dh *pdh;

    tick = atomic_add_return(0, (atomic_t *)&g_thrd_h->tick) & SPT_BUF_TICK_MASK;
    list_vec_id = pthrd_data->data_alloc_out;

    while(list_vec_id != pthrd_data->data_free_in)
    {
        pnode = (spt_buf_list *)vec_id_2_ptr(pclst,list_vec_id);
        if(tick <  pnode->tick)
        {
            tick = tick | (1<<SPT_BUF_TICK_BITS);
        }
        if(tick - pnode->tick < 2)
        {
            pthrd_data->data_alloc_out = list_vec_id;
            return;
        }
        pdh = (spt_dh *)db_id_2_ptr(pclst, pnode->id);
        if(pdh->pdata != NULL)
        {
            if(spt_data_free_flag(pdh))
            {
                pclst->freedata(pdh->pdata);
            }
        }
           // pclst->freedata(pdh->pdata, spt_data_free_flag(pdh));
        db_free(pclst, pnode->id);
        tmp_id = list_vec_id;
        list_vec_id = pnode->next;
        vec_free(pclst, tmp_id);
        pthrd_data->vec_cnt --;
        pthrd_data->data_list_cnt--;
        pthrd_data->data_cnt --;
    }
    pthrd_data->data_alloc_out = list_vec_id;
    return;
}
/* free vector into buffer list */
void vec_free_to_buf_simple(cluster_head_t *pclst, int id, int thread_id)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 list_vec_id;
    u32 tick;
    spt_buf_list *pnode;
    spt_vec *pvec;

    pvec = (spt_vec *)vec_id_2_ptr(pclst, id);
    pvec->status = SPT_VEC_RAW;
    list_vec_id = vec_alloc_from_rsvlist(pclst, thread_id, (spt_vec **)&pnode);
    tick = atomic_add_return(0, (atomic_t *)&g_thrd_h->tick) & SPT_BUF_TICK_MASK;
    pnode->tick = tick;
    pnode->id = id;
    pnode->next = SPT_NULL;
    if(pthrd_data->vec_free_in == SPT_NULL)
    {
        pthrd_data->vec_free_in = pthrd_data->vec_alloc_out = list_vec_id;
    }
    else
    {
        pnode = (spt_buf_list *)vec_id_2_ptr(pclst, pthrd_data->vec_free_in);
        pnode->next = list_vec_id;
        pthrd_data->vec_free_in = list_vec_id;
    }
    pthrd_data->vec_list_cnt++;
    pthrd_data->vec_cnt += 2;
    return;
}

/* free db into buffer list */
void db_free_to_buf_simple(cluster_head_t *pclst, int id, int thread_id)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 list_vec_id;
    u32 tick;
    spt_buf_list *pnode;

    list_vec_id = vec_alloc_from_rsvlist(pclst, thread_id, (spt_vec **)&pnode);
    tick = atomic_add_return(0, (atomic_t *)&g_thrd_h->tick) & SPT_BUF_TICK_MASK;
    pnode->tick = tick;
    pnode->id = id;
    pnode->next = SPT_NULL;
    if(pthrd_data->data_free_in == SPT_NULL)
    {
        pthrd_data->data_free_in = pthrd_data->data_alloc_out = list_vec_id;
    }
    else
    {
        pnode = (spt_buf_list *)vec_id_2_ptr(pclst, pthrd_data->data_free_in);
        pnode->next = list_vec_id;
        pthrd_data->data_free_in = list_vec_id;
    }
    pthrd_data->data_list_cnt++;
    pthrd_data->data_cnt ++;
    pthrd_data->vec_cnt ++;
    atomic_sub(1, (atomic_t *)&pclst->data_total);

    return;
}

/* free db into buffer list
 * if buffer list exceed water mark, free the db that can be freed
 * supplement vector into reserved list
 */
int db_free_to_buf(cluster_head_t *pclst, int id, int thread_id)
{
    spt_thrd_data *pthrd_data = &pclst->thrd_data[thread_id];
    u32 list_vec_id;
    u32 tick;
    spt_buf_list *pnode;

    list_vec_id = vec_alloc_from_rsvlist(pclst, thread_id, (spt_vec **)&pnode);
    tick = atomic_add_return(0, (atomic_t *)&g_thrd_h->tick) & SPT_BUF_TICK_MASK;
    pnode->tick = tick;
    pnode->id = id;
    pnode->next = SPT_NULL;
    if(pthrd_data->data_free_in == SPT_NULL)
    {
        pthrd_data->data_free_in = pthrd_data->data_alloc_out = list_vec_id;
    }
    else
    {
        pnode = (spt_buf_list *)vec_id_2_ptr(pclst, pthrd_data->data_free_in);
        pnode->next = list_vec_id;
        pthrd_data->data_free_in = list_vec_id;
    }
    pthrd_data->data_list_cnt++;
    pthrd_data->data_cnt ++;
    pthrd_data->vec_cnt ++;
    atomic_sub(1, (atomic_t *)&pclst->data_total);

    if(pthrd_data->data_list_cnt > SPT_BUF_DATA_WATERMARK)
    {
        db_buf_free(pclst, thread_id);
    }
    if(atomic_read((atomic_t *)&pthrd_data->data_list_cnt) > SPT_BUF_DATA_WATERMARK)
    {
        fill_in_rsv_list_simple(pclst, 1, thread_id);
        pclst->status = SPT_WAIT_AMT;
        return SPT_WAIT_AMT;
    }
    else
    {
        return fill_in_rsv_list_simple(pclst, 1, thread_id);
    }
}
/* alloc vector from buffer list first
 * if return NULL, then alloc from free list
 */
unsigned int vec_alloc_combo(cluster_head_t *pclst, int thread_id, spt_vec **vec)
{
    u32 ret;
    ret = vec_alloc_from_buf(pclst, thread_id, vec);
    if(ret == -1)
    {
        ret = vec_alloc(pclst, vec);
    }
    return ret;
}
/* alloc db from buffer list first
 * if return NULL, then alloc from free list
 */
unsigned int data_alloc_combo(cluster_head_t *pclst, int thread_id, spt_dh **db)
{
    u32 ret;
    ret = db_alloc_from_buf(pclst, thread_id, db);
    if(ret == -1)
    {
        ret = db_alloc(pclst, db);
        if(ret == -1)
            return -1;
    }
    atomic_add(1, (atomic_t *)&pclst->data_total);
    return ret;
}

int test_add_page(cluster_head_t *pclst)
{
    char *page, **indir_page, ***dindir_page;
    u32 size;
    int pg_id, offset;
    int ptrs_bit = pclst->pg_ptr_bits;
    int ptrs = (1 << ptrs_bit);
    u64 direct_pgs = CLST_NDIR_PGS;
    u64 indirect_pgs = 1<<ptrs_bit;
    u64 double_pgs = 1<<(ptrs_bit*2);

    if(pclst->pg_num_max == pclst->pg_cursor)
    {
        return CLT_FULL;
    }

    page = spt_alloc_zero_page();
    if(page == NULL)
        return CLT_NOMEM;

    if(pclst->pg_cursor == pclst->pg_num_total)
    {
        cluster_head_t *new_head;
        size = (sizeof(char *)*pclst->pg_num_total + sizeof(cluster_head_t));
        if(size*2 >= PG_SIZE)
        {
            new_head = (cluster_head_t *)spt_alloc_zero_page();
            if(new_head == NULL)
            {
                spt_free_page(page);
                return CLT_NOMEM;
            }
            memcpy((char *)new_head, (char *)pclst, size);
            new_head->pg_num_total = pclst->pg_num_max;
        }
        else
        {
            new_head = spt_malloc(size*2);
            if(new_head == NULL)
            {
                spt_free_page(page);
                return CLT_NOMEM;
            }    
            memcpy(new_head, pclst, size);
            new_head->pg_num_total = (size*2-sizeof(cluster_head_t))/sizeof(char *);
        }
        spt_free(pclst);
        pclst = new_head;

        
    }
    pg_id = pclst->pg_cursor;
    if(pg_id < direct_pgs)
    {
        pclst->pglist[pg_id] = page;
    }
    else if((pg_id -= direct_pgs) < indirect_pgs)
    {
        indir_page = (char **)pclst->pglist[CLST_IND_PG];
        offset = pg_id;
        indir_page[offset] = page;
    }
    else if((pg_id -= indirect_pgs) < double_pgs)
    {
        dindir_page = (char ***)pclst->pglist[CLST_DIND_PG];
        offset = pg_id >> ptrs_bit;
        indir_page = dindir_page[offset];
        offset = pg_id & (ptrs-1);
        indir_page[offset] = page;
    }
    else
    {
        spt_debug("warning: id is too big");
        return 0;
    }
    pclst->pg_cursor++;
    return SPT_OK;
}


int test_add_N_page(cluster_head_t *pclst, int n)
{
    int i;
    for(i=0;i<n;i++)
    {
        if(SPT_OK != cluster_add_page(pclst))
        {
            spt_debug("%d\r\n", i);
            return -1;
        }        
    }
    return 0;
}

void test_vec_alloc_n_times(cluster_head_t *pclst)
{
    int i,vec_a;
    spt_vec *pvec_a, *pid_2_ptr;
    
    for(i=0; ; i++)
    {
        vec_a = vec_alloc(pclst, &pvec_a);
        if(pvec_a == 0)
        {
            //printf("\r\n%d\t%s\ti:%d", __LINE__, __FUNCTION__, i);
            break;
        }
        pid_2_ptr = (spt_vec *)vec_id_2_ptr(pclst, vec_a);
        if(pid_2_ptr != pvec_a)
        {
            spt_debug("vec_a:%d pvec_a:%p pid_2_ptr:%p\r\n", vec_a,pvec_a,pid_2_ptr);
        }
    }
    spt_debug("total:%d\r\n", i);
    for(;i>0;i--)
    {
        vec_free(pclst, i-1);
    }
    spt_debug(" ==============done!\r\n");
    return;
}


