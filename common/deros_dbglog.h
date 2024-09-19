#ifndef __DEROS_DBGLOG_H__
#define __DEROS_DBGLOG_H__

// public api definition for the structured debug log module

// debug messages levels (just an example, init can define otherwise)
#define D_DEBG  1
#define D_INFO  2
#define D_WARN  3
#define D_ERRR  4
#define D_GRRR  5
extern char *deros_dbg_levels[];

int deros_dbglog_init(char *path, char *prefix, int num_levels, char **level_names);
void deros_dbglog_set_minimum_level(int min_level);
void deros_dbglog_enable_level(int level, int enable);
void deros_dbglog_msg(int level, char *node, char *label, char *msg);
void deros_dbglog_msg_int(int level, char *node, char *label, char *msg, int val1);
void deros_dbglog_msg_2int(int level, char *node, char *label, char *msg, int val1, int val2);
void deros_dbglog_msg_3int(int level, char *node, char *label, char *msg, int val1, int val2, int val3);
void deros_dbglog_msg_str(int level, char *node, char *label, char *msg, char *val);
void deros_dbglog_msg_2str(int level, char *node, char *label, char *msg, char *val1, char *val2);
void deros_dbglog_msg_3str(int level, char *node, char *label, char *msg, char *val1, char *val2, char *val3);
void deros_dbglog_msg_str_int(int level, char *node, char *label, char *msg, char *val1, int val2);
void deros_dbglog_msg_str_2int(int level, char *node, char *label, char *msg, char *val1, int val2, int val3);
void deros_dbglog_msg_2str_int(int level, char *node, char *label, char *msg, char *val1, char *val2, int val3);
void deros_dbglog_msg_2str_2int(int level, char *node, char *label, char *msg, char *val1, char *val2, int val3, int val4);
void deros_dbglog_msg_3str_int(int level, char *node, char *label, char *msg, char *val1, char *val2, char *val3, int val4);


#endif
