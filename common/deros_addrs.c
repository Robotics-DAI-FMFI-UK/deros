// maintenance of list of known deros addresses - their fast lookup using binary search

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "deros_addrs.h"
#include "deros_dbglog.h"

char *addresses[MAX_NUM_ADDRESSES];
int addr[MAX_NUM_ADDRESSES];
int num_addresses;
int *addr_subscribers[MAX_NUM_ADDRESSES];
int addr_num_sub[MAX_NUM_ADDRESSES];


void addr_mem_failure(char *msg)
{
    deros_dbglog_msg_str(D_GRRR, "addr", "common", "not enough memory", msg);
    exit(1);
}

// returns position in the index array, not the addr id itself
int find_address(char *adr, int *found)
{
    int l = 0, h = num_addresses;
    while (l != h) 
    {
        int mid = (l + h) / 2;
        int cmpmid = strcmp(adr, addresses[addr[mid]]);
        if (cmpmid > 0) l = mid + 1;
        else if (cmpmid < 0) h = mid;
        else 
        {
            if (found) *found = 1;
            deros_dbglog_msg_str_2int(D_DEBG, "addr", "common", "find addr (1) => found (2) index (3)", adr, *found, mid);
            return mid;
        }
    }  
    if (found) *found = 0;
    deros_dbglog_msg_str_2int(D_DEBG, "addr", "common", "find addr (1) => found (2) index (3)", adr, *found, l);
    return l;
}

void insert_address_at_index(char *address, int at_index)
{
    char *adr = (char *) malloc(strlen(address) + 1);
    if (!adr) addr_mem_failure("insert addr");
    strcpy(adr, address);
    addresses[num_addresses] = adr;
    for (int i = num_addresses; i > at_index; i--)
       addr[i] = addr[i - 1];
    addr[at_index] = num_addresses;
    num_addresses++;
}

void init_address_of_subscriber(int adr_id, int sub_id)
{
    addr_subscribers[sub_id] = (int *) malloc(sizeof(int) * 1);
    if (!addr_subscribers[sub_id]) addr_mem_failure("init addr");
    addr_subscribers[sub_id][0] = sub_id;
    addr_num_sub[adr_id] = 1;
}

void add_subscriber_to_address(int adr_id, int sub_id)
{
    int ind = addr_num_sub[adr_id]++;
    addr_subscribers[adr_id] = (int *) realloc(addr_subscribers[adr_id], sizeof(int) * addr_num_sub[adr_id]);
    if (!addr_subscribers[adr_id]) addr_mem_failure("add addr");
    addr_subscribers[adr_id][ind] = sub_id;
}

void remove_subscriber_from_address(int adr_id, int sub_id)
{
    for (int i = 0; i < addr_num_sub[adr_id]; i++)
        if (addr_subscribers[adr_id][i] == sub_id)
        {
            addr_subscribers[adr_id][i] = addr_subscribers[adr_id][--addr_num_sub[adr_id]];
            break;
        }
}
