#pragma once

typedef struct client_t client_t;

typedef enum client_state_t {
  CLIENT_RUNNING,
  CLIENT_QUITTING,
  CLIENT_CREATING,
  CLIENT_DESTROYING,
} client_state_t;

client_t *CL_CreateClient(const char *title, unsigned width, unsigned height);

client_state_t CL_GetClientState(client_t *client);

void CL_UpdateClient(client_t *client);

void CL_DrawClient(client_t *client);

void CL_DestroyClient(client_t *client);
