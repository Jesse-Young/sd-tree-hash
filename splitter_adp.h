/*************************************************************************
	> File Name: splitter_adp.h
	> Author: ma6174
	> Mail: ma6174@163.com 
	> Created Time: Fri 24 Mar 2017 01:01:03 AM PDT
 ************************************************************************/
char *tree_get_key_from_data(char *pdata);
void tree_free_key(char *pkey);
void tree_free_data(char *pdata);
char *tree_construct_data_from_key(char *pkey);
