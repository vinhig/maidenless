#include "g_game.h"

typedef struct collision_mesh_t collision_mesh_t;

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cglm/cglm.h"
#include "cgltf.h"
#include "stbi_image.h"
#include "toml.h"

#include "client/cl_client.h"
#include "client/cl_input.h"
#include "vk/vk.h"

typedef struct scene_t {
  toml_table_t *def;
  bool loaded;
} scene_t;

struct game_t {
  char *base;

  vec3 fps_pos;
  float fps_pitch;
  float fps_yaw;

  scene_t *current_scene;
  collision_mesh_t *current_mesh;
};

collision_mesh_t *G_LoadCollisionMap(primitive_t *primitives,
                                     unsigned primitive_count);
void G_DestroyCollisionMap(collision_mesh_t *mesh);
bool G_CollisionRayQuery(collision_mesh_t *mesh, vec3 orig, vec3 dir,
                         float distance, bool movement, float *t);

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

  game->fps_pos[0] = 7.5f;
  game->fps_pos[1] = 10.505f;
  game->fps_pos[2] = -7.5f;

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

  // First, a sanity check. We don't want to let the user wait too long
  // before saying the mesh is actually invalid. So the rule is: no loading if
  // the code returns, and no return if the code loads
  // Here we return, so we don't load.
  size_t primitive_count = 0;
  for (cgltf_size m = 0; m < data->meshes_count; m++) {
    cgltf_mesh *mesh = &data->meshes[m];

    for (cgltf_size p = 0; p < mesh->primitives_count; p++) {
      cgltf_primitive *primitive = &mesh->primitives[p];

      primitive_count++;

      if (primitive->type != cgltf_primitive_type_triangles) {
        printf("All primitives has to be triangles for the map `%s` (%s).\n",
               map_path, complete_map_path);
        return false;
      }

      if (!primitive->material->has_pbr_metallic_roughness) {
        printf("All primitives must have correct textures. `%s` (%s).\n",
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

  primitive_t *primitives = malloc(sizeof(primitive_t) * primitive_count);
  texture_t *textures =
      malloc(sizeof(texture_t) * primitive_count *
             3); // Assuming each texture has a albedo+normal+rougness textures

  size_t curr_primitive = 0;
  size_t curr_texture = 0;
  for (cgltf_size m = 0; m < data->meshes_count; m++) {
    cgltf_mesh *mesh = &data->meshes[m];

    for (cgltf_size p = 0; p < mesh->primitives_count; p++) {
      cgltf_primitive *primitive = &mesh->primitives[p];

      size_t vertex_count = 0;
      vertex_t *vertices = NULL;

      size_t index_count = 0;
      unsigned *indices = NULL;

      // Extracting base color texture
      {
        cgltf_texture_view base_color =
            primitive->material->pbr_metallic_roughness.base_color_texture;

        const uint8_t *texture_data = cgltf_buffer_view_data(
            (const cgltf_buffer_view *)base_color.texture->image->buffer_view);

        int w, h, n;
        unsigned char *data = stbi_load_from_memory(
            texture_data, base_color.texture->image->buffer_view->size, &w, &h,
            &n, 4);

        textures[curr_texture].width = w;
        textures[curr_texture].height = h;
        textures[curr_texture].c = n;
        textures[curr_texture].data = data;
        curr_texture++;
      }

      switch (primitive->indices->component_type) {
      case cgltf_component_type_r_16u: {
        index_count = primitive->indices->buffer_view->size / sizeof(uint16_t);
        uint16_t *data =
            (uint16_t *)cgltf_buffer_view_data(primitive->indices->buffer_view);

        indices = calloc(1, sizeof(unsigned) * index_count);

        for (size_t y = 0; y < index_count; y++) {
          indices[y] = (unsigned)data[y];

          if (indices[y] > 3000) {
            printf("mmmmmhh %d\n", indices[y]);
          }
        }

        break;
      }
      default: {
        printf("YO WTF???\n");
      }
      }

      for (cgltf_size a = 0; a < primitive->attributes_count; a++) {
        if (strcmp(primitive->attributes[a].name, "TEXCOORD_0") == 0) {
          switch (primitive->attributes[a].data->component_type) {
          case cgltf_component_type_r_32f: {
            float *data = (float *)cgltf_buffer_view_data(
                primitive->attributes[a].data->buffer_view);
            size_t n = primitive->attributes[a].data->buffer_view->size /
                       sizeof(float) / 2;
            if (vertices == NULL) {
              vertices = malloc(sizeof(vertex_t) * n);
              vertex_count = n;
            }

            for (size_t p = 0; p < n; p++) {
              vertices[p].uv[0] = data[p * 2 + 0];
              vertices[p].uv[1] = data[p * 2 + 1];
            }
            break;
          }
          default:
            break;
          }
        } else if (strcmp(primitive->attributes[a].name, "POSITION") == 0) {
          switch (primitive->attributes[a].data->component_type) {
          case cgltf_component_type_r_32f: {
            float *data = (float *)cgltf_buffer_view_data(
                primitive->attributes[a].data->buffer_view);
            size_t n = primitive->attributes[a].data->buffer_view->size /
                       sizeof(float) / 3;
            if (vertices == NULL) {
              vertices = malloc(sizeof(vertex_t) * n);
              vertex_count = n;
            }

            for (size_t p = 0; p < n; p++) {
              vertices[p].pos[0] = data[p * 3 + 0];
              vertices[p].pos[1] = data[p * 3 + 1];
              vertices[p].pos[2] = data[p * 3 + 2];
            }
            break;
          }
          default:
            break;
          }
        }
        if (strcmp(primitive->attributes[a].name, "NORMAL") == 0) {
          switch (primitive->attributes[a].data->component_type) {
          case cgltf_component_type_r_32f: {
            float *data = (float *)cgltf_buffer_view_data(
                primitive->attributes[a].data->buffer_view);
            size_t n = primitive->attributes[a].data->buffer_view->size /
                       sizeof(float) / 3;
            if (vertices == NULL) {
              vertices = malloc(sizeof(vertex_t) * n);
              vertex_count = n;
            }

            for (size_t p = 0; p < n; p++) {
              vertices[p].norm[0] = data[p * 3 + 0];
              vertices[p].norm[1] = data[p * 3 + 1];
              vertices[p].norm[2] = data[p * 3 + 2];
            }
            break;
          }
          default:
            break;
          }
        }
      }

      primitives[curr_primitive].indices = indices;
      primitives[curr_primitive].vertices = vertices;
      primitives[curr_primitive].vertex_count = vertex_count;
      primitives[curr_primitive].index_count = index_count;

      curr_primitive++;
    }
  }

  if (game->current_mesh) {
    G_DestroyCollisionMap(game->current_mesh);
  }

  game->current_mesh = G_LoadCollisionMap(primitives, primitive_count);

  VK_PushMap(CL_GetRend(client), primitives, primitive_count, textures,
             curr_texture);

  for(unsigned i = 0; i < primitive_count; i++) {
    free(primitives[i].indices);
    free(primitives[i].vertices);
  }
  for(unsigned i = 0; i < curr_texture; i++) {
    free(textures[i].data);
  }
  free(primitives);
  free(textures);

  cgltf_free(data);
  free(complete_map_path);
  free(buff);
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

game_state_t G_TickGame(client_t *client, game_t *game) {
  // Process input
  input_t *input = CL_GetInput(client);

  // Update camera rotation
  game->fps_yaw += input->view.x_axis * 0.12;
  if (game->fps_yaw >= 90.0f) {
    game->fps_yaw = 89.9f;
  } else if (game->fps_yaw <= -90.0f) {
    game->fps_yaw = -89.9f;
  }
  game->fps_pitch += input->view.y_axis * 0.12;

  mat4 rot;
  glm_mat4_identity(rot);
  // For the movement, we don't need to influence with yaw
  glm_rotate_y(rot, glm_rad(game->fps_pitch), rot);

  vec3 center = {0.0, 0.0, 1.0};
  vec3 mvnt = {input->movement.y_axis, 0.0, input->movement.x_axis};

  glm_mat4_mulv3(rot, mvnt, 1.0, mvnt);
  // But for the view dir, we need to influence with pitch
  glm_rotate_x(rot, glm_rad(game->fps_yaw), rot);
  glm_mat4_mulv3(rot, center, 1.0, center);

  glm_vec3_normalize(mvnt);
  glm_vec3_normalize(center);

  vec3 foot_pos;
  glm_vec3_sub(game->fps_pos, (vec3){0.0, 0.8, 0.0}, foot_pos);

  vec3 gravity = {0.0, -1.0, 0.0};

  float t;
  bool collided = G_CollisionRayQuery(game->current_mesh, foot_pos, gravity,
                                      0.8, false, &t);
  if (!collided) {
    gravity[1] *= 0.12;
    glm_vec3_add(gravity, game->fps_pos, game->fps_pos);
  } else {
    // printf("foot_pos = {%f, %f, %f}\n", foot_pos[0], foot_pos[1],
    // foot_pos[2]); printf("game->fps_pos = {%f, %f, %f}\n", game->fps_pos[0],
    // game->fps_pos[1],
    //        game->fps_pos[2]);
    // printf("mvnt = {%f, %f, %f}\n", mvnt[0], mvnt[1], mvnt[2]);
    gravity[1] = (0.7 - t);
    glm_vec3_add(gravity, game->fps_pos, game->fps_pos);
  }

  if (!G_CollisionRayQuery(game->current_mesh, foot_pos, mvnt, 0.35, true,
                           NULL)) {

    mvnt[0] *= 0.24;
    mvnt[1] = 0.0;
    mvnt[2] *= 0.24;

    glm_vec3_add(mvnt, game->fps_pos, game->fps_pos);
  }

  // Construct game state
  unsigned v_width, v_height;
  CL_GetViewDim(client, &v_width, &v_height);

  vec3 eye = {game->fps_pos[0], game->fps_pos[1], game->fps_pos[2]};
  // glm_vec3_add(eye, center, center);
  vec3 up = {0.0, 1.0, 0.0};

  game_state_t game_state;

  glm_look(eye, center, up, game_state.fps.view);
  glm_perspective(glm_rad(60.0f), (float)v_width / (float)v_height, 0.001f,
                  150.0f, game_state.fps.proj);
  game_state.fps.proj[1][1] *= -1;
  glm_mat4_mul(game_state.fps.proj, game_state.fps.view,
               game_state.fps.view_proj);

  return game_state;
}

void G_DestroyGame(game_t *game) {
  G_DestroyCollisionMap(game->current_mesh);
  free(game->base);
  toml_free(game->current_scene->def);
  free(game->current_scene);

  free(game);
}
