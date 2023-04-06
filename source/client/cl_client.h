#pragma once

#include <stdbool.h>

typedef struct game_t game_t;
typedef struct game_state_t game_state_t;
typedef struct client_t client_t;
typedef struct vk_rend_t vk_rend_t;

typedef struct client_desc_t {
  unsigned width, height;
  char *desired_gpu;
  char *game;
  bool fullscreen;
} client_desc_t;

typedef enum client_state_t {
  CLIENT_RUNNING,
  CLIENT_QUITTING,
  CLIENT_PAUSED,
  CLIENT_CREATING,
  CLIENT_DESTROYING,
} client_state_t;

bool CL_ParseClientDesc(client_desc_t *desc, int argc, char *argv[]);
client_t *CL_CreateClient(const char *title, client_desc_t *desc);

client_state_t CL_GetClientState(client_t *client);
vk_rend_t *CL_GetRend(client_t *client);
void CL_GetViewDim(client_t *client, unsigned *width, unsigned *height);

void CL_UpdateClient(client_t *client);

void CL_PushLoadingScreen(client_t *client);

void CL_PopLoadingScreen(client_t *client);

void CL_DrawClient(client_t *client, game_state_t *game);

void CL_DestroyClient(client_t *client);
