/*************************************************************************
	> File Name: data_cache.h
	> Author: ma6174
	> Mail: ma6174@163.com 
	> Created Time: Wed 01 Mar 2017 01:53:33 AM PST
 ************************************************************************/

#ifndef __DATA_CACHE_H__
#define __DATA_CACHE_H__

struct data_set_cache
{
	long long  cache_idx;
	long long  ins_num;
	long long  ins_size;
	long long  cur_ins_id;
	void *cache_mem;
};

struct data_set_cache *get_next_data_set_cache(struct data_set_cache *cur);
void *get_next_data(struct data_set_cache *cur);
#endif
