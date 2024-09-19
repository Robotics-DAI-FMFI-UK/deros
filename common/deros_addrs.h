#ifndef __DEROS_ADDRS_H__
#define __DEROS_ADDRS_H__

// structures and methods used to maintain list of used deros addresses

#define MAX_NUM_ADDRESSES 1000

extern char *addresses[MAX_NUM_ADDRESSES];
extern int addr[MAX_NUM_ADDRESSES];
extern int num_addresses;
extern int *addr_subscribers[MAX_NUM_ADDRESSES];
extern int addr_num_sub[MAX_NUM_ADDRESSES];

// returns position in the index array, not the addr id itself
int find_address(char *adr, int *found);

void insert_address_at_index(char *address, int at_index);

void init_address_of_subscriber(int adr_id, int sub_id);
void add_subscriber_to_address(int adr_id, int sub_id);
void remove_subscriber_from_address(int adr_id, int sub_id);



#endif
