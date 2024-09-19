#ifndef __DEROS_CORE_INTERNAL__
#define __DEROS_CORE_INTERNAL__

// internal interaction inside of the client node

#include <pthread.h>

#include "../common/deros_common.h"

extern char *node_names[MAX_NODES];
extern char *node_log_path[MAX_NODES];
extern pthread_mutex_t node_mutexes[MAX_NODES];
extern int node_server_sockets[MAX_NODES];
extern int node_listen_ports[MAX_NODES];


extern int next_free_node_id;

void deros_node_mem_failure(char *msg);

void start_subscriber_listen_thread();
void publisher_remove_subscriber(int subscriber_port, char *subscriber_ip, char *adres);
int publisher_add_new_subscriber(int subscriber_port, char *subscriber_ip, char *adres);

#endif
