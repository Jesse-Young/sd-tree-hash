/*************************************************************************
	> File Name: test_case_main.c
	> Author: ma6174
	> Mail: ma6174@163.com 
	> Created Time: Tue 21 Feb 2017 07:35:16 PM PST
 ************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <string.h>
#include <pthread.h>
#include <sched.h>
#include "data_set.h"
#include "data_cache.h"
#include "chunk.h"
#include "splitter_adp.h"
#include "spt_dep.h"
char test_case_name[64] = {'t','e','s','t','_','c','a','s','e'};

typedef void (*test_proc_pfn)(void *args);

void* test_insert_thread(void *arg);

void* test_delete_thread(void *arg);
void* test_divid_thread(void *arg);

enum cmd_index
{
	SET_NAME,
	DATA_LEN ,
	DATA_NUM,
	RANDOM_SET,
	FILE_SIZE,
	MAP_START_ADDR,
	MAP_READ_START,
	MAP_READ_LEN,
	CACHE_UNIT_SIZE,
};

struct cmdline_dsc
{
	int cmd_id;
	char *cmd;
	char *dsc;
};

struct cmdline_dsc cmdline[] = 
{
	{.cmd_id = SET_NAME,.cmd = "set_name",.dsc = "data set name ,type string"},
	
	{.cmd_id = DATA_LEN,.cmd = "instance_size",.dsc = "data instance len,type value"},
	{.cmd_id = DATA_NUM,.cmd = "instance_num",.dsc = "data instance num,type value"},
	
	{.cmd_id = RANDOM_SET,.cmd = "random_set",.dsc = "1 for random data construct ,otherwise specfic data set files"},
	{.cmd_id = FILE_SIZE,.cmd = "data_set_file_size",.dsc = "per data set file len,type value"},

	{.cmd_id = MAP_START_ADDR ,.cmd = "map_start_addr",.dsc = "data set map to memory from file,type value"},
	{.cmd_id = MAP_READ_START,.cmd = "map_read_start",.dsc = "the start data instance maped to memory start from 0 to instance_num-1",},
	{.cmd_id = MAP_READ_LEN,.cmd = "map_read_len",.dsc = "the instance num maped to memory from 1 to instance_num"},

	{.cmd_id = CACHE_UNIT_SIZE,.cmd = "read_cache_unit_size",.dsc = "the unit len read from map to test"},
};

enum test_proc_type
{
	find_proc,
	insert_proc,
	delete_proc,
};

struct proc_type
{
	int type;
	test_proc_pfn pfn;
};



struct proc_type test_proc_array[] = 
{
	{.type = insert_proc,.pfn = test_insert_proc},
	
	{.type = delete_proc,.pfn = test_delete_proc},
};

#define get_cmd_id 0
#define get_cmd_value_begin 1
#define get_cmd_value 2
#define get_cmd_end 3
void print_cmd_help_info(void)
{
	int i = 0;
	printf("\r\ncmd style is xxx=xxx deperated by one space\r\n");
	
	for(i = 0; i < sizeof(cmdline)/sizeof(struct cmdline_dsc); i++)
	{
		printf("%s : %s \r\n",cmdline[i].cmd,cmdline[i].dsc);
	}
	return ;
}


int  parse_cmdline(int argc,char *argv[])
{
	int i ,j, k ;
	int cmd_id = -1;
	int step = get_cmd_id;

	for(i = 1; i < argc ; i++){
		j = 0;
		if(1 == i)
		{
			if(argv[i][0] == '?')
			{
				print_cmd_help_info();
				return 1;
			}
		}
		step = get_cmd_id;
		while(argv[i][j] != '\0')
		{
			if(argv[i][j]!= ' '){
				switch(step)
				{
					case get_cmd_id:
					{
						for(k = 0; k < sizeof(cmdline)/sizeof(struct cmdline_dsc); k++)
						{
							if(strncmp(&argv[i][j],cmdline[k].cmd,strlen(cmdline[k].cmd))== 0)
							{
								cmd_id = cmdline[k].cmd_id;
								j += strlen(cmdline[k].cmd);
								step = get_cmd_value_begin;
								break;
							}
						}
						if(-1 == cmd_id){
							goto error_cmd;
						}
						break;
					}
					
					case get_cmd_value_begin:
					{
						if(argv[i][j] != '=')
						{
							goto error_cmd;	
						}
						else
						{
							step = get_cmd_value;
							j++;
						}
						break;
					}

					case get_cmd_value:
					{
						long long value = -1;
						
						switch (cmd_id)
						{
							case SET_NAME:
							{
								strncpy(test_case_name,&argv[i][j],64);
								printf(test_case_name);
								break;
							}
							
							case DATA_LEN:
							{
								value = strtol(&argv[i][j],NULL,10);
								printf("value is %lld\r\n",value);
								set_instance_len_config(value);
								break;
							}

							case DATA_NUM :
							{
								value = strtol(&argv[i][j],NULL,10);
								printf("value is %lld\r\n",value);
								set_instance_num_config(value);
								break;
							}
							case RANDOM_SET:
							{
								value = strtol(&argv[i][j],NULL,10);
								printf("value is %lld\r\n",value);
								set_instance_random(value);
								break;
							}
							
							case FILE_SIZE:
							{
								value = strtol(&argv[i][j],NULL,10);
								printf("value is %lld\r\n",value);
								set_data_file_len_config(value);
								break;
							}

							case MAP_START_ADDR:
							{
								value = strtol(&argv[i][j],NULL,10);
								printf("value is %lld\r\n",value);
								set_data_map_start_addr(value);
								break;
							}
							case MAP_READ_START:
							{
								value = strtol(&argv[i][j],NULL,10);
								printf("value is %lld\r\n",value);
								set_map_read_start(value);
								break;
							}
							case MAP_READ_LEN:
							{
								value = strtol(&argv[i][j],NULL,10);
								printf("value is %lld\r\n",value);
								set_map_read_len(value);
								break;
							}
						
							case CACHE_UNIT_SIZE:
							{
								value = strtol(&argv[i][j],NULL,10);
								printf("value is %lld\r\n",value);
								set_read_cache_unit_size(value);
								break;
							}	
							default :
							{
								printf("error cmd id in get value\r\n");
								goto error_cmd;
							}
						
						}
						step = get_cmd_end;

						break;
					}
					
					case get_cmd_end:
					{
						j++;
						break;
					}

					default :
						printf("error parse cmd step\r\n");
						goto error_cmd;
				}
			}
			else
			{
				j++;
			}
		}
	}
	
	return 0;

error_cmd:
	printf("error cmdline %s\r\n",argv[i]);
	return -1;
}

struct data_set_file *set_file_list = NULL;
extern u64 g_vec;
int main(int argc,char *argv[])
{
	int ret = -1;
	void *addr = NULL;
	long long get_data_len;
	int i = 0 ;
	int err;
	pthread_t ntid;
	int thread_num = 0;

	if(parse_cmdline(argc,argv))
	{
		return 0;
	}
	set_file_list = get_data_set_file_list();

	if(NULL == set_file_list)
	{
		printf("get file list error\r\n");
		return 0;
	}
	ret = construct_data_set(set_file_list);
	printf("construct_data_set ret is %d\r\n",ret);
	
	data_set_config_map_address = 0x7fa0746e000;
	
	addr  = map_data_set_file_anonymous(set_file_list,0x7fa0746e000);
	//printf("map data set file ret is %d\r\n",ret);
	if(-1 == data_set_config_map_read_start)
	{
		data_set_config_map_read_start = 0;
	}

	if(-1 == data_set_config_map_read_len )
	{
		get_data_len = -1;
	}
	else
	{
		get_data_len = data_set_config_map_read_len*data_set_config_instance_len;
	}
	get_data_from_file(set_file_list,data_set_config_map_read_start*data_set_config_instance_len,get_data_len);
	
	//set data size
	
	set_data_size(data_set_config_instance_len);
	
	//init cluster head
	thread_num = data_set_config_insert_thread_num + data_set_config_delete_thread_num + 1;	
    printf("thread_num is %d\r\n",thread_num);

    sd_perf_stat_init();

	pgclst = spt_cluster_init(0,DATA_BIT_MAX, thread_num, 
                              tree_get_key_from_data,
                              tree_free_key,
                              tree_free_data,
                              tree_construct_data_from_key);
    if(pgclst == NULL)
    {
        spt_debug("cluster_init err\r\n");
        return 1;
    }
    pghash = spt_hash_tbl_init();
    if(pghash == NULL)
    {
        spt_debug("spt_hash_tbl_init err\r\n");
        return 1;
    }
	#if 1
    ret = spt_insert_initial_tag(pgclst);
    if(ret != SPT_OK)
    {
        spt_debug("spt_insert_initial_tag err\r\n");
        return 1;
    }
    #endif
    g_thrd_h = spt_thread_init(thread_num);
    if(g_thrd_h == NULL)
    {
        spt_debug("spt_thread_init err\r\n");
        return 1;
	}
	data_pre_process();
#if 1
	err = pthread_create(&ntid, NULL, test_divid_thread, (void *)thread_num-1);
	if (err != 0)
		printf("can't create thread: %s\n", strerror(err));
#endif

	g_thrd_id = 0;
//	test_insert_thread(0);
	g_vec = 0;

	sleep(2);    
for(i = 0;  i  < data_set_config_insert_thread_num ; i++)
    {
        err = pthread_create(&ntid, NULL, test_insert_thread, (void *)i);
        if (err != 0)
            printf("can't create thread: %s\n", strerror(err));
    }
	sleep(1);  

	
    for(i = 0;  i  < data_set_config_delete_thread_num ; i++)
    {
        err = pthread_create(&ntid, NULL, test_delete_thread, (void *)(data_set_config_insert_thread_num+i));
        if (err != 0)
            printf("can't create thread: %s\n", strerror(err));
    }

	#if 0
	test_insert_proc(NULL);
	sleep(10);
	test_insert_proc(NULL);
//	sleep(10);
//	test_delete_proc(NULL);
	sleep(10);
//	test_delete_proc(NULL);
	#endif


	while(1)
	{
		sleep(1);
	}
	return 0;

}

void* test_insert_data(char *pdata)
{
    //set_31bit_zero(pdata);
	return insert_data(pgclst,pdata);
}
int test_delete_data(char *pdata)
{
    //set_31bit_zero(pdata);
	return delete_data(pgclst,pdata);
}
extern cluster_head_t *pbclst;
extern int dbg_switch;
int opnum = 0;
int main1()
{
    FILE *f_data, *f_op;
    char *pdata, op, *ret_data;
    int ret;
    int thread_num = 1;
    long long startbit;
    g_data_size = 4096;

    spt_vec veca = {0};

    veca.pos = 0-1;
    startbit = (long long)veca.pos + 1;

    printf("startbit %d\r\n", startbit);
    while(1)
        ;
    
#if 0 
    pgclst = spt_cluster_init(0,DATA_BIT_MAX, thread_num, 
                              tree_get_key_from_data,
                              tree_free_key,
                              tree_free_data,
                              tree_construct_data_from_key);
    if(pgclst == NULL)
    {
        spt_debug("cluster_init err\r\n");
        return 1;
    }
    pghash = spt_hash_tbl_init();
    if(pghash == NULL)
    {
        spt_debug("spt_hash_tbl_init err\r\n");
        return 1;
    }
    ret = spt_insert_initial_tag(pgclst, pghash);
    if(ret != SPT_OK)
    {
        spt_debug("spt_insert_initial_tag err\r\n");
        return 1;
    }
    g_thrd_h = spt_thread_init(thread_num);
    if(g_thrd_h == NULL)
    {
        spt_debug("spt_thread_init err\r\n");
        return 1;
	}
    g_thrd_id = 0;

    while(1)
        ;

   
	pgclst = spt_cluster_init(0,DATA_BIT_MAX, thread_num, 
                              tree_get_key_from_data,
                              tree_free_key,
                              tree_free_data,
                              tree_construct_data_from_key);
    if(pgclst == NULL)
    {
        spt_debug("cluster_init err\r\n");
        return 1;
    }

    g_thrd_h = spt_thread_init(thread_num);
    if(g_thrd_h == NULL)
    {
        spt_debug("spt_thread_init err\r\n");
        return 1;
	}
    g_thrd_id = 0;

    if((f_data=fopen("pagefile","r"))==NULL)
    {
        fprintf(stderr,"Can not open output file.\n");
        return 0;
    }
    if((f_op=fopen("opfile","r"))==NULL)
    {
        fprintf(stderr,"Can not open output file.\n");
        return 0;
    }
    fseek(f_op,0,SEEK_SET);
    fseek(f_data,0,SEEK_SET);
    while(!feof(f_op))
    {
        fread(&op,1,1,f_op);
        pdata = malloc(4096);
        if(pdata == NULL)
            printf("ERR,%s\t%d\r\n", __FILE__, __LINE__);
        fread(pdata,4096,1,f_data);
        spt_thread_start(g_thrd_id);
        if(op == 1)
        {
			if(NULL ==(ret_data =  test_insert_data(pdata)))
            {
                ret = spt_get_errno();
                if(ret == SPT_MASKED)
                {
                    printf("ERR,%s\t%d\r\n", __FILE__, __LINE__);
                }
                else if(ret == SPT_WAIT_AMT)
                {
                    printf("ERR,%s\t%d\r\n", __FILE__, __LINE__);
                }
                else if(ret == SPT_NOMEM)
                {
                    printf("ERR,%s\t%d\r\n", __FILE__, __LINE__);
                }
                else
                {
                    printf("ERR[%d],%s\t%d\r\n", ret, __FILE__, __LINE__);
                }
			}
        }
        else if(op == 2)
        {
			if((ret = test_delete_data(pdata)) < 0)
            {
                if(-10000 == ret)
				{
					printf("ERR,%s\t%d\r\n", __FILE__, __LINE__);
				}
				else
				{
					ret = spt_get_errno();
					if(ret == SPT_MASKED)
					{
                        printf("ERR,%s\t%d\r\n", __FILE__, __LINE__);
					}
					else if(ret == SPT_WAIT_AMT)
					{
                        printf("ERR,%s\t%d\r\n", __FILE__, __LINE__);
					}
					else if(ret == SPT_NOMEM)
					{
                        printf("ERR,%s\t%d\r\n", __FILE__, __LINE__);
					}
					else
					{
                        printf("ERR[%d],%s\t%d\r\n", ret, __FILE__, __LINE__);
					}
				}
            } 
        }
        else
        {
            printf("ERR,%s\t%d", __FILE__, __LINE__);
        }
        spt_thread_exit(g_thrd_id);
        opnum++;
    }
#endif
}

int test_stop = 0;
void* test_insert_thread(void *arg)
{
	int i = (long)arg;
	cpu_set_t mask;
	g_thrd_id = i;
	CPU_ZERO(&mask);
	CPU_SET(i,&mask);
	if(sched_setaffinity(0,sizeof(mask),&mask)== -1)
	{
		printf("warning: could not set CPU AFFINITY\r\n");
	}
	while(test_stop == 0)
	    test_insert_proc(i);
	if(i != 0)
	{	
		while(1)
		{
			sleep(10);		
}
	}
}

void* test_delete_thread(void *arg)
{
	int i = (long)arg;
	cpu_set_t mask;
	g_thrd_id = i;
	CPU_ZERO(&mask);
	CPU_SET(i,&mask);
	if(sched_setaffinity(0,sizeof(mask),&mask)== -1)
	{
		printf("warning: could not set CPU AFFINITY\r\n");
	}
	while(test_stop == 0)
	    test_delete_proc(i);
	while(1)
	{
		sleep(1);
	}
}

void *test_divid_thread(void *arg)
{
	int i = (long)arg;
	cpu_set_t mask;
	int ret;
	
	g_thrd_id = i;
	CPU_ZERO(&mask);
	CPU_SET(i,&mask);
	if(sched_setaffinity(0,sizeof(mask),&mask)== -1)
	{
		printf("warning: could not set CPU AFFINITY\r\n");
	}
	i = pghash->used_bit;
	while(1)
	{
		//spt_divided_scan(pgclst);
		for(;i>14; i--)
		{
			ret = spt_hash_tag_shrink_a_bit(pgclst);
			if(ret != SPT_OK)
			{
				spt_debug("spt_hash_tag_expand_a_bit return err[%d]\r\n", ret);
				return 0;
			}
			sleep(2);
		}
		for(;i<23;i++)
		{
			ret = spt_hash_tag_expand_a_bit(pgclst);
			if(ret != SPT_OK)
			{
				spt_debug("spt_hash_tag_expand_a_bit return err[%d]\r\n", ret);
				return 0;
			}
			sleep(2);
		}
		if(test_stop == 1)
		{
			while(1);
		}
	}
}
