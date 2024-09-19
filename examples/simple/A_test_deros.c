#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../deros.h"

#define PORT_MODULE_A 9332
#define ADDR_OF_A "address_of_A"
#define ADDR_OF_B "address_of_B"

#define FIRST_MESSAGE 123

// change this to some folder that exists:
#define LOG_PATH "/usr/local/smely-zajko-24/logs"

void callback_for_B(uint8_t *message, int length)
{
  printf("node A subscribed to B received new message %d of length %d\n", *((int *)message), length);
}

int main()
{
  printf("module A started\n");
  int my_node_id = deros_init("127.0.0.1", DEFAULT_DEROS_SERVER_PORT, "nodeA", PORT_MODULE_A, LOG_PATH);
  if (my_node_id < 0)
  {
    printf("could not init deros\n");
    return 0;
  }
  printf("module A connected to deros server\n");
  
  int publisher_A = publisher_register(my_node_id, ADDR_OF_A, sizeof(int), 1);
  if (publisher_A < 0)
  {
    printf("could not register publisher A\n");
    deros_done(my_node_id);
    return 0;
  }
  printf("module A registered its publisher\n");

  publisher_log_enable(publisher_A, 1);

  int subscriber_B = subscriber_register(my_node_id, ADDR_OF_B, sizeof(int), callback_for_B, 1);

  if (subscriber_B < 0)
  {
    printf("could not register subscriber of B\n");
    publisher_unregister(publisher_A);
    deros_done(my_node_id);
    return 0;
  }
  printf("module A registered its subscriber\n");

  printf("sleeping 5 seconds...\n");
  sleep(5);
  int msg = FIRST_MESSAGE;
  for (int i = 0; i < 5; i++)
  {
    if (!publish(publisher_A, (uint8_t *) &msg, sizeof(int)))
    {
      printf("could not publish message %d at address %s\n", msg, ADDR_OF_A);
      break;
    }
    printf("module A published a message %d\n", msg);
    sleep(1);
    msg++;
  }

  printf("sleeping 5 seconds...\n");
  sleep(5);

  printf("module A unregistering subscriber and publisher\n");
  subscriber_unregister(subscriber_B);
  publisher_unregister(publisher_A);
  
  printf("module A closing deros\n");
  deros_done(my_node_id);

  printf("module A done.\n");
  return 0;
}
