#pragma once

#include <stddef.h>

typedef struct game_state_t game_state_t;
typedef struct client_t client_t;
typedef struct vk_rend_t vk_rend_t;

typedef struct vertex_t {
  float pos[3];
  float norm[3];
  float uv[3];
} vertex_t;

typedef struct primitive_t {
  vertex_t *vertices;
  size_t vertex_count;
  unsigned *indices;
  size_t index_count;

} primitive_t;

typedef struct texture_t {
  int width, height, c;
  unsigned char *data;
} texture_t;

typedef struct vk_map_t vk_map_t;

vk_rend_t *VK_CreateRend(client_t *client, unsigned width, unsigned height);

void VK_PushMap(vk_rend_t *rend, primitive_t *primitives,
                size_t primitive_count, texture_t *textures,
                size_t texture_count);

void VK_Draw(vk_rend_t *ren, game_state_t *game);

void VK_DestroyRend(vk_rend_t *rend);

const char *VK_GetError();
