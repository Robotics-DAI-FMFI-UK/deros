#ifndef __DEROS_COMMON_H__
#define __DEROS_COMMON_H__

// some definitions shared between the server and client nodes

#define MAX_NODES 100

#define MAX_PACKET_LENGTH       (10*1024*1024)
#define PACKET_INIT              1
#define PACKET_RESPONSE_INIT     2
#define PACKET_DONE              3
#define PACKET_PUB_REGISTER      4
#define PACKET_PUB_UNREGISTER    5
#define PACKET_SUB_REGISTER      6
#define PACKET_SUB_UNREGISTER    7
#define PACKET_ADD_SUBSCRIBER    8
#define PACKET_REMOVE_SUBSCRIBER 9
#define PACKET_NEW_MESSAGE       10


#define INIT_MSG_HEADER     "deros?"
#define INIT_MSG_RESPONSE   "deros!"

#define MAX_NUM_PUBLISHERS         1000
#define MAX_NUM_SUBSCRIBERS        1000
#define MAX_NUM_REMOTE_SUBSCRIBERS  200

#endif
