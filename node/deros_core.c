// client node initialization and termination, and listening to and processing of pushed publishers connections and messages

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "../deros.h"
#include "../common/deros_common.h"
#include "../common/deros_net.h"
#include "../common/deros_dbglog.h"
#include "deros_core_internal.h"


static pthread_mutex_t global_deros_lock;

int next_free_node_id = 0;

char *node_names[MAX_NODES];
static char *node_server_addresses[MAX_NODES];
static int node_server_ports[MAX_NODES];
char *node_log_path[MAX_NODES];

pthread_mutex_t node_mutexes[MAX_NODES];
int node_server_sockets[MAX_NODES];
static volatile int node_thread_started;
static uint8_t *node_recv_packet[MAX_NODES];
int node_listen_ports[MAX_NODES];

void deros_node_process_packet(int node_id, uint8_t packet_type, int packet_size)
{
    char *pack = (char *)node_recv_packet[node_id];
    switch (packet_type)
    {
        case PACKET_ADD_SUBSCRIBER:
        case PACKET_REMOVE_SUBSCRIBER:

                deros_dbglog_msg_str(D_DEBG, node_names[node_id], "process_packet", "add/remove subscriber packet", pack);

                char *exclpos = strchr(pack, '!');
                if (exclpos == 0)
                {
                    deros_dbglog_msg(D_ERRR, node_names[node_id], "process_packet", "malformed add subscriber packet");
                    return;
                }
                *exclpos = 0;

                int subscriber_port;
                sscanf(pack, "%d", &subscriber_port);
                char *restpack = exclpos + 1;
                exclpos = strchr(restpack, '!');
                if (exclpos == 0)
                {
                    deros_dbglog_msg(D_ERRR, node_names[node_id], "process_packet", "malformed ADD subscriber packet");
                    return;
                }
                *exclpos = 0;
                char *subscriber_ip = (char *)malloc(strlen(restpack) + 1);
                if (!subscriber_ip) deros_node_mem_failure("node add/remove sub");
                strcpy(subscriber_ip, restpack);

                deros_dbglog_msg_str(D_DEBG, node_names[node_id], "process_packet", "subscriber_ip", subscriber_ip);

                restpack = exclpos + 1;
                char *adres = (char *)malloc(strlen(restpack) + 1);
                if (!adres) deros_node_mem_failure("node add/remove sub");
                strcpy(adres, restpack);

                if (packet_type == PACKET_ADD_SUBSCRIBER) 
                {
                    if (!publisher_add_new_subscriber(subscriber_port, subscriber_ip, adres))
                    {
                        free(adres);
                        free(subscriber_ip);
                    }
                }
                else
                {
                    publisher_remove_subscriber(subscriber_port, subscriber_ip, adres);
                    free(adres);
                    free(subscriber_ip);
                }
                break;

    }

}


void *node_read_from_server_thread(void *arg)
{
    uint8_t packet_type;
    int my_node_id = next_free_node_id - 1;
    node_recv_packet[my_node_id] = (uint8_t *)malloc(MAX_PACKET_LENGTH + 1);
    if (!node_recv_packet[my_node_id]) deros_node_mem_failure("read from server thread");

    node_thread_started = 1;
    deros_dbglog_msg_int(D_DEBG, node_names[my_node_id], "process_packet", "read thread for node started", my_node_id);

    int packet_size = 0;

    while ((packet_type = deros_receive_packet(node_server_sockets[my_node_id], node_recv_packet[my_node_id], &packet_size, MAX_PACKET_LENGTH)))
    {
        deros_dbglog_msg_int(D_DEBG, node_names[my_node_id], "process_packet", "arrived packet", packet_type);
        node_recv_packet[my_node_id][packet_size] = 0;
        deros_node_process_packet(my_node_id, packet_type, packet_size);  
    }

    free(node_recv_packet[my_node_id]);
    deros_dbglog_msg_int(D_DEBG, node_names[my_node_id], "process_packet", "read thread for node finished", my_node_id);
    return 0;
}


int deros_init(char *server_address, int server_port, char *node_name, int listen_port, char *log_path)
{
    uint8_t packet_type;

    if ((node_name == 0) || (log_path == 0) || (server_address == 0) || (server_port > 49151) || (server_port < 1024)) 
    {
       printf("deros_init() illegal parameters\n");
       return -1;
    }
    if (next_free_node_id == 0)
    {
        pthread_mutex_init(&global_deros_lock, 0);
        deros_dbglog_init(log_path, node_name, 5, deros_dbg_levels);
    }
    pthread_mutex_lock(&global_deros_lock);

    pthread_mutex_init(&node_mutexes[next_free_node_id], 0);


    node_log_path[next_free_node_id] = (char *) malloc(strlen(log_path) + 1);
    strcpy(node_log_path[next_free_node_id], log_path);
    node_listen_ports[next_free_node_id] = listen_port;
    start_subscriber_listen_thread();

    int sock_conn = deros_connect_to_server(server_address, server_port);
    if (sock_conn)
    {
        char *init_msg = (char *)malloc(strlen(node_name) + strlen(INIT_MSG_HEADER) + 1 + 11);
        if (!init_msg) deros_node_mem_failure("init msg");

        sprintf(init_msg, "%s%s!%d", INIT_MSG_HEADER, node_name, listen_port);
        int initmsg_len = strlen(init_msg);
        int packet_size = 0;
        int responsemsg_len = strlen(INIT_MSG_RESPONSE) + strlen(node_name);
        if (!deros_send_packet(sock_conn, PACKET_INIT, (uint8_t *)init_msg, initmsg_len))
        {
            close(sock_conn);
            sock_conn = 0;
        }
        else if (!(packet_type = deros_receive_packet(sock_conn, (uint8_t *)init_msg, &packet_size, responsemsg_len)))
        {
            close(sock_conn);
            sock_conn = 0;
        }
        if (sock_conn && 
            (packet_type == PACKET_RESPONSE_INIT) &&
            (packet_size == responsemsg_len) &&
            (strncmp(init_msg, INIT_MSG_RESPONSE, strlen(INIT_MSG_RESPONSE)) == 0) &&
            (strncmp(init_msg + strlen(INIT_MSG_RESPONSE), node_name, strlen(node_name)) == 0))
        {
            deros_dbglog_msg(D_INFO, node_name, "process_packet", "connection to deros server established");
            node_server_sockets[next_free_node_id] = sock_conn;
            node_names[next_free_node_id] = node_name;
            node_server_ports[next_free_node_id] = server_port;
            node_server_addresses[next_free_node_id] = (char *)malloc(strlen(server_address) + 1);
            strcpy(node_server_addresses[next_free_node_id], server_address);
            next_free_node_id++;
            pthread_t thr;
            node_thread_started = 0;
            pthread_create(&thr, 0, node_read_from_server_thread, 0);
            while (!node_thread_started) usleep(1);

        }
        else if (sock_conn)
        {
            deros_dbglog_msg_str_2int(D_ERRR, node_name, "process_packet", "unexpected response from deros server, (msg, response, size)", init_msg, packet_type, packet_size);
            close(sock_conn);
            pthread_mutex_destroy(&node_mutexes[next_free_node_id]);
            sock_conn = 0;
        }
    }

    pthread_mutex_unlock(&global_deros_lock);
    if (sock_conn) return next_free_node_id - 1;
    return -1;
}

void deros_done(int node_id)
{
    pthread_mutex_lock(&global_deros_lock);
    do {

      if ((node_id >= next_free_node_id) || (node_id < 0)) break;
      if (node_server_sockets[node_id] == 0) break;

      close(node_server_sockets[node_id]);
      node_server_sockets[node_id] = 0;
      free(node_server_addresses[node_id]);

    } while (0);
    pthread_mutex_unlock(&global_deros_lock);
}

void deros_node_mem_failure(char *msg)
{
    deros_dbglog_msg_str(D_GRRR, "memf", "node", "not enough memory", msg);
    exit(1);
}

