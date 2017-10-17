/*************************************************************************
	> File Name: splitter_adp.c
	> Author: ma6174
	> Mail: ma6174@163.com 
	> Created Time: Fri 24 Mar 2017 12:18:58 AM PDT
 ************************************************************************/

#include<stdio.h>
#include "chunk.h"
#include "atomic_user.h"

u64 g_get_key = 0;
u64 g_free_key = 0;

char *tree_get_key_from_data(char *pdata)
{
    atomic64_add(1, (atomic64_t *)&g_get_key);

	return pdata;
}
void tree_free_key(char *key)
{
    atomic64_add(1, (atomic64_t *)&g_free_key);

	return;
}
void tree_free_data(char *p)
{
	return;
}

char *tree_construct_data_from_key(char *pkey)
{
    char *pdata;

    pdata = (char *)malloc(DATA_SIZE);
    if(pdata == NULL)
        return NULL;
    memcpy(pdata, pkey, DATA_SIZE);
    return (char *)pdata;
}

