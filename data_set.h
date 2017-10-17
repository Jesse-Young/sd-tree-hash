/*************************************************************************
	> File Name: data_set.h
	> Author: ma6174
	> Mail: ma6174@163.com 
	> Created Time: Mon 27 Feb 2017 07:52:37 PM PST
 ************************************************************************/
#ifndef __DATA_SET_H__
#define __DATA_SET_H__

struct data_set_file
{
	char set_name[64];
	int  set_len;
	int  set_num;
	int  set_index;
	int  set_file_status;
	int  file_fd;
	struct  data_set_file *next;
};

#define FILE_STATUS_NULL 0
#define FILE_STATUS_DATA 1
#define FILE_STATUS_MAP  2
#define FILE_STATUS_CLOSE 3

extern long long  data_set_config_instance_len ;
extern long long  data_set_config_instance_num ;
extern long long  data_set_config_random ;
extern long long  data_set_config_file_len ;
extern long long  data_set_config_cache_unit_len;

extern long data_set_config_map_address ;
extern long long data_set_config_map_read_start ;
extern long long data_set_config_map_read_len;
extern int data_set_config_insert_thread_num;
extern int data_set_config_delete_thread_num;


static inline void set_instance_len_config(long long  len)
{
	data_set_config_instance_len = len;
}

static inline void set_instance_num_config(long long  num)
{
	data_set_config_instance_num = num;
}

static inline void set_data_file_len_config(long long  len)
{
	data_set_config_file_len = len;
}

static inline void set_instance_random(long long value)
{
	data_set_config_random = value;
}

static inline void set_data_map_start_addr(long value)
{
	data_set_config_map_address = value;
}

static inline void set_map_read_start(long long value)
{
	data_set_config_map_read_start = value;
}

static inline void set_map_read_len(long long value)
{
	data_set_config_map_read_len  = value;
}

static inline void set_read_cache_unit_size(long long value)
{
	data_set_config_cache_unit_len = value;
}
static inline void set_insert_thread_num(int value)
{
	data_set_config_insert_thread_num = value;
}

static inline void set_delete_thread_num(int value)
{
	data_set_config_delete_thread_num = value;
}
struct data_set_file*  get_data_set_file_list();
int construct_data_set(struct data_set_file *list);
void print_data_config();
void print_data_set_file_list(struct data_set_file *flist);
void* map_data_set_file_shared(struct data_set_file *flist,long start_addr);
void* map_data_set_file_anonymous(struct data_set_file *flist,long start_addr);

void test_insert_proc(void *args);
void test_delete_proc(void *args);

#endif

