#pragma once

#include <stdbool.h>

typedef struct game_t game_t;

typedef struct client_desc_t {
  unsigned width, height;
  char *desired_gpu;
  char *game;
  bool fullscreen;
} client_desc_t;

typedef struct client_t client_t;

typedef enum client_state_t {
  CLIENT_RUNNING,
  CLIENT_QUITTING,
  CLIENT_CREATING,
  CLIENT_DESTROYING,
} client_state_t;

bool CL_ParseClientDesc(client_desc_t *desc, int argc, char *argv[]);
client_t *CL_CreateClient(const char *title, client_desc_t *desc);

client_state_t CL_GetClientState(client_t *client);

void CL_UpdateClient(client_t *client);

void CL_PushLoadingScreen(client_t *client);

void CL_PopLoadingScreen(client_t *client);

void CL_DrawClient(client_t *client, game_t *game);

void CL_DestroyClient(client_t *client);
