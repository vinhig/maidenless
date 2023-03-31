#include <stdio.h>

#include "client/cl_client.h"

#define VERSION "0.1"

int main(int argc, char **argv) {
  printf("Creating client using Maidenless Engine `%s`.\n", VERSION);
  client_t *client = CL_CreateClient("Maidenless Engine", 1280, 720);

  if (!client) {
    printf("Couldn't create a client. Check error log for details.\n");
    return -1;
  }

  while (CL_GetClientState(client) == CLIENT_RUNNING) {
    CL_UpdateClient(client);

    CL_DrawClient(client);
  }

  CL_DestroyClient(client);

  printf("Exiting client. Thx for using the Maidenless Engine `%s`. Maybe you'll "
         "find your maiden one day.\n",
         VERSION);

  return 0;
}
