// glsl version 4.5
#version 450

#extension GL_EXT_nonuniform_qualifier: require

layout(location = 0) in vec3 o_color;
layout(location = 1) in vec2 o_uv;
layout(location = 2) flat in int o_albedo_id;

// output write
layout(location = 0) out vec4 outFragColor;

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main() {
  // return red
  outFragColor = vec4(texture(textures[o_albedo_id], o_uv).rgb, 1.0f);
}
