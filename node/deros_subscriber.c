// implementation of the subscriber API of the client node

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "../deros.h"
#include "../common/deros_common.h"
#include "../common/deros_addrs.h"
#include "../common/deros_net.h"
#include "../common/deros_dbglog.h"
#include "deros_core_internal.h"

static int subscriber_node_id[MAX_NUM_SUBSCRIBERS];
static int subscriber_address[MAX_NUM_SUBSCRIBERS];
static subscriber_callback_function subscriber_callback[MAX_NUM_SUBSCRIBERS];
static int subscriber_msgsize[MAX_NUM_SUBSCRIBERS];
static int subscriber_msgqueue_size[MAX_NUM_SUBSCRIBERS];
static int num_subscribers = 0;
static int next_subscriber_id = 0;

static int open_publisher_sockets[MAX_NUM_PUBLISHERS]; 
static int num_open_pub_sockets = 0;

static volatile int subscriber_listen_thread_runs = 0;
static volatile int subscriber_client_thread_runs = 0;

int process_packet_from_publisher(int my_node_id, uint8_t packet_type, uint8_t *packet, int packet_size)
{
    packet[packet_size] = 0;
    char *excl = strchr((char *)packet, '!');
    if (!excl) return 0;
    *excl = 0;
    char *adres = (char *)packet;
    excl++;
    char *msg = strchr(excl, '!');
    if (!msg) return 0;
    *msg = 0;
    int msglen;
    sscanf(excl, "%d", &msglen);
    msg++;
   
    int found = 0;
    int adr_id = find_address(adres, &found); 
    if (!found)  // msg to address we do not know yet are ignored with warning
    {
        deros_dbglog_msg_str(D_WARN, node_names[my_node_id], "subscriber", "msg from publisher to subscriber to unrecognized address=", adres);
        return 1;
    }

    for (int i = 0; i < addr_num_sub[adr_id]; i++)
    {
        int sub_id = addr_subscribers[adr_id][i];
        if (msglen != subscriber_msgsize[sub_id])
        {
            deros_dbglog_msg_str_2int(D_ERRR, node_names[my_node_id], "subscriber", "msg from publisher to subscriber len mismatch (adr, len1, len2)", adres, msglen, subscriber_msgsize[sub_id]);
            return 0;
        }

        subscriber_callback[sub_id]((uint8_t *)msg, msglen);
    }
    return 1;
}

void *subscriber_handler_for_publisher_thread(void *args)
{
    int my_node_id = *((int *)args);
    int my_socket = open_publisher_sockets[num_open_pub_sockets - 1];
    subscriber_client_thread_runs = 1;

    uint8_t *my_buffer = (uint8_t *)malloc(MAX_PACKET_LENGTH + 1);
    if (my_buffer == 0) deros_node_mem_failure("sub handler for pub");
    int packet_size;

    while (1)
    {
        int packet_type = deros_receive_packet(my_socket, my_buffer, &packet_size, MAX_PACKET_LENGTH);
        if (!packet_type) break;
        if (!process_packet_from_publisher(my_node_id, packet_type, my_buffer, packet_size))
        {
            deros_dbglog_msg_int(D_ERRR, node_names[my_node_id], "subscriber", "sub_handler: a problem with packet from pub, packet_type=", packet_type);
            break;
        }
    }

    deros_dbglog_msg_str(D_INFO, node_names[my_node_id], "subscriber", "deros_subscriber_handler: a publisher node disconnected (node)", node_names[my_node_id]);
    //lock
    for (int i = 0; i < num_open_pub_sockets; i++)
        if (my_socket == open_publisher_sockets[i])
        {
            open_publisher_sockets[i] = open_publisher_sockets[--num_open_pub_sockets];
            break;
        }
    // unlock

    close(my_socket);
    free(my_buffer);
    return 0;
}

void *subscriber_listen_thread(void *args)
{
    int my_node_id = next_free_node_id;

    subscriber_listen_thread_runs = 1;

    int deros_sub_server_socket = deros_create_server(node_listen_ports[my_node_id]);
    if (!deros_sub_server_socket) 
    {
        deros_dbglog_msg(D_ERRR, node_names[my_node_id], "subscriber", "deros subscriber: cannot create listening socket");
        return 0;
    }
    do {

        int new_client_socket = deros_wait_for_client_connection(deros_sub_server_socket);
        if (new_client_socket == 0) break;

        subscriber_client_thread_runs = 0;
        open_publisher_sockets[num_open_pub_sockets++] = new_client_socket;

        pthread_t thr;
        pthread_create(&thr, 0, subscriber_handler_for_publisher_thread, &my_node_id);

        while (!subscriber_client_thread_runs) usleep(1);

    } while (1);
    deros_dbglog_msg_str(D_INFO, node_names[my_node_id], "subscriber", "subscriber_listen_thread for node terminates (node)", node_names[my_node_id]);
    return 0;
}

void start_subscriber_listen_thread()
{
    if (subscriber_listen_thread_runs) return;

    pthread_t thr;
    if (pthread_create(&thr, 0, subscriber_listen_thread, 0) != 0)
    {
        deros_dbglog_msg_int(D_GRRR, "sys", "subscriber", "could not create subscriber listening thread", errno);
        exit(1);
    }
    while (!subscriber_listen_thread_runs) usleep(1);
}


int subscriber_register(int node_id, char *address, int message_size, subscriber_callback_function callback, int message_queue_size)
{
    if (strlen(address) > MAX_ADDRESS_LENGTH) return -1;

    if (pthread_mutex_lock(&node_mutexes[node_id])) return -1;

    if ((node_id < 0) ||
        (node_id >= next_free_node_id) ||
        (node_server_sockets[node_id] == 0))
    {
        pthread_mutex_unlock(&node_mutexes[node_id]);
        return -1;
    }

    uint8_t *my_buffer = (uint8_t *) malloc(strlen(address) + 12);
    if (!my_buffer) deros_node_mem_failure("sub register");

    sprintf((char *)my_buffer, "%d!%s", message_size, address);

    if (!deros_send_packet(node_server_sockets[node_id], PACKET_SUB_REGISTER, my_buffer, strlen((char *)my_buffer)))
    {
        deros_dbglog_msg(D_ERRR, node_names[node_id], "subscriber", "sending register subscriber packet failed");
        close(node_server_sockets[node_id]);
        node_server_sockets[node_id] = 0;
        pthread_mutex_unlock(&node_mutexes[node_id]);
        free(my_buffer);
        return -1;
    }
    deros_dbglog_msg(D_DEBG, node_names[node_id], "subscriber", "sent register subscriber packet");

    int sub_id = 0;
    while (sub_id < next_subscriber_id)
    {
        if (subscriber_address[sub_id] == 0) break;
        sub_id++;
    }
    if (sub_id == next_subscriber_id) next_subscriber_id++;

    subscriber_node_id[sub_id] = node_id;
    int adr_found = 0;
    int adr_id = find_address(address, &adr_found);
    if (!adr_found) 
    {
        insert_address_at_index(address, adr_id);
        init_address_of_subscriber(adr_id, sub_id);
    }
    else add_subscriber_to_address(adr_id, sub_id);

    subscriber_address[sub_id] = adr_id;
    subscriber_callback[sub_id] = callback;
    subscriber_msgsize[sub_id] = message_size;
    subscriber_msgqueue_size[sub_id] = message_queue_size;
    num_subscribers++;

    pthread_mutex_unlock(&node_mutexes[node_id]);
    return sub_id;
}

void subscriber_unregister(int subscriber_id)
{
    if ((subscriber_id < 0) ||
        (subscriber_id >= next_subscriber_id) ||
        (subscriber_callback[subscriber_id] == 0)) return;

    int node_id = subscriber_node_id[subscriber_id];
    if ((node_id < 0) || (node_id > next_free_node_id) ||
        (node_server_sockets[node_id] == 0)) return; 

    if (pthread_mutex_lock(&node_mutexes[node_id])) return;

    int adres = subscriber_address[subscriber_id];

    if (!deros_send_packet(node_server_sockets[node_id], PACKET_SUB_UNREGISTER, (uint8_t *)addresses[adres], strlen(addresses[adres])))
    {
        deros_dbglog_msg(D_ERRR, node_names[node_id], "subscriber", "sending unregister subscriber packet failed");
        close(node_server_sockets[node_id]);
        node_server_sockets[node_id] = 0;
    }
    else 
        deros_dbglog_msg(D_DEBG, node_names[node_id], "subscriber", "sent unregister subscriber packet");

    remove_subscriber_from_address(adres, subscriber_id);

    subscriber_node_id[subscriber_id] = 0;
    subscriber_callback[subscriber_id] = 0;
    num_subscribers--;

    pthread_mutex_unlock(&node_mutexes[node_id]);

}

