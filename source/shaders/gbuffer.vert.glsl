#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 o_color;

layout(push_constant) uniform Constants { mat4 view_proj; }
uniforms;

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 view;
  mat4 proj;
  mat4 view_proj;
}
global_ubo;

void main() {
  gl_Position = global_ubo.view_proj * vec4(pos, 1.0f);
  o_color = vec3(uv, 1.0);
}
