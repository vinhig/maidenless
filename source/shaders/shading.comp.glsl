#version 450

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

#extension GL_EXT_ray_query : enable
#extension GL_GOOGLE_include_directive : enable

#include "global_ubo.glsl"

layout(set = 1, binding = 0) uniform writeonly image2D img_shading;
layout(set = 1, binding = 1) uniform accelerationStructureEXT acc_struct;
layout(set = 1, binding = 2) uniform sampler2D tex_position;
layout(set = 1, binding = 3) uniform sampler2D tex_normal;
layout(set = 1, binding = 4) uniform sampler2D tex_albedo;
layout(set = 1, binding = 5) uniform sampler2D tex_depth;

void main() {
  ivec2 coord = ivec2(gl_GlobalInvocationID);

  if (coord.x < 0 || coord.y < 0 || coord.x >= global_ubo.view_dim.x ||
      coord.y >= global_ubo.view_dim.y) {
    return;
  }

  vec4 norm = texelFetch(tex_normal, coord, 0);
  vec4 albedo = texelFetch(tex_albedo, coord, 0);

  imageStore(img_shading, coord, albedo);
}
