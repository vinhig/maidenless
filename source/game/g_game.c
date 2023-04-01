#include "g_game.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgltf.h"
#include "toml.h"

typedef struct scene_t {
  toml_table_t *def;
  bool loaded;
} scene_t;

struct game_t {
  char *base;

  scene_t *current_scene;
};

char *G_GetCompletePath(char *base, char *path) {
  char *complete_path = malloc(strlen(base) + strlen(path) + 2);
  sprintf(complete_path, "%s/%s", base, path);

  return complete_path;
}

game_t *G_CreateGame(char *base) {
  game_t *game = calloc(1, sizeof(game_t));
  game->base = malloc(strlen(base) + 1);
  strcpy(game->base, base);

  // Parse base/main.toml
  // No question asked, it's mandatory to have that file
  char *main_toml_path = G_GetCompletePath(game->base, "main.toml");
  FILE *f = fopen(main_toml_path, "r");

  if (!f) {
    printf("`%s` doesn't seem to contain a `main.toml` file (`%s`).\n",
           game->base, main_toml_path);
    free(main_toml_path);
    fclose(f);
    return NULL;
  }

  char error[256];
  toml_table_t *main_scene = toml_parse_file(f, error, sizeof(error));

  if (!main_scene) {
    printf(
        "`%s` doesn't seem to be a valid toml file. Error while parsing %s.\n",
        main_toml_path, error);
    free(main_toml_path);
    fclose(f);
    return NULL;
  }

  game->current_scene = malloc(sizeof(scene_t));

  game->current_scene->def = main_scene;
  game->current_scene->loaded = false;

  free(main_toml_path);
  fclose(f);

  return game;
}

bool G_LoadMap(client_t *client, game_t *game, char *map_path) {
  char *complete_map_path = G_GetCompletePath(game->base, map_path);

  FILE *f = fopen(complete_map_path, "rb");
  if (!f) {
    printf("Couldn't open map file `%s` (%s).\n", map_path, complete_map_path);
    return false;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  void *buff = malloc(size);

  fread(buff, size, 1, f);
  fclose(f);

  cgltf_options options = {0};
  cgltf_data *data = NULL;
  cgltf_result result = cgltf_parse(&options, buff, size, &data);

  if (result != cgltf_result_success) {
    printf("Couldn't parse map file `%s` (%s).\n", map_path, complete_map_path);
    free(data);
    free(complete_map_path);
    return false;
  }

  if (cgltf_load_buffers(&options, data, complete_map_path) !=
      cgltf_result_success) {
    printf("I'm tired boss.\n");
    return false;
  }

  // The purpose here is to convert the hierarchical gtlf into a completely flat
  // mesh able to be drawn in a single draw call.
  // But first, a sanity check. We don't want to let the user wait too long
  // before saying the mesh is actually invalid. So the rule is: no loading if
  // the code returns, and no return if the code loads
  // Here we return, so we don't load.
  for (cgltf_size m = 0; m < data->meshes_count; m++) {
    cgltf_mesh *mesh = &data->meshes[m];

    for (cgltf_size p = 0; p < mesh->primitives_count; p++) {
      cgltf_primitive *primitive = &mesh->primitives[p];

      if (primitive->type != cgltf_primitive_type_triangles) {
        printf("All primitives has to be triangles for the map `%s` (%s).\n",
               map_path, complete_map_path);
        return false;
      }

      bool found_uv = false;
      bool found_pos = false;
      bool found_normal = false;
      for (cgltf_size a = 0; a < primitive->attributes_count; a++) {
        if (strcmp(primitive->attributes[a].name, "TEXCOORD_0") == 0) {
          found_uv = true;
        }
        if (strcmp(primitive->attributes[a].name, "POSITION") == 0) {
          found_pos = true;
        }
        if (strcmp(primitive->attributes[a].name, "NORMAL") == 0) {
          found_normal = true;
        }
      }

      if (!found_uv) {
        printf(
            "All primitives has to have a `TEXCOORD_0` attributes `%s` (%s).\n",
            map_path, complete_map_path);
        return false;
      }
      if (!found_pos) {
        printf(
            "All primitives has to have a `POSITION` attributes `%s` (%s).\n",
            map_path, complete_map_path);
        return false;
      }
      if (!found_normal) {
        printf("All primitives has to have a `NORMAL` attributes `%s` (%s).\n",
               map_path, complete_map_path);
        return false;
      }
    }
  }

  // Here we load, so we don't return.
  for (cgltf_size m = 0; m < data->meshes_count; m++) {
    cgltf_mesh *mesh = &data->meshes[m];

    for (cgltf_size p = 0; p < mesh->primitives_count; p++) {
      cgltf_primitive *primitive = &mesh->primitives[p];

      for (cgltf_size a = 0; a < primitive->attributes_count; a++) {
        if (strcmp(primitive->attributes[a].name, "TEXCOORD_0") == 0) {
          cgltf_size s = primitive->attributes[a].data->buffer_view->size;
          switch (primitive->attributes[a].data->component_type) {
          case cgltf_component_type_r_32f: {
            float *data = (float*)cgltf_buffer_view_data(
                primitive->attributes[a].data->buffer_view);

            for (cgltf_size i = 0; i < s / sizeof(float); i++) {
              printf("%f\n", data[i]);
            }
            break;
          }
          default: {
            // As explained before, no return when we are loading.
          }
          }
        }
        if (strcmp(primitive->attributes[a].name, "POSITION") == 0) {
        }
        if (strcmp(primitive->attributes[a].name, "NORMAL") == 0) {
        }
      }
    }
  }

  free(data);
  free(complete_map_path);
  return true;
}

bool G_LoadCurrentScene(client_t *client, game_t *game) {
  if (!game->current_scene) {
    printf("No current scene set.\n");
    return false;
  }

  if (game->current_scene->loaded) {
    printf("Current scene was already loaded.\n");
    return false;
  }

  scene_t *scene = game->current_scene;

  toml_table_t *server = toml_table_in(scene->def, "scene");
  if (!server) {
    printf("The current scene definition (toml file) doesn't contain a "
           "[scene] entry.\n");
    return false;
  }

  toml_datum_t map = toml_string_in(server, "map");
  if (!map.ok) {
    printf("The current scene definition (toml file) doesn't contain a "
           "[scene.map] entry.\n");
    return false;
  }

  char *level_path = map.u.s;

  printf("Loading %s...\n", level_path);

  if (!G_LoadMap(client, game, level_path)) {
    printf("Failed to load map file.\n");
    free(level_path);
    return false;
  }

  printf("Finished!\n");

  free(level_path);

  return true;
}

void G_TickGame(client_t *client, game_t *game) {}

void G_DestroyGame(game_t *game) {
  free(game->base);
  toml_free(game->current_scene->def);
}
