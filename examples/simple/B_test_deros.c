#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../../deros.h"

#define PORT_MODULE_B 9333
#define ADDR_OF_A "address_of_A"
#define ADDR_OF_B "address_of_B"

#define FIRST_MESSAGE 456

// change this to some folder that exists
#define LOG_PATH "/usr/local/smely-zajko-24/logs"

void callback_for_A(uint8_t *message, int length)
{
  printf("node B subscribed to A received new message %d of length %d\n", *((int *)message), length);
}

int main(int argc, char **argv)
{
  char *log_path = LOG_PATH;
  if ((argc > 2) && (strcmp(argv[1], "--logpath") == 0)) log_path = argv[2];

  printf("module B started\n");
  int my_node_id = deros_init("127.0.0.1", DEFAULT_DEROS_SERVER_PORT, "nodeB", PORT_MODULE_B, log_path);
  if (my_node_id < 0)
  {
    printf("could not init deros\n");
    return 0;
  }
  printf("module B connected to deros server\n");

  
  int publisher_B = publisher_register(my_node_id, ADDR_OF_B, sizeof(int), 1);
  if (publisher_B < 0)
  {
    printf("could not register publisher B\n");
    deros_done(my_node_id);
    return 0;
  }
  printf("module B registered its publisher\n");

  int subscriber_A = subscriber_register(my_node_id, ADDR_OF_A, sizeof(int), callback_for_A, 1);
  if (subscriber_A < 0)
  {
    printf("could not register subscriber of B\n");
    publisher_unregister(publisher_B);
    deros_done(my_node_id);
    return 0;
  }
  printf("module B registered its subscriber\n");

  printf("sleeping 5 seconds...\n");
  sleep(5);

  int msg = FIRST_MESSAGE;
  for (int i = 0; i < 5; i++)
  {
    if (!publish(publisher_B, (uint8_t *) &msg, sizeof(int)))
    {
      printf("could not publish message %d at address %s\n", msg, ADDR_OF_B);
      break;
    }
    printf("module B published a message %d\n", msg);
    sleep(1);
    msg++;
  }

  printf("sleeping 5 seconds...\n");
  sleep(5);

  printf("module B unregistering subscriber and publisher\n");
  subscriber_unregister(subscriber_A);
  publisher_unregister(publisher_B);

  printf("module B closing deros\n");
  deros_done(my_node_id);

  printf("module B done.\n");
  return 0;
}
