#version 450

#extension GL_GOOGLE_include_directive : enable

#include "global_ubo.glsl"

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 o_color;
layout(location = 1) out vec2 vtx_uv;
layout(location = 2) flat out int o_albedo_id;

layout(location = 3) out vec3 vtx_position;
layout(location = 4) out vec3 vtx_normal;

layout(push_constant) uniform Constants { int albedo_id; }
uniforms;

void main() {
  gl_Position = global_ubo.view_proj * vec4(pos, 1.0f);
  o_color = vec3(uv, 1.0);
  vtx_uv = uv;
  o_albedo_id = uniforms.albedo_id;

  vtx_position = (global_ubo.view_proj * vec4(pos, 1.0f)).xyz;
  vtx_normal = normalize(norm.xyz);
}
