#include "cglm/cglm.h"
#include "g_game.h"
#include "vk/vk.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct intersection_t {
  float t;
  vec3 n;
} intersection_t;

typedef struct triangle_t {
  vec3 a;
  vec3 b;
  vec3 c;
  vec3 n;
} triangle_t;

typedef struct collision_mesh_t {
  triangle_t *triangles;
  unsigned triangle_count;
} collision_mesh_t;

collision_mesh_t *G_LoadCollisionMap(primitive_t *primitives,
                                     size_t primitive_count) {
  // We don't know the number of triangles in advance
  triangle_t *triangles = calloc(1, sizeof(triangle_t) * 1024);
  unsigned triangle_capacity = 1024;
  unsigned triangle_count = 0;
  // TODO: this is SUPER naive
  for (size_t i = 0; i < primitive_count; i++) {
    primitive_t *primitive = &primitives[i];

    for (size_t j = 0; j < primitive->index_count / 3; j++) {
      triangle_t new_triangle;
      unsigned i_a = primitive->indices[j * 3 + 0];
      unsigned i_b = primitive->indices[j * 3 + 1];
      unsigned i_c = primitive->indices[j * 3 + 2];
      {
        new_triangle.a[0] = primitive->vertices[i_a].pos[0];
        new_triangle.a[1] = primitive->vertices[i_a].pos[1];
        new_triangle.a[2] = primitive->vertices[i_a].pos[2];
      }
      {
        new_triangle.b[0] = primitive->vertices[i_b].pos[0];
        new_triangle.b[1] = primitive->vertices[i_b].pos[1];
        new_triangle.b[2] = primitive->vertices[i_b].pos[2];
      }
      {
        new_triangle.c[0] = primitive->vertices[i_c].pos[0];
        new_triangle.c[1] = primitive->vertices[i_c].pos[1];
        new_triangle.c[2] = primitive->vertices[i_c].pos[2];
      }

      {
        vec3 b_a, c_a;
        glm_vec3_sub(new_triangle.b, new_triangle.a, b_a);
        glm_vec3_sub(new_triangle.c, new_triangle.a, c_a);
        glm_vec3_cross(b_a, c_a, new_triangle.n);
        glm_normalize(new_triangle.n);
      }

      if (triangle_count == triangle_capacity) {
        triangle_capacity *= 2;
        triangles = realloc(triangles, triangle_capacity * sizeof(triangle_t));
      }

      triangles[triangle_count] = new_triangle;
      triangle_count++;
    }
  }

  collision_mesh_t *mesh = malloc(sizeof(collision_mesh_t));
  mesh->triangle_count = triangle_count;
  mesh->triangles = triangles;

  return mesh;
}

bool G_TestTriangle(triangle_t *triangle, vec3 orig, vec3 dir, float distance,
                    vec3 tuv) {
  vec3 edge1;
  vec3 edge2;

  glm_vec3_sub(triangle->b, triangle->a, edge1);
  glm_vec3_sub(triangle->c, triangle->a, edge2);

  vec3 pvec;
  glm_vec3_cross(dir, edge2, pvec);

  float det = glm_vec3_dot(edge1, pvec);

  if (det > -0.000001 && det < 0.000001) {
    return false;
  }

  float inv_det = 1.0 / det;

  vec3 tvec;
  glm_vec3_sub(orig, triangle->a, tvec);

  float u = glm_vec3_dot(tvec, pvec) * inv_det;
  if (u < 0.0 || u > 1.0) {
    return false;
  }

  vec3 qvec;
  glm_vec3_cross(tvec, edge1, qvec);

  float v = glm_vec3_dot(dir, qvec) * inv_det;
  if (v < 0.0 || u + v > 1.0) {
    return false;
  }

  float t = glm_vec3_dot(edge2, qvec) * inv_det;
  if (t < 0.0 || t >= distance) {
    return false;
  }

  tuv[0] = t;
  tuv[1] = u;
  tuv[2] = v;

  return true;
}

bool G_CollisionRayQuery(collision_mesh_t *mesh, vec3 orig, vec3 dir,
                         float distance, bool movement, float *corr) {
  // A shame, really
  intersection_t intersections[10];
  unsigned intersection_count = 0;
  for (unsigned t = 0; t < mesh->triangle_count; t++) {
    triangle_t *triangle = &mesh->triangles[t];
    vec3 tuv;

    if (G_TestTriangle(triangle, orig, dir, distance, tuv)) {
      intersections[intersection_count].n[0] = triangle->n[0];
      intersections[intersection_count].n[1] = triangle->n[1];
      intersections[intersection_count].n[2] = triangle->n[2];
      intersections[intersection_count].t = tuv[0];
      intersection_count++;
    }
  }

  if (intersection_count == 0) {
    return false;
  }

  // Find closest intersection;
  unsigned idx = 0;
  float min_dis = 100.0;
  for (unsigned i = 0; i < intersection_count; i++) {
    if (min_dis > intersections[i].t) {
      idx = i;
      min_dis = intersections[i].t;
    }
  }

  float t = intersections[idx].t;
  if (corr != NULL) {
    *corr = t;
  }

  if (movement) {
    vec3 new_n = {-intersections[idx].n[2], 0.0, intersections[idx].n[0]};
    float d = glm_vec3_dot(new_n, dir);
    dir[0] = new_n[0] * d;
    dir[1] = 0.0;
    dir[2] = new_n[2] * d;
  }
  return true;
}

void G_DestroyCollisionMap(collision_mesh_t *mesh) {
  free(mesh->triangles);
  free(mesh);
}
