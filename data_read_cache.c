/*************************************************************************
	> File Name: data_read_cache.c
	> Author: ma6174
	> Mail: ma6174@163.com 
	> Created Time: Wed 01 Mar 2017 01:53:20 AM PST
 ************************************************************************/

#include<stdio.h>
#include "data_set.h"
#include "data_cache.h"

#if 1
void* get_next_data(struct data_set_cache *cur)
{
	if(cur->cur_ins_id < cur->ins_num)
	{
		void *data = cur->cache_mem + cur->cur_ins_id*cur->ins_size;	
		cur->cur_ins_id++;
		return data;
	}
	else
	{
		return NULL;
	}
}


struct data_set_cache* get_data_set_cache(long long cache_idx)
{
	struct data_set_cache *set_cache = NULL;
	struct data_set_file *cur,*prev;
	int i,j,k;
	void *set_cache_mem = NULL;
	long long ins_num_per_cache = 0;
	long long max_cache_idx = 0;
	long long read_max_ins_num = 0;
	
	long long start_off = 0;
	long long start_idx,cache_num;

	void *mem_start = data_set_config_map_address;

	if(-1 == data_set_config_map_read_start)
	{
		start_off = 0;
	}
	else
	{
		start_off = data_set_config_map_read_start * data_set_config_instance_len;
	}
	ins_num_per_cache = data_set_config_cache_unit_len / data_set_config_instance_len;

	set_cache = malloc(sizeof(struct data_set_cache));
	if(!set_cache)
	{
		return NULL;
	}

	if(-1 == data_set_config_map_read_len)
	{
		if(-1 == data_set_config_map_read_start)
		{
			data_set_config_map_read_start = 0;
		}
		read_max_ins_num = data_set_config_instance_num - data_set_config_map_read_start;  
	}
	else
	{
		read_max_ins_num = data_set_config_map_read_len;
	}

	cache_num = read_max_ins_num / ins_num_per_cache;
	
	if(read_max_ins_num%ins_num_per_cache)
	{
		cache_num++;
	}

	//printf("cache max is %d\r\n",cache_num);

	//printf("cache idx is %d\r\n",cache_idx);
	set_cache->cache_idx = cache_idx;
	if(cache_idx < (cache_num -1))
	{
		set_cache->ins_num = ins_num_per_cache;
	}
	else if(cache_idx == (cache_num -1))
	{
		set_cache->ins_num = read_max_ins_num - ins_num_per_cache*cache_idx;
	}
	else
	{
		free(set_cache);
		return NULL;
	}
	//printf("cache ins num is %d\r\n",set_cache->ins_num);
	set_cache->cur_ins_id = 0;
	set_cache->ins_size = data_set_config_instance_len;
	set_cache->cache_mem = mem_start + start_off  + cache_idx*data_set_config_cache_unit_len;

	return set_cache;	
}

struct data_set_cache *get_next_data_set_cache(struct data_set_cache *cur)
{
	if(NULL == cur)
	{
		return get_data_set_cache(0);
	}
	else
	{
		return get_data_set_cache(cur->cache_idx +1);	
	}
}
#endif
