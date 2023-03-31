#include "cl_client.h"
#include "vk/vk.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

struct client_t {
  SDL_Window *window;
  client_state_t state;

  vk_rend_t *rend;
};

void *CL_GetWindow(client_t *client) { return client->window; }

client_t *CL_CreateClient(const char *title, unsigned width, unsigned height) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("Failed to initialize SDL2.\n");
    return NULL;
  }

  SDL_Window *window = SDL_CreateWindow(
      title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)width,
      (int)height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);

  if (window == NULL) {
    printf("Failed to create a SDL2 window: %s\n", SDL_GetError());
    return NULL;
  }

  int rend_count = SDL_GetNumRenderDrivers();

  if (rend_count == 0) {
    printf("Failed to find a suitable SDL2 renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return NULL;
  }

  client_t *client = malloc(sizeof(client_t));

  client->state = CLIENT_CREATING;
  client->window = window;

  client->rend = VK_CreateRend(client, width, height);

  if (client->rend == NULL) {
    printf("Failed to create a VK renderer. `%s`\n", VK_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return NULL;
  }

  client->state = CLIENT_RUNNING;

  return client;
}

client_state_t CL_GetClientState(client_t *client) { return client->state; }

void CL_UpdateClient(client_t *client) {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      client->state = CLIENT_QUITTING;
      return;
    }
    switch (event.type) {}
  }
}

void CL_DrawClient(client_t *client) { VK_Draw(client->rend); }

void CL_DestroyClient(client_t *client) {
  VK_DestroyRend(client->rend);
  SDL_DestroyWindow(client->window);
  SDL_Quit();
}
