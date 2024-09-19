// implementation of useful socket communication functions for connecting TCP sockets, and sending/receiving packets demarked with packet length and type

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "deros_dbglog.h"

/** try to connect to a socket server at specified IP:port
 * @param  server  IP address of the server
 * @param  port    TCP/IP port where the server is listening 
 * @return  connected socket, or 0 if connection could not be made */
int deros_connect_to_server(char *server, int port)
{
    int sock;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    { 
        deros_dbglog_msg_int(D_ERRR, "net", "common", "socket error", errno); 
        return 0; 
    } 
    deros_dbglog_msg(D_DEBG, "net", "common", "socket created");

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    int convert_result = inet_pton(AF_INET, server, &serv_addr.sin_addr);
    if (convert_result == 0)
    {
        deros_dbglog_msg_str(D_ERRR, "net", "common", "address not recognized", server);
        return 0;
    }
    else if (convert_result < 0)
    { 
        deros_dbglog_msg(D_ERRR, "net", "common", "server address error"); 
        return 0; 
    } 
    deros_dbglog_msg(D_DEBG, "net", "common", "address converted");
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        deros_dbglog_msg(D_ERRR, "net", "common", "connect failed"); 
        return 0; 
    } 
    deros_dbglog_msg_str_int(D_INFO, "net", "common", "server connected", server, port);

    return sock;
}

/** convert unsigned int to C string */
void deros_store_uint(uint8_t *buffer, unsigned int x)
{
    for (int i = 0; i < sizeof(unsigned int); i++)
    {
        buffer[i] = x & 255;
        x >>= 8;
    }
}

/** convert C string to unsigned int */
void deros_retrieve_uint(uint8_t *buffer, unsigned int *x)
{
    int i = sizeof(unsigned int);
    *x = 0;
    while (i--)
    {
        *x <<= 8;
        *x += buffer[i];
    }
}

/** send a packet to a connected TCP/IP peer over the specified socket 
 *  @param socket  an open socket to send it to
 *  @param packet_type  one-byte number manifesting the type of the packet
 *  @param buffer  binary contents of the packet
 *  @param size  length of the packet in bytes 
 *  @return  if sending was successful, returns 1, otherwise 0 */
int deros_send_packet(int socket, uint8_t packet_type, uint8_t *buffer, unsigned int size)
{
    unsigned int count = size;
    uint8_t len[sizeof(unsigned int) + 1];

    deros_store_uint(len, count);

    len[sizeof(unsigned int)] = packet_type;

    if (send(socket, len, sizeof(unsigned int) + 1, 0) < 0)
        return 0;

    if (send(socket, buffer, size, 0) < 0)
        return 0;

    return 1;
}

/** wait for a packet arriving from the spcified socket, and store it to a buffer up to the maximum size specified in bytes
 * @param socket  an open TCP/IP socket on which to wait for the message
 * @param buffer  an array in memory where the incoming packet should be stored
 * @param size    expected size of the packet
 * @param maxsize  how much space can be safely stored into the buffer
 * @return the type of the packete that was received (1 byte), or 0 on error */ 
uint8_t deros_receive_packet(int socket, uint8_t *buffer, int *size, unsigned int maxsize)
{
    uint8_t len[sizeof(unsigned int) + 1];
    int i = 0;
    *size = 0;

    while (i < sizeof(unsigned int) + 1)
    {
        int nread = recv(socket, len + i, sizeof(unsigned int) + 1 - i, 0);
        if (nread <= 0) return 0;
        i += nread;
    }
    uint8_t packet_type = len[--i];

    deros_retrieve_uint(len, (unsigned int *)size);
    if (*size > maxsize) return 0;

    i = 0;
    while (i < *size)
    {
        int nread = recv(socket, buffer + i, *size - i, 0);
        if (nread <= 0) return 0;
        i += nread;
    }
    return packet_type;
}

/** will create a TCP/IP server socket, bind it to the specified port, and prepare it for listening for connections 
 * @param port  the port where this server socket will be accepting connections - after another function is called
 * @return  if setup is successful, returns the server socket descriptor, otherwise returns 0 */
int deros_create_server(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) 
    {
        deros_dbglog_msg_int(D_ERRR, "net", "common", "socket failed", errno);
        return 0;
    }
    deros_dbglog_msg(D_DEBG, "net", "common", "socket created");

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        deros_dbglog_msg_int(D_ERRR, "net", "common", "setsockopt", errno);
        return 0;
    }
    deros_dbglog_msg(D_DEBG, "net", "common", "options set");
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr *)&address, addrlen) < 0) 
    { 
        deros_dbglog_msg_int(D_ERRR, "net", "common", "bind failed", errno);
        return 0;
    } 
    deros_dbglog_msg(D_INFO, "net", "common", "socket bound to address");

    deros_dbglog_msg(D_DEBG, "net", "common", "listening...");
    if (listen(server_fd, 3) < 0) 
    { 
        deros_dbglog_msg_int(D_ERRR, "net", "common", "listen", errno);
        return 0;
    }  
    return server_fd;
}

/** after the server socket was created, we can start accepting connections
 * @param server_fd  server socket descriptor created by the previous function
 * @return  after a connection to a newly accepted node is established, the new TCP socket for communication with the new peer is returned, 0 is returned on error */
int deros_wait_for_client_connection(int server_fd)
{
    deros_dbglog_msg(D_DEBG, "net", "common", "accepting");
    int new_socket;
    if ((new_socket = accept(server_fd, 0, 0)) < 0)
    {
        deros_dbglog_msg_int(D_ERRR, "net", "common", "accept", errno);
	    return 0;
    }
    return new_socket;
}

