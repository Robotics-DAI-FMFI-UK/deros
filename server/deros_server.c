// implementation of the Deros server - it waits for connections from client nodes and maintains the list of subscribers and publishers for their specified addresses, 
// publishers are automatically notified about their subscribers so that publishers make direct connections to subscribers for pushing the new messages in peer-to-peer
// communication

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "../deros.h"
#include "../common/deros_common.h"
#include "../common/deros_net.h"
#include "../common/deros_addrs.h"
#include "../common/deros_dbglog.h"
#include "deros_server.h"

static int deros_port = DEFAULT_DEROS_SERVER_PORT;
static char *log_path = DEFAULT_LOG_PATH;

static pthread_mutex_t deros_server_lock;
static int deros_server_socket;

static int client_sockets[MAX_NUM_CLIENTS];
static char *client_node_names[MAX_NUM_CLIENTS]; 
static char *client_ip[MAX_NUM_CLIENTS];
static int client_port[MAX_NUM_CLIENTS];
static int client_to_be_removed[MAX_NUM_CLIENTS];
static int next_client_id = 0;
static int num_clients = 0;

static int publisher_client[MAX_NUM_PUBLISHERS];
static int publisher_msgsize[MAX_NUM_PUBLISHERS];
static int publisher_address[MAX_NUM_PUBLISHERS];
static int publisher_count[MAX_NUM_PUBLISHERS];
static int num_publishers;   // count > 1 is counted only once here

static int subscriber_client[MAX_NUM_SUBSCRIBERS];
static int subscriber_msgsize[MAX_NUM_SUBSCRIBERS];
static int subscriber_address[MAX_NUM_SUBSCRIBERS];
static int subscriber_count[MAX_NUM_SUBSCRIBERS];
static int num_subscribers;  // count > 1 is counted only once here

static volatile int client_thread_started;
static int new_client_socket;
static volatile int server_running;

/** parsing the server command line, same arguments as the main() server function */
void process_arguments(int argc, char **argv)
{
    int will_exit = 0;
  
    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--help", 6) == 0)
        {
            printf("usage: deros_server [--help] [--port TCP_PORT] [--logpath PATH]\n");
            will_exit = 1;
        }
        else if (strncmp(argv[i], "--port", 6) == 0)
        {
            sscanf(argv[++i], "%d", &deros_port);
            if ((deros_port < 1024) || (deros_port > 65535))
            {
                deros_dbglog_msg_int(D_ERRR, "server", "args", "port number out of range", deros_port);
                will_exit = 1;
            }
        }
        else if (strncmp(argv[i], "--logpath", 9) == 0)
        {
            log_path = argv[++i];
        }
    }

    if (will_exit) exit(0);
}

/** common treatment of 0 returned from malloc() */
void mem_failure()
{
    deros_dbglog_msg(D_GRRR, "server", "memf", "deros_server: not enough memory");
    exit(1);
}

/** internal function to update data structures when publisher is leaving the server */
void remove_publisher(int id_publisher)
{
    publisher_client[id_publisher] = publisher_client[num_publishers - 1];
    publisher_msgsize[id_publisher] = publisher_msgsize[num_publishers - 1];
    publisher_address[id_publisher] = publisher_address[num_publishers - 1];
    publisher_count[id_publisher] = publisher_count[num_publishers - 1];
    num_publishers--;
}

/** subscriber has just left, its publisher is being notified to close connection to the original subscriber node */
void notify_publisher_of_removed_subscriber(int id_publisher, int id_subscriber)
{
    int id_client = subscriber_client[id_subscriber];
    int publisher_socket = client_sockets[publisher_client[id_publisher]];

    char *packet = (char *)malloc(strlen(addresses[subscriber_address[id_subscriber]]) + 2 + 15 + 5 + 1);
    sprintf(packet, "%d!%s!%s", client_port[id_client], client_ip[id_client], addresses[subscriber_address[id_subscriber]]);

    if (!deros_send_packet(publisher_socket, PACKET_REMOVE_SUBSCRIBER, (uint8_t *)packet, strlen(packet)))
    {
        deros_dbglog_msg(D_WARN, "server", "remsub", "deros_server: could not send remove subscriber to publisher");
        if (publisher_client[id_publisher] != subscriber_client[id_subscriber])
            client_to_be_removed[publisher_client[id_publisher]] = 1;
    }
}

/** subscriber has just left, all publishers should be notified to close their connections to the leaving subscriber node */
void notify_all_publishers_of_removed_subscriber(int id_subscriber)
{
    for (int i = 0; i < num_publishers; i++)
        if (publisher_address[i] == subscriber_address[id_subscriber]) 
            notify_publisher_of_removed_subscriber(i, id_subscriber);
}

/** internal update of data structures when subscriber is removed */
void remove_subscriber(int id_subscriber)
{
    subscriber_client[id_subscriber] = subscriber_client[num_subscribers - 1];
    subscriber_msgsize[id_subscriber] = subscriber_msgsize[num_subscribers - 1];
    subscriber_address[id_subscriber] = subscriber_address[num_subscribers - 1];
    subscriber_count[id_subscriber] = subscriber_count[num_subscribers - 1];
    num_subscribers--;
}

/** internal update of data structures when the whole node is leaving - all subscribers and publishers of this node should clean up */
void remove_node(int node_id)
{
    pthread_mutex_lock(&deros_server_lock);

    if (client_sockets[node_id] == 0) return; // already removed

    for (int i = 0; i < num_subscribers; i++)
        if (subscriber_client[i] == node_id)
        {
            notify_all_publishers_of_removed_subscriber(i);
            remove_subscriber(i); 
        }

    for (int i = 0; i < num_publishers; i++)
        if (publisher_client[i] == node_id)
            remove_publisher(i);

    close(client_sockets[node_id]);
    client_sockets[node_id] = 0;
    free(client_node_names[node_id]);
    free(client_ip[node_id]);
    client_port[node_id] = 0;
    client_to_be_removed[node_id] = 0;

    num_clients--;
    pthread_mutex_unlock(&deros_server_lock);
}

/** if a message cannot be sent to the subscribers, they are put on hold for possible later reconnection */
void remove_unaccessible_clients()
{
    for (int i = 0; i < next_client_id; i++)
        if (client_sockets[i]) continue;
        else if (client_to_be_removed[i]) 
        {
            remove_node(i);
            i = -1;  // start over, maybe some new node is to be removed as a consequence
        }
}

/** Deros does allow multiple publishers to the same address from the same node, but handles that just by a counter */
int if_publisher_from_this_node_exists_only_increment_counter(int node_id, int msgsize, int id_addr)
{
    for (int i = 0; i < num_publishers; i++)
    {
        if ((publisher_client[i] == node_id) &&
            (publisher_address[i] == id_addr))
        {
            if (publisher_msgsize[i] != msgsize)
            {
                deros_dbglog_msg_str_2int(D_GRRR, "server", "chkpub", "deros register new publisher with wrong msgsize (adr, node_id, msgsize)", addresses[id_addr], node_id, msgsize);
                exit(1);
            }
            publisher_count[i]++;
            return 1;
        }
    }
    return 0;
}

/** Doers does allow multiple subscribers of the same address from the same node, but hadnles that jsut by a counter */
int if_subscriber_from_this_node_exists_only_increment_counter(int node_id, int msgsize, int id_addr)
{
    for (int i = 0; i < num_subscribers; i++)
    {
        if ((subscriber_client[i] == node_id) &&
            (subscriber_address[i] == id_addr))
        {
            if (subscriber_msgsize[i] != msgsize)
            {
                deros_dbglog_msg_str_2int(D_GRRR, "server", "chksub", "deros register new subscriber with wrong msgsize (adr, node_id, msgsize)", addresses[id_addr], node_id, msgsize);
                exit(1);
            }
            subscriber_count[i]++;
            return 1;
        }
    }
    return 0;
}

/** internal communication to notify a publisher about its subscriber */
void send_subscriber_to_publisher(int id_publisher, int id_sub_node)
{
    int publisher_socket = client_sockets[publisher_client[id_publisher]];
    char *adres = addresses[publisher_address[id_publisher]];

    char *packet = (char *) malloc(15 + 5 + 2 + strlen(adres) + 1);
    sprintf(packet, "%d!%s!%s", client_port[id_sub_node], client_ip[id_sub_node], adres);
                    
    if (!deros_send_packet(publisher_socket, PACKET_ADD_SUBSCRIBER, (uint8_t *)packet, strlen(packet)))
    {
        deros_dbglog_msg(D_WARN, "server", "newsub", "deros_server: could not send new subscriber to publisher");
        client_to_be_removed[publisher_client[id_publisher]] = 1;
    }

    free(packet);
}

/** internal communication to notify a publisher about all of its subscribers, one by one (after publisher just arrived) */
void send_all_subscribers_to_publisher(int id_publisher)
{
    int adr = publisher_address[id_publisher];
    for (int i = 0; i < num_subscribers; i++)
    {
        if (subscriber_address[i] == adr)
            send_subscriber_to_publisher(id_publisher, subscriber_client[i]);
    }    
}

/** process packet of new publisher arriving */
void register_new_publisher(int node_id, uint8_t *packet, int size)
{
    packet[size] = 0;
    char *exclpos = strchr((char *)packet, '!');
    if (exclpos == 0)
    {
        deros_dbglog_msg(D_ERRR, "server", "regpub", "malformatted PUB_REGISTER packet");
        return;
    }
    *exclpos = 0;
    int msgsize;
    sscanf((char *)packet, "%d", &msgsize);

    int found;

    pthread_mutex_lock(&deros_server_lock);

    int id_addr = find_address(exclpos + 1, &found);
    if (!found) insert_address_at_index(exclpos + 1, id_addr);
    id_addr = addr[id_addr];  // now it is addr id
    deros_dbglog_msg_int(D_DEBG, "server", "regpub", "actual addr id = ", id_addr);

    if (if_publisher_from_this_node_exists_only_increment_counter(node_id, msgsize, id_addr)) 
    { 
        pthread_mutex_unlock(&deros_server_lock);
        return;
    }
    
    publisher_client[num_publishers] = node_id;
    publisher_msgsize[num_publishers] = msgsize;
    publisher_address[num_publishers] = id_addr;
    publisher_count[num_publishers] = 1;
    num_publishers++;

    send_all_subscribers_to_publisher(num_publishers - 1);
    id_addr = addr[id_addr];  // now it is addr id
    deros_dbglog_msg_2str(D_INFO, "server", "regpub", "registered publisher (from, address)", client_node_names[node_id], addresses[id_addr]);
    pthread_mutex_unlock(&deros_server_lock);
}

/** process packet of publisher leaving */
void unregister_publisher(int node_id, uint8_t *packet, int size)
{
    packet[size] = 0;

    pthread_mutex_lock(&deros_server_lock);
    int found;
    int id_addr = find_address((char *)packet, &found);
    if (!found)
    {
        deros_dbglog_msg_str(D_ERRR, "server", "unregpub", "deros_server: unregister_publisher() with unknown address from node", client_node_names[node_id]);
        pthread_mutex_unlock(&deros_server_lock);
        return;
    }
    id_addr = addr[id_addr]; // now it is address id
    for (int i = 0; i < num_publishers; i++)
        if ((publisher_client[i] == node_id) &&
            (publisher_address[i] == id_addr))
        {
            if (publisher_count[i] == 1)
               remove_publisher(i);
            else publisher_count[i]--;
            deros_dbglog_msg_2str(D_INFO, "server", "unregpub", "unregistered publisher (from, address)", client_node_names[node_id], addresses[id_addr]);
            break;
        }
    pthread_mutex_unlock(&deros_server_lock);
}

/** if subscriber arrived, we need to notify all publishers */
void send_a_new_subscriber_to_all_publishers(int sub_node_id, int id_addr, int msgsize)
{
    for (int i = 0; i < num_publishers; i++)
    {
        if (publisher_address[i] == id_addr)
        {
            if (publisher_msgsize[i] != msgsize)
            {
                deros_dbglog_msg_2str_int(D_GRRR, "server", "newsub", "deros server: new subscriber with an incompatible msg size from client (node,adr,size)", client_node_names[sub_node_id], addresses[id_addr], msgsize);
                exit(1);
            }
            send_subscriber_to_publisher(i, sub_node_id);
        }
    }
}

/** new subscriber just arrived, process its packet */
void register_new_subscriber(int node_id, uint8_t *packet, int size)
{
    packet[size] = 0;
    char *exclpos = strchr((char *)packet, '!');
    if (exclpos == 0)
    {
        deros_dbglog_msg(D_ERRR, "server", "regsub", "malformatted SUB_REGISTER packet");
        return;
    }
    *exclpos = 0;
    int msgsize;
    sscanf((char *)packet, "%d", &msgsize);

    int found;

    pthread_mutex_lock(&deros_server_lock);

    int id_addr = find_address(exclpos + 1, &found);
    if (!found) insert_address_at_index(exclpos + 1, id_addr);

    id_addr = addr[id_addr];  // now it is address id
    deros_dbglog_msg_int(D_DEBG, "server", "regsub", "actual addr id =", id_addr);

    if (if_subscriber_from_this_node_exists_only_increment_counter(node_id, msgsize, id_addr)) 
    { 
        pthread_mutex_unlock(&deros_server_lock);
        return;
    }
    
    subscriber_client[num_subscribers] = node_id;
    subscriber_msgsize[num_subscribers] = msgsize;
    subscriber_address[num_subscribers] = id_addr;
    subscriber_count[num_subscribers] = 1;
    num_subscribers++;

    send_a_new_subscriber_to_all_publishers(node_id, id_addr, msgsize);
    pthread_mutex_unlock(&deros_server_lock);
    deros_dbglog_msg_2str(D_INFO, "server", "regsub", "registered subscriber (from, address)", client_node_names[node_id], addresses[id_addr]);
}

/* a subscriber has left, process its packet */
void unregister_subscriber(int node_id, uint8_t *packet, int size)
{
    packet[size] = 0;

    pthread_mutex_lock(&deros_server_lock);
    int found;
    int id_addr = find_address((char *)packet, &found);
    if (!found)
    {
        deros_dbglog_msg_str(D_WARN, "server", "unregsub", "deros_server: unregister_subscriber with unknown address from node", client_node_names[node_id]);
        pthread_mutex_unlock(&deros_server_lock);
        return;
    }
    for (int i = 0; i < num_subscribers; i++)
        if ((subscriber_client[i] == node_id) &&
            (subscriber_address[i] == id_addr))
        {
            if (subscriber_count[i] == 1)
            {
               notify_all_publishers_of_removed_subscriber(i);
               remove_subscriber(i);
            }
            else subscriber_count[i]--;
            deros_dbglog_msg_2str(D_WARN, "server", "unregsub", "unregistered subscriber (from,address)", client_node_names[node_id], addresses[id_addr]);
            break;
        }
    pthread_mutex_unlock(&deros_server_lock);
}

/** a new packet has arrived from client node, do a respective packet handling */
int process_client_packet(int node_id, uint8_t packet_type, uint8_t *my_buffer, int packet_size)
{
    deros_dbglog_msg_3int(D_DEBG, "server", "newpak", "packet from node of size (type,nodeid,size)", packet_type, node_id, packet_size); 
    switch (packet_type) 
    {
        case PACKET_DONE: // client node terminates: remove it from lists of publishers and subscribers, and from the list of nodes
                          remove_node(node_id);
                          return 0;  // this will terminate this node reading thread
        case PACKET_PUB_REGISTER: register_new_publisher(node_id, my_buffer, packet_size);
                                  break;
        case PACKET_PUB_UNREGISTER: unregister_publisher(node_id, my_buffer, packet_size);
                                    break;
        case PACKET_SUB_REGISTER: register_new_subscriber(node_id, my_buffer, packet_size);
                                  break;
        case PACKET_SUB_UNREGISTER: unregister_subscriber(node_id, my_buffer, packet_size);
                                    break;
    }
    remove_unaccessible_clients();
    return 1;
}

/* CLIENT -> SERVER protocol:
 *
 * 1. PACKET_INIT                (deros?name!listen_port)
 * 2. PACKET_DONE                ()
 * 3. PACKET_PUB_REGISTER        (msg_size!address)
 * 4. PACKET_PUB_UNREGISTER      (address)
 * 5. PACKET_SUB_REGISTER        (msg_size!address)
 * 6. PACKET_SUB_UNREGISTER      (address)
 *
 * SERVER -> CLIENT protocol:
 *
 * 1. PACKET_RESPONSE_INIT       (deros!name)
 * 2. PACKET_ADD_SUBSCRIBER      (port!ip!address)   // sent to publisher for each [new] subscriber
 * 3. PACKET_REMOVE_SUBSCRIBER   (port!ip!address)   // sent to publisher
 *
 * CLIENT -> SUBSCRIBER protocol:
 *
 * 1. PACKET_NEW_MESSAGE         (address!len!message)
 *
 */

/** retrieves an unused client id */
int find_new_client_id()
{
    int id = 0;
    for (id = 0; id < next_client_id; id++)
        if (client_sockets[id] == 0) 
            return id;
    return next_client_id++;
}

/** after a client (publisher/subscriber) registers on the server, it will get a separate thread to deal with it */
void *client_thread(void *arg)
{
    int my_socket = new_client_socket;
    client_thread_started = 1;

    uint8_t *my_buffer = (uint8_t *)malloc(MAX_PACKET_LENGTH + 1);
    if (my_buffer == 0) mem_failure();

    int packet_size = 0;
    uint8_t packet_type = deros_receive_packet(my_socket, my_buffer, &packet_size, MAX_PACKET_LENGTH);
    my_buffer[packet_size] = 0;
    char *exclpos = strchr((char *)(my_buffer + strlen(INIT_MSG_HEADER)), '!');

    if ((packet_type != PACKET_INIT) || (packet_size < strlen(INIT_MSG_HEADER) + 3) || 
        (strncmp(INIT_MSG_HEADER, (char *)my_buffer, strlen(INIT_MSG_HEADER)) != 0) || (exclpos == 0))
    {
        deros_dbglog_msg_2int(D_ERRR, "server", "clithr", "deros_server: node has not logged in properly, (packet_type,packet_size)", packet_type, packet_size);
        return 0;
    }   

    int name_length = (exclpos - (char *)my_buffer) - strlen(INIT_MSG_HEADER);
    char *my_node_name = (char *)malloc(name_length + 1);
    if (my_node_name == 0) mem_failure();
    strncpy(my_node_name, (char *)(my_buffer + strlen(INIT_MSG_HEADER)), name_length);
    my_node_name[name_length] = 0;
    sprintf((char *)my_buffer, "%s%s", INIT_MSG_RESPONSE, my_node_name);
    if (!deros_send_packet(my_socket, PACKET_RESPONSE_INIT, my_buffer, strlen(INIT_MSG_RESPONSE) + name_length))
    {
        deros_dbglog_msg(D_ERRR, "server", "clithr", "deros_server: could not respond to login from node");
        return 0;
    }

    pthread_mutex_lock(&deros_server_lock);
    int new_client_id = find_new_client_id();
    sscanf(exclpos + 1, "%d", &client_port[new_client_id]);

    struct sockaddr_in peer_addr;
    socklen_t peer_adr_len = sizeof(peer_addr);
    getpeername(my_socket, (struct sockaddr*)&peer_addr, &peer_adr_len);
    client_ip[new_client_id] = (char *) malloc(20);
    strncpy(client_ip[new_client_id], inet_ntoa(peer_addr.sin_addr), 19);
    client_ip[new_client_id][20] = 0;

    client_sockets[new_client_id] = my_socket;
    client_node_names[new_client_id] = my_node_name;
    int my_node_id = new_client_id;
    num_clients++;
    pthread_mutex_unlock(&deros_server_lock);

    while (1)
    {
        packet_type = deros_receive_packet(my_socket, my_buffer, &packet_size, MAX_PACKET_LENGTH);
        if (!packet_type) break;
        if (!process_client_packet(my_node_id, packet_type, my_buffer, packet_size)) break;
    }

    deros_dbglog_msg_str(D_INFO, "server", "clithr", "deros_server: node disconnected", client_node_names[my_node_id]);

    remove_node(my_node_id);
    free(my_buffer);
    return 0;
}

/** the main thread that accepts new connections from client nodes */
void deros_server_accepting_thread()
{
    server_running = 1;
    do {
        new_client_socket = deros_wait_for_client_connection(deros_server_socket);
        if (new_client_socket == 0) continue;

        client_thread_started = 0;

        pthread_t thr;
        pthread_create(&thr, 0, client_thread, 0);

        while (!client_thread_started) usleep(1);

    } while (server_running);
}

/** main program entry */
int main(int argc, char  **argv)
{
    if (argc > 1) process_arguments(argc, argv);

    pthread_mutex_init(&deros_server_lock, 0);

    deros_dbglog_init(log_path, "server", 5, deros_dbg_levels);

    deros_dbglog_msg_int(D_INFO, "server", "main", "starting deros server on port", deros_port);
    deros_server_socket = deros_create_server(deros_port);

    deros_server_accepting_thread();

    close(deros_server_socket);

    deros_dbglog_msg(D_INFO, "server", "main", "deros_server terminates.");
    exit(0);
}
