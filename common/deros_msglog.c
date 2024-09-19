// implementation of a module responsible for logging deros messages - each publisher has its own logfile

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include "deros_msglog.h"
#include "deros_dbglog.h"

#define DEROS_MAX_NUM_MSGLOGS 100

static pthread_mutex_t deros_lock_log[DEROS_MAX_NUM_MSGLOGS];
static char *deros_msglog_filename[DEROS_MAX_NUM_MSGLOGS];
static int num_deros_msglogs = 0;

void deros_msglog_mem_fail()
{
    deros_dbglog_msg(D_GRRR, "msglog", "common", "deros_dbglog: not enough memory");
    exit(1);
} 

int deros_msglog_init(char *path, char *prefix)
{
    int handle = num_deros_msglogs++;
    pthread_mutex_init(&deros_lock_log[handle], 0);
    time_t t;
    time(&t);
    if (path)
    {
        int path_len = strlen(path);
        deros_msglog_filename[handle] = (char *)malloc(path_len + strlen(prefix) + 30);
        if (!deros_msglog_filename[handle]) deros_msglog_mem_fail();
        if ((path_len > 0) && ((path[path_len - 1] == '/') ||
            (path[path_len - 1] == '\\')))
            sprintf(deros_msglog_filename[handle], "%s%s_derosmsglog_%ld.txt", path, prefix, t);
        else sprintf(deros_msglog_filename[handle], "%s/%s_derosmsglog_%ld.txt", path, prefix, t);
    }
    else
    {
        deros_msglog_filename[handle] = (char *)malloc(strlen(prefix) + 30);
        if (!deros_msglog_filename[handle]) deros_msglog_mem_fail();
        sprintf(deros_msglog_filename[handle], "%s_derosmsglog_%ld.txt", prefix, t);
    }
    
    FILE *f = fopen(deros_msglog_filename[handle], "w+");
    if (!f)
    {
       deros_dbglog_msg_str(D_ERRR, "msglog", "common", "cannot open msglogfilename", deros_msglog_filename[handle]);
       return -1;
    }
    struct timeval tm;
    gettimeofday(&tm, 0);
    char tajm[30];
    ctime_r(&t, tajm);
    while ((tajm[strlen(tajm) - 1] == '\n') || (tajm[strlen(tajm) - 1] == '\n')) tajm[strlen(tajm) - 1] = 0;
    fprintf(f, "%ld.%ld (%s), Deros message log (pid %u) tab-separated columns: timestamp, node_name, address, message[max100]\n", tm.tv_sec, tm.tv_usec / 10, tajm, getpid());
    fclose(f);

    return handle;
}

void deros_msglog_published_msg(int handle, struct timeval *tm, char *pub_node_name, char *address, 
                                char *msg, int msglen, int isAlreadyFormatted)
{
    static char msgcopy[DEROS_MSGLOG_MAX_MESSAGE_LEN + 1];

    pthread_mutex_lock(&deros_lock_log[handle]);
    
    FILE *f = fopen(deros_msglog_filename[handle], "a+");
    if (!f)
    {
       deros_dbglog_msg_str(D_ERRR, "msglog", "common", "cannot open msglogfilename", deros_msglog_filename[handle]);
       return;
    }
    fprintf(f, "%ld.%ld\t%s\t%s\t", tm->tv_sec, tm->tv_usec / 10, pub_node_name, address);
    if (isAlreadyFormatted)
    {
        if (msglen > DEROS_MSGLOG_MAX_MESSAGE_LEN) msglen = DEROS_MSGLOG_MAX_MESSAGE_LEN;
        strncpy(msgcopy, msg, msglen);
        msgcopy[msglen] = 0;
        fprintf(f, "%s\n", msgcopy);
    }
    else
    {
        if (msglen > DEROS_MSGLOG_MAX_MESSAGE_LEN) msglen = DEROS_MSGLOG_MAX_MESSAGE_LEN;
        for (int i = 0; i < msglen; i++)
            if ((msg[i] >= 32) && (msg[i] < 127)) fprintf(f, "%c", msg[i]);
            else fprintf(f, ".");
        fprintf(f, "  [");
        for (int i = 0; i < msglen - 1; i++)
            fprintf(f, "%d ", msg[i]);
        fprintf(f, "%d]\n", msg[msglen - 1]);
    }
    fclose(f);

    pthread_mutex_unlock(&deros_lock_log[handle]);
}

