// implementation of the publisher api for client node

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include "../deros.h"
#include "../common/deros_net.h"
#include "../common/deros_addrs.h"
#include "../common/deros_msglog.h"
#include "../common/deros_dbglog.h"
#include "deros_core_internal.h"

typedef char *(*pretty_print_function)(uint8_t *message, int length);

int publisher_node_id[MAX_NUM_PUBLISHERS];
char *publisher_address[MAX_NUM_PUBLISHERS];
int publisher_msgsize[MAX_NUM_PUBLISHERS];
int publisher_msgqueue_size[MAX_NUM_PUBLISHERS];
int publisher_log_enabled[MAX_NUM_PUBLISHERS];
int publisher_log_initialized[MAX_NUM_PUBLISHERS];
int publisher_log_handle[MAX_NUM_PUBLISHERS];
pretty_print_function publisher_pretty_printer[MAX_NUM_PUBLISHERS];
int *subscribed_remote_node_ids[MAX_NUM_PUBLISHERS];
int num_sub_remote_nodes[MAX_NUM_PUBLISHERS];
int num_publishers = 0;
int next_publisher_id = 0;

pthread_mutex_t remote_nodes_lock;

char* s_remote_node_IP[MAX_NUM_REMOTE_SUBSCRIBERS];
int s_remote_node_port[MAX_NUM_REMOTE_SUBSCRIBERS];
int s_remote_node_used_by_num_pubs[MAX_NUM_REMOTE_SUBSCRIBERS];
int s_remote_node_socket[MAX_NUM_REMOTE_SUBSCRIBERS];
uint8_t s_remote_node_msg_queue[MAX_NUM_REMOTE_SUBSCRIBERS];
int s_remote_node_items_in_queue[MAX_NUM_REMOTE_SUBSCRIBERS];
int next_remote_node_id = 0;

void deros_pub_mem_failure(char *msg)
{
    deros_dbglog_msg_str(D_GRRR, "memf", "publisher", "not enough memory", msg);
    exit(1);
}

int find_remote_node(char *ip, int port)
{
    for (int i = 0; i < next_remote_node_id; i++)
        if (s_remote_node_port[i] == 0) continue;
        else if ((s_remote_node_port[i] == port) &&
                 (strncmp(s_remote_node_IP[i], ip, 15) == 0)) return i;
    return -1;
} 

int add_remote_node(char *ip, int port)
{
    int i;
    for (i = 0; i < next_remote_node_id; i++)
        if (s_remote_node_port[i] == 0)
            break;
    if (i == next_remote_node_id) 
        next_remote_node_id++;

    s_remote_node_port[i] = port;
    s_remote_node_IP[i] = ip;

    s_remote_node_socket[i] = deros_connect_to_server(s_remote_node_IP[i], s_remote_node_port[i]);
    if (!s_remote_node_socket[i])
    {
        s_remote_node_port[i] = 0;
        return -1;
    }
    s_remote_node_used_by_num_pubs[i] = 1;

    return i;
}

int publisher_register(int node_id, char *address, int message_size, int message_queue_size)
{
    if (strlen(address) > MAX_ADDRESS_LENGTH) return -1;
    if (pthread_mutex_lock(&node_mutexes[node_id])) return -1;

    if (node_server_sockets[node_id] == 0)
    {
        pthread_mutex_unlock(&node_mutexes[node_id]); 
        return -1;
    }

    uint8_t *my_buffer = (uint8_t *) malloc(strlen(address) + 12);
    if (!my_buffer) deros_pub_mem_failure("pub register");

    sprintf((char *)my_buffer, "%d!%s", message_size, address);

    if (!deros_send_packet(node_server_sockets[node_id], PACKET_PUB_REGISTER, my_buffer, strlen((char *)my_buffer)))
    {
        deros_dbglog_msg(D_ERRR, node_names[node_id], "publisher", "sending register publisher packet failed");
        close(node_server_sockets[node_id]);
        node_server_sockets[node_id] = 0;
        pthread_mutex_unlock(&node_mutexes[node_id]);
        free(my_buffer);
        return -1;
    }
    deros_dbglog_msg(D_DEBG, node_names[node_id], "publisher", "sent register publisher packet");

    int pub_id = 0;
    while (pub_id < next_publisher_id)
    {
        if (publisher_address[pub_id] == 0) break;
        pub_id++;
    }
    if (pub_id == next_publisher_id) 
    {
        next_publisher_id++;
        deros_dbglog_msg_int(D_DEBG, node_names[node_id], "publisher", "increased next_publisher_id to", next_publisher_id);
    }

    publisher_node_id[pub_id] = node_id;
    publisher_address[pub_id] = (char *)malloc(strlen(address) + 1);
    if (!publisher_address[pub_id]) deros_pub_mem_failure("pub register adr");
    publisher_pretty_printer[pub_id] = 0;
    publisher_log_enabled[pub_id] = 0;
    publisher_log_initialized[pub_id] = 0;
    strcpy(publisher_address[pub_id], address);
    publisher_msgsize[pub_id] = message_size;
    publisher_msgqueue_size[pub_id] = message_queue_size;
    subscribed_remote_node_ids[pub_id] = 0;
    num_sub_remote_nodes[pub_id] = 0;
    num_publishers++;

    pthread_mutex_unlock(&node_mutexes[node_id]);
    free(my_buffer);
    return pub_id;
}

int publish(int publisher_id, uint8_t *message, int msg_len)
{
    if ((publisher_id < 0) || (publisher_id >= next_publisher_id) ||
        (publisher_address[publisher_id] == 0)) return 0;

    int node_id = publisher_node_id[publisher_id];

    if ((publisher_msgsize[publisher_id] >= 0) &&
        (publisher_msgsize[publisher_id] != msg_len)) 
    {
        deros_dbglog_msg_str_2int(D_GRRR, node_names[node_id], "publisher", "publishing message to address with incorrect length (adr, len1, len2)", publisher_address[publisher_id], publisher_msgsize[publisher_id], msg_len);
        exit(1);
    }

    struct timeval timestamp;
    gettimeofday(&timestamp, 0);

    if (pthread_mutex_lock(&node_mutexes[node_id])) return 0;

    char *adres = publisher_address[publisher_id];
    int adrlen = strlen(adres);
    uint8_t *packet = (uint8_t *) malloc(adrlen + 2 + 10 + msg_len + 1);
    if (!packet) deros_pub_mem_failure("publish packet");

    for (int remote = 0; remote < num_sub_remote_nodes[publisher_id]; remote++)
    {
        int remote_node = subscribed_remote_node_ids[publisher_id][remote];

        int remote_socket = s_remote_node_socket[remote_node];

        sprintf((char *)packet, "%s!%d!", adres, msg_len);
        int paklen1 = strlen((char *)packet);
        memcpy(packet + paklen1, message, msg_len);
        if (!deros_send_packet(remote_socket, PACKET_NEW_MESSAGE, packet, msg_len + paklen1))
        {
            deros_dbglog_msg_2str_int(D_WARN, node_names[node_id], "publisher", "publishing message failed, will try reconnecting (adr,dstip,dstport)", adres, s_remote_node_IP[remote_node], s_remote_node_port[remote_node]);

            close(remote_socket);
            s_remote_node_socket[remote_node] = -1;  // indicates reconnecting
            pthread_mutex_unlock(&node_mutexes[node_id]);
            free(packet);
            return 0;
        }
        deros_dbglog_msg_3str_int(D_DEBG, node_names[node_id], "publisher", "published message to subscriber (node,adr,dstip,dstport)", node_names[node_id], adres, s_remote_node_IP[remote_node], s_remote_node_port[remote_node]);
    }
    free(packet);
    if (!publisher_log_enabled[publisher_id]) 
    {
        pthread_mutex_unlock(&node_mutexes[node_id]);
        return 1;
    }

    char *pretty = (char *)message;
    if (publisher_pretty_printer[publisher_id])
    {
       pretty = publisher_pretty_printer[publisher_id](message, msg_len);
       deros_msglog_published_msg(publisher_log_handle[publisher_id], &timestamp, node_names[node_id], adres, pretty, strlen(pretty), 1);
    }
    else deros_msglog_published_msg(publisher_log_handle[publisher_id], &timestamp, node_names[node_id], adres, (char *)message, msg_len, 0);

    pthread_mutex_unlock(&node_mutexes[node_id]);

    return 1;
}    

int publisher_add_new_subscriber(int subscriber_port, char *subscriber_ip, char *adres)
{
    pthread_mutex_lock(&remote_nodes_lock);
    
    // first make sure we have this remote node and a connection to it
    int remote_node = find_remote_node(subscriber_ip, subscriber_port);
    if (remote_node < 0) 
    {
        deros_dbglog_msg_str_int(D_INFO, adres, "publisher", "adding new remote node (ip,port)", subscriber_ip, subscriber_port);
        remote_node = add_remote_node(subscriber_ip, subscriber_port);
        if (remote_node < 0) 
        {
            pthread_mutex_unlock(&remote_nodes_lock);
            return 0;
        }
    }

    // then check publishers of all nodes if they publish to this address, add the node to their remote subscribers list
    for (int pub_i = 0; pub_i < next_publisher_id; pub_i++)
    {
        if (publisher_address[pub_i] == 0) continue;
        if (strcmp(publisher_address[pub_i], adres) == 0)
        {
            int already_subscribed = 0;
            for (int j = 0; j < num_sub_remote_nodes[pub_i]; j++)
            {
                if (subscribed_remote_node_ids[pub_i][j] == remote_node) // already subscribed?
                {
                    already_subscribed = 1;
                    break;
                }
            }
            if (already_subscribed) continue;
            if (num_sub_remote_nodes[pub_i] == 0) 
                subscribed_remote_node_ids[pub_i] = (int *) malloc(sizeof(int));
            else 
                subscribed_remote_node_ids[pub_i] = (int *) realloc(subscribed_remote_node_ids[pub_i], sizeof(int) * (1 + num_sub_remote_nodes[pub_i]));
            if (!subscribed_remote_node_ids[pub_i]) deros_pub_mem_failure("pub new sub");

            subscribed_remote_node_ids[pub_i][num_sub_remote_nodes[pub_i]++] = remote_node;
            s_remote_node_used_by_num_pubs[remote_node]++;
        }
    }

    pthread_mutex_unlock(&remote_nodes_lock);
    return 1;
}

void remove_publisher_from_remote_node(int pub_id, int remote_node)
{
    for (int remote_ind_in_pub = 0; remote_ind_in_pub < num_sub_remote_nodes[pub_id]; remote_ind_in_pub++)
        if (subscribed_remote_node_ids[pub_id][remote_ind_in_pub] == remote_node)
        {
            s_remote_node_used_by_num_pubs[remote_node]--;
            if (s_remote_node_used_by_num_pubs[remote_node] == 0)  // last publisher of this subscriber?
            {
                close(s_remote_node_socket[remote_node]);
                s_remote_node_port[remote_node] = 0;
                s_remote_node_socket[remote_node] = 0;
            }
            subscribed_remote_node_ids[pub_id][remote_ind_in_pub] = subscribed_remote_node_ids[pub_id][--num_sub_remote_nodes[pub_id]];
            if (num_sub_remote_nodes[pub_id])
                subscribed_remote_node_ids[pub_id] = (int *)realloc(subscribed_remote_node_ids[pub_id], sizeof(int) * num_sub_remote_nodes[pub_id]);
            else
            {
                free(subscribed_remote_node_ids[pub_id]);
                subscribed_remote_node_ids[pub_id] = 0;
            }
            break;
        }
}

void publisher_remove_subscriber(int subscriber_port, char *subscriber_ip, char *adres)
{
    pthread_mutex_lock(&remote_nodes_lock);

    int remote_node = find_remote_node(subscriber_ip, subscriber_port);
    if (remote_node < 0)  // we do not have him
    {
        pthread_mutex_unlock(&remote_nodes_lock);
        return;
    }

    for (int pub_i = 0; pub_i < next_publisher_id; pub_i++)
    {
        if (publisher_address[pub_i] == 0) continue;

        if (strcmp(publisher_address[pub_i], adres) == 0)
            remove_publisher_from_remote_node(pub_i, remote_node);
    }

    pthread_mutex_unlock(&remote_nodes_lock);
}

void publisher_unregister(int publisher_id)
{
    if ((publisher_id < 0) || 
        (publisher_id >= next_publisher_id) ||
        (publisher_address[publisher_id] == 0))
    {
        return;
    }

    int node_id = publisher_node_id[publisher_id];
    if (pthread_mutex_lock(&node_mutexes[node_id])) return;

    if ((node_id < 0) || 
        (node_id >= next_free_node_id) ||
        (node_server_sockets[node_id] == 0))
    {
        pthread_mutex_unlock(&node_mutexes[node_id]); 
        return;
    }

    char *adres = publisher_address[publisher_id];

    if (!deros_send_packet(node_server_sockets[node_id], PACKET_PUB_UNREGISTER, (uint8_t *)adres, strlen(adres)))
    {
        deros_dbglog_msg_str(D_ERRR, node_names[node_id], "publisher", "sending unregister publisher packet failed (adr)", adres);
        close(node_server_sockets[node_id]);
        node_server_sockets[node_id] = 0;
    }
    deros_dbglog_msg(D_DEBG, node_names[node_id], "publisher", "sent unregister publisher packet");

    for (int i = 0; i < num_sub_remote_nodes[publisher_id]; i++)
    {
        int remote_node = subscribed_remote_node_ids[publisher_id][i];
        remove_publisher_from_remote_node(publisher_id, remote_node);
    }

    publisher_node_id[publisher_id] = 0;
    free(subscribed_remote_node_ids[publisher_id]);
    subscribed_remote_node_ids[publisher_id] = 0;
    free(publisher_address[publisher_id]);
    publisher_address[publisher_id] = 0;
    num_publishers--;

    pthread_mutex_unlock(&node_mutexes[node_id]);
}

int publisher_log_enable(int publisher_id, int enable)
{
    if ((publisher_id < 0) || (publisher_id > next_publisher_id)) return 0;
    publisher_log_enabled[publisher_id] = enable;

    if (publisher_log_enabled[publisher_id] && !publisher_log_initialized[publisher_id]) 
    {
        publisher_log_initialized[publisher_id] = 1;
        int node_id = publisher_node_id[publisher_id];
        char *prefix = (char *) malloc(strlen(node_names[node_id]) + strlen(publisher_address[publisher_id]) + 15);
        sprintf(prefix, "%s_%s_%d", node_names[node_id], publisher_address[publisher_id], publisher_id);
        publisher_log_handle[publisher_id] = deros_msglog_init(node_log_path[node_id], prefix);
    }
        
    return 1;
}

void publisher_log_prettyprint(int publisher_id, pretty_print_function pretty_printer)
{
    if ((publisher_id < 0) || (publisher_id > next_publisher_id)) return;
    publisher_pretty_printer[publisher_id] = pretty_printer;
}

char *double_pretty_printer(uint8_t *message, int length)
{
    static char dbl[30];
    strncpy(dbl, (char *)message, 29);
    dbl[29] = 0;
    return dbl;
}

char *int_pretty_printer(uint8_t *message, int length)
{
    static char inte[30];
    strncpy(inte, (char *)message, 30);
    inte[29] = 0;
    return inte;
}

