#pragma once

#include <stdbool.h>

#include "cglm/types.h"

typedef struct client_t client_t;

typedef struct game_t game_t;

typedef struct game_state_t {
  // First person camera.
  // Yes, we want to speed gameplay
  struct {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    // vec3 pos;
  } fps;

  // Next, transforms for every actor
  // Actor are moving animated meshes
  struct {
    mat4 model;
    mat4 inv_model;
  } actors[64];
} game_state_t;

/// @brief Create a new game, allocating the memory for it. Read `main.toml`
/// from the given base folder, and set it as the current scene. No assets
/// loading occurs.
/// @param base Base folder to fetch all assets from.
/// @return
game_t *G_CreateGame(char *base);

bool G_LoadCurrentScene(client_t *client, game_t *game);

game_state_t G_TickGame(client_t *client, game_t *game);

void G_DestroyGame(game_t *game);
