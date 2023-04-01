#pragma once

#include <stdbool.h>

typedef struct client_t client_t;

typedef struct game_t game_t;

/// @brief Create a new game, allocating the memory for it. Read `main.toml`
/// from the given base folder, and set it as the current scene. No assets
/// loading occurs.
/// @param base Base folder to fetch all assets from.
/// @return
game_t *G_CreateGame(char *base);

bool G_LoadCurrentScene(client_t *client, game_t *game);

void G_TickGame(client_t *client, game_t *game);

void G_DestroyGame(game_t *game);
