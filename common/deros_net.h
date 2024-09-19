#ifndef __DEROS_NET_H__
#define __DEROS_NET_H__

// defining the API for the useful TCP/IP communication module

#include <inttypes.h>

int deros_connect_to_server(char *server, int port);
int deros_send_packet(int socket, uint8_t packet_type, uint8_t *buffer, unsigned int size);
uint8_t deros_receive_packet(int socket, uint8_t *buffer, int *size, unsigned int maxsize);
int deros_create_server(int port);
int deros_wait_for_client_connection(int server_fd);

void deros_store_uint(uint8_t *buffer, unsigned int x);
void deros_retrieve_uint(uint8_t *buffer, unsigned int *x);


#endif

