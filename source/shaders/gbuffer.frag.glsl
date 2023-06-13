// glsl version 4.5
#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 o_color;
layout(location = 1) in vec2 vtx_uv;
layout(location = 2) flat in int o_albedo_id;

layout(location = 3) in vec3 vtx_position;
layout(location = 4) in vec3 vtx_normal;

// output write
layout(location = 0) out vec4 o_position;
layout(location = 1) out vec4 o_normal;
layout(location = 2) out vec4 o_albedo;

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main() {
  o_albedo = vec4(texture(textures[o_albedo_id], vtx_uv).rgb, 1.0f);
  o_position = vec4(vtx_position, 1.0);
  o_normal = vec4(normalize(vtx_normal), 1.0);
}
