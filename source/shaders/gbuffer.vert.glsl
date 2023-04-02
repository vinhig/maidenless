#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 o_color;

void main() {
  gl_Position = vec4(pos, 1.0f);
  o_color = vec3(uv, 1.0);
}
