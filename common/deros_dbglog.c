// a genaral module for structured and time-stamped debug logs at different levels, individually controllable

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>

#include "deros_dbglog.h"

#define MAX_DEROS_DBG_SESSIONS  30

char *deros_dbg_levels[] = { "----", "DEBG", "INFO", "WARN", "ERRR", "GRRR" };

static pthread_mutex_t deros_lock_dbglog;
static int *deros_log_level_enabled;
static int deros_log_num_levels;
static int deros_log_min_level;
static char *deros_dbglog_filename;
static char **deros_dbglog_level_names;
static pid_t mypid;

void deros_dbglog_mem_fail()
{
    perror("deros_dbglog: not enough memory");
    exit(1);
} 

int deros_dbglog_init(char *path, char *prefix, int num_levels, char **level_names)
{
    mypid = getpid();
    pthread_mutex_init(&(deros_lock_dbglog), 0);
    deros_log_level_enabled = (int *) malloc(sizeof(int) * (num_levels + 1));
    if (!deros_log_level_enabled) deros_dbglog_mem_fail();
    deros_log_num_levels = num_levels;
    if (level_names) deros_dbglog_level_names = level_names;
    else
    {
        deros_dbglog_level_names = (char **)malloc(sizeof(char *) * (num_levels + 1));
        for (int i = 0; i <= num_levels; i++) 
        {
            deros_dbglog_level_names[i] = (char *)malloc(6);
            sprintf(deros_dbglog_level_names[i], "L%2d", i + 1);
        }
    }
            
    deros_log_min_level = 1;
    for (int i = 0; i <= num_levels; i++) 
        deros_log_level_enabled[i] = 1;
    time_t t;
    time(&t);
    if (path)
    {
        int path_len = strlen(path);
        deros_dbglog_filename = (char *)malloc(path_len + 30);
        if (!deros_dbglog_filename) deros_dbglog_mem_fail();
        if ((path_len > 0) && ((path[path_len - 1] == '/') ||
            (path[path_len - 1] == '\\')))
            sprintf(deros_dbglog_filename, "%s%s_deroslog_%ld.txt", path, prefix, t);
        else sprintf(deros_dbglog_filename, "%s/%s_deroslog_%ld.txt", path, prefix, t);
    }
    else
    {
        deros_dbglog_filename = (char *)malloc(30);
        if (!deros_dbglog_filename) deros_dbglog_mem_fail();
        sprintf(deros_dbglog_filename, "%s_deroslog_%ld.txt", prefix, t);
    }
    
    struct timeval tm;
    gettimeofday(&tm, 0);
    char tajm[30];
    ctime_r(&t, tajm);
    while ((tajm[strlen(tajm) - 1] == '\n') || (tajm[strlen(tajm) - 1] == '\n')) tajm[strlen(tajm) - 1] = 0;

    FILE *f = fopen(deros_dbglog_filename, "w+");
    if (!f)
    {
       perror("could not open dbglog filename");
       return 0;
    }
    fprintf(f, "%ld.%3ld (%s) Deros debug log (pid %u), tab-separated columns: timestamp, node, level, label, message\n", tm.tv_sec, tm.tv_usec /10, tajm, mypid);
    fclose(f);
    return 1;
}

void deros_dbglog_set_minimum_level(int min_level)
{
    if ((min_level < 0) || (min_level > deros_log_num_levels)) return;
    deros_log_min_level = min_level;
}

void deros_dbglog_enable_level(int level, int enable)
{
    if ((level < 1) || (level > deros_log_num_levels)) return;
    deros_log_level_enabled[level] = enable;
}

FILE *deros_dbglog_prologue(int level)
{
    if ((level < 1) || (level > deros_log_num_levels) ||
        (level < deros_log_min_level) ||
        (!deros_log_level_enabled[level])) return 0;
    
    struct timeval tm;
    gettimeofday(&tm, 0);
    
    FILE *f = fopen(deros_dbglog_filename, "a+");
    if (!f)
    {
        perror("cannot open dbglogfilename");
        return 0;
    }
    fprintf(f, "%ld.%3ld\t%u\t%s\t", tm.tv_sec, tm.tv_usec /10, mypid, deros_dbglog_level_names[level]);
    return f;
}

void deros_dbglog_msg(int level, char *node, char *label, char *msg)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\n", node, label, msg);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_int(int level, char *node, char *label, char *msg, int val)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%d\n", node, label, msg, val);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_2int(int level, char *node, char *label, char *msg, int val1, int val2)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%d\t%d\n", node, label, msg, val1, val2);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_3int(int level, char *node, char *label, char *msg, int val1, int val2, int val3)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%d\t%d\t%d\n", node, label, msg, val1, val2, val3);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_str(int level, char *node, char *label, char *msg, char *val)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%s\n", node, label, msg, val);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_2str(int level, char *node, char *label, char *msg, char *val1, char *val2)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%s\t%s\n", node, label, msg, val1, val2);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_3str(int level, char *node, char *label, char *msg, char *val1, char *val2, char *val3)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n", node, label, msg, val1, val2, val3);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_str_int(int level, char *node, char *label, char *msg, char *val1, int val2)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%s\t%d\n", node, label, msg, val1, val2);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_str_2int(int level, char *node, char *label, char *msg, char *val1, int val2, int val3)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%s\t%d\t%d\n", node, label, msg, val1, val2, val3);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_2str_int(int level, char *node, char *label, char *msg, char *val1, char *val2, int val3)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%s\t%s\t%d\n", node, label, msg, val1, val2, val3);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_2str_2int(int level, char *node, char *label, char *msg, char *val1, char *val2, int val3, int val4)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%s\t%s\t%d\t%d\n", node, label, msg, val1, val2, val3, val4);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}

void deros_dbglog_msg_3str_int(int level, char *node, char *label, char *msg, char *val1, char *val2, char *val3, int val4)
{
    FILE *f = deros_dbglog_prologue(level);
    if (f)
    {
        pthread_mutex_lock(&deros_lock_dbglog);
        fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t%d\n", node, label, msg, val1, val2, val3, val4);
        fclose(f);
        pthread_mutex_unlock(&deros_lock_dbglog);
    }
}
