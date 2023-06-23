layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 view;
  mat4 proj;
  mat4 view_proj;
  vec4 view_dir;

  vec2 view_dim;
}
global_ubo;