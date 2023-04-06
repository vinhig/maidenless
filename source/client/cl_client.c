#include "cl_client.h"
#include "cl_input.h"
#include "vk/vk.h"

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "game/g_game.h"

struct client_t {
  SDL_Window *window;
  client_state_t state;

  unsigned v_width, v_height;

  vk_rend_t *rend;

  input_t input;
};

void *CL_GetWindow(client_t *client) { return client->window; }

input_t *CL_GetInput(client_t *client) { return &client->input; }

bool CL_ParseClientDesc(client_desc_t *desc, int argc, char *argv[]) {
  // Arbitrary decision: in debug mode, a badly formed argument is fatal
  //                     in release mode, it's not
  // It's handled on the function call site (int main)
  bool is_error = false;
  for (int i = 1; i < argc; i += 2) {
    char *arg = argv[i];
    // Check for each pair of argv what's the name of the property to change.
    // Most of the properties are strings that should be transformed into
    // number, resulting in a lot of if-else. Deeply sorry, but no other way to
    // ensure that.
    if (!strcmp(arg, "--width") || !strcmp(arg, "-w")) {
      char *width = argv[i + 1];
      char *endptr;
      unsigned val = strtol(width, &endptr, 10);

      if ((endptr - width) == 0 || (endptr - width) != (long)strlen(width)) {
        printf("Couldn't parse '--width' argument value '%s'.\n", width);
        is_error = true;
      } else {
        desc->width = val;
      }
    } else if (!strcmp(arg, "--height") || !strcmp(arg, "-h")) {
      char *height = argv[i + 1];
      char *endptr;
      unsigned val = strtol(height, &endptr, 10);

      if ((endptr - height) == 0 || (endptr - height) != (long)strlen(height)) {
        printf("Couldn't parse '--height' argument value '%s'.\n", height);
        is_error = true;
      } else {
        desc->height = val;
      }
    } else if (!strcmp(arg, "--fullscreen") || !strcmp(arg, "-f")) {
      if (i + 1 >= argc) {
        printf("Missing 'true' or 'false' after '--fullscreen' or '-f'.\n");
        is_error = true;
        break;
      }
      char *fullscreen = argv[i + 1];

      if (!strcmp(fullscreen, "true")) {
        desc->fullscreen = true;
      } else if (!strcmp(fullscreen, "false")) {
        desc->fullscreen = false;
      } else {
        printf("Fullscreen is either 'true' or 'false'.\n");
        is_error = true;
      }
    } else if (!strcmp(arg, "--physical_device") || !strcmp(arg, "-gpu")) {
      char *gpu = argv[i + 1];

      // Remove " " if they are present
      if (gpu[0] == '"' && gpu[strlen(gpu) - 1] == '"') {
        gpu[strlen(gpu) - 1] = '\0';
        gpu++;
      }

      desc->desired_gpu = malloc(strlen(gpu) + 1);
      strcpy(desc->desired_gpu, gpu);
    }
  }

  return is_error;
}

client_t *CL_CreateClient(const char *title, client_desc_t *desc) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    printf("Failed to initialize SDL2.\n");
    return NULL;
  }

  SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN;

  if (desc->fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
  }

  SDL_Window *window =
      SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       (int)desc->width, (int)desc->height, flags);

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

  // TODO: the referenced GPU in the description should be passed
  client->rend = VK_CreateRend(client, desc->width, desc->height);

  if (client->rend == NULL) {
    printf("Failed to create a VK renderer. `%s`\n", VK_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return NULL;
  }

  client->state = CLIENT_RUNNING;
  client->v_width = desc->width;
  client->v_height = desc->height;

  SDL_SetRelativeMouseMode(true);

  return client;
}

client_state_t CL_GetClientState(client_t *client) { return client->state; }

vk_rend_t *CL_GetRend(client_t *client) { return client->rend; }

void CL_GetViewDim(client_t *client, unsigned *width, unsigned *height) {
  *width = client->v_width;
  *height = client->v_height;
}

void CL_UpdateClient(client_t *client) {
  SDL_Event event;

  client->input.view.x_axis = 0.0;
  client->input.view.y_axis = 0.0;

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      client->state = CLIENT_QUITTING;
      return;
    }
    switch (event.type) {
    case SDL_KEYDOWN: {
      if (event.key.keysym.sym == SDLK_z) {
        client->input.movement.x_axis = 1.0f;
      } else if (event.key.keysym.sym == SDLK_s) {
        client->input.movement.x_axis = -1.0f;
      }

      if (event.key.keysym.sym == SDLK_q) {
        client->input.movement.y_axis = 1.0f;
      } else if (event.key.keysym.sym == SDLK_d) {
        client->input.movement.y_axis = -1.0f;
      }

      if (event.key.keysym.sym == SDLK_ESCAPE) {
        SDL_SetRelativeMouseMode(false);
      }
      break;
    }
    case SDL_KEYUP: {
      if (event.key.keysym.sym == SDLK_z) {
        client->input.movement.x_axis = 0.0f;
      } else if (event.key.keysym.sym == SDLK_s) {
        client->input.movement.x_axis = 0.0f;
      }

      if (event.key.keysym.sym == SDLK_q) {
        client->input.movement.y_axis = 0.0f;
      } else if (event.key.keysym.sym == SDLK_d) {
        client->input.movement.y_axis = 0.0f;
      }
      break;
    }
    case SDL_MOUSEMOTION: {
      client->input.view.x_axis = event.motion.yrel;
      client->input.view.y_axis = -event.motion.xrel;
      break;
    }
    default: {
    }
    }
  }
}

void CL_DrawClient(client_t *client, game_state_t *game) {
  VK_Draw(client->rend, game);
}

void CL_PushLoadingScreen(client_t *client) {}

void CL_PopLoadingScreen(client_t *client) {}

void CL_DestroyClient(client_t *client) {
  VK_DestroyRend(client->rend);
  SDL_DestroyWindow(client->window);
  SDL_Quit();

  free(client);
}
