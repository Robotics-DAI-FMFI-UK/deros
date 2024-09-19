#ifndef __DEROS_H__
#define __DEROS_H__

#include <inttypes.h>

#define DEFAULT_DEROS_SERVER_PORT  9342
#define MAX_ADDRESS_LENGTH 100


#define VARIABLE_SIZE_MESSAGE -1

/** defines callback function type for pretty printing message bodies into message log */
typedef char *(*pretty_print_function)(uint8_t *message, int length);

/** defines callback function type for receiving message from subscribed addresses */
typedef void (*subscriber_callback_function)(uint8_t *message, int length);

// API for client nodes

/** each program that wants to use the framework should initialize it first, specify the server IP and port,
 *  its name (just for logging purposes) a port on which Deros will listen for publishers to push messages
 *  from subscribed addresses, and a path where log files can be created 
 *  @return  ID of this node - keep it for interfacing with Deros further on, one program can potentially register multiple nodes */
int deros_init(char *server_address, int server_port, char *node_name, int listen_port, char *log_path);

/** when Deros communication is not needed anymore, program can call deros_done(), it can later
 * initialize it over agin with deros_init() */
void deros_done(int node_id);

// API for the publishers:

/** register a new publisher on the server, if subscribers to the same address are already registered, this publisher will be immediatelly notified to open 
 *  connections for pushing messages to those subscribers, later arriving subscribers will also be automatically connected.
 *  @param node_id  id returned by deros_init()
 *  @param address  string containing the address where messages will be published (such as "lidar")
 *  @param message_size  Deros messages are typically of a fixed size (when the size do not match, error is reporter), but -1 allows for variable-length messages
 *  @param message_queue_size  currently, there are no message queues and messages are delivered instantly, reserved for future versions 
 *  @return  ID of publisher or -1 on error 
 */
int publisher_register(int node_id, char *address, int message_size, int message_queue_size);

/** send message to a specified address, i.e. to all nodes that subscribed to this address - their callbacks will be called with the message delivered
 *  @return  if successful returns 1, otherwise 0 */
int publish(int publisher_id, uint8_t *message, int msg_len);

/** remove this publisher from the server - if any subscribers are found on the same address, connection for pushing messages to them is closed */
void publisher_unregister(int publisher_id);

/** enable or disable logging messages of the specified publisher into message log 
 *  @return  1 on success, 0 if publisher is not known */
int publisher_log_enable(int publisher_id, int enable);

/** setup a pretty print formatting function for logging messages into message log for the specified publisher - the function should return a static string,
 *  it will not be deallocated by Deros, the pretty print function should take care, it will not be called from the same node multiple times at once. */
void publisher_log_prettyprint(int publisher_id, char *(*pretty_printer)(uint8_t *message, int length));

/** sample pretty printer function that formats single binary 32-bit integer message */
char *double_pretty_printer(uint8_t *message, int length);

/** sample pretty printer function that formats single binary 64-bit double message */
char *int_pretty_printer(uint8_t *message, int length);

// API for subscribers

/** register a new subscriber on the server - all publishers on the same address are automatically notified to make connections for pushing messages,
 *  publishers arriving later will also automatically connect 
 *  @param node_id  id returned by deros_init() 
 *  @param address  string containing the address from which messages are automatically deilivered
 *  @param message_size  should match the message size of the publisher, or -1 for variable-length messages
 *  @param msg_queue_size  currently messages are never queued, they are delivered immediately (queue size 1), reserved for future use */
int subscriber_register(int node_id, char *address, int message_size, subscriber_callback_function callback, int msg_queue_size);

/** remove this subscriber from the server - if any publishers are found on the same address, they will automatically close their connections to this subscriber */
void subscriber_unregister(int subscriber_id);

#endif
