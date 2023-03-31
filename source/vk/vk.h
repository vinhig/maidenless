#pragma once

typedef struct client_t client_t;
typedef struct vk_rend_t vk_rend_t;

vk_rend_t *VK_CreateRend(client_t *client, unsigned width, unsigned height);

void VK_Draw(vk_rend_t *rend);

void VK_DestroyRend(vk_rend_t *rend);

const char *VK_GetError();
