# , 'b_lundef=false', 'b_sanitize=address,undefined'
project('maidenless', 'c', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c11', 'cpp_std=c++17'])

cc = meson.get_compiler('c')
m = cc.find_library('m', required : false)
dl = cc.find_library('dl', required : false)

sdl2 = dependency('SDL2')
opengl = dependency('OpenGL')


if build_machine.system() == 'darwin'
    molten_dirs = []
    if import('fs').exists('/opt/homebrew/lib/libMoltenVK.dylib')
        molten_dirs += '/opt/homebrew/lib'
    endif
    vulkan = cc.find_library('MoltenVK', required : true, dirs : molten_dirs)
else
    vulkan = dependency('vulkan')
endif

shaders = [
  'source/shaders/gbuffer.vert.glsl',
  'source/shaders/gbuffer.frag.glsl',
  'source/shaders/shading.comp.glsl',
]

add_global_arguments(
  '-Wall',
  '-Wno-unused-parameter',
  '-Wno-nullability-extension',
  '-Werror',
  language: 'c',
)

# CPP files aren't my code, don't want to mess the warnings
add_global_arguments(
  '-w',
  '-Wno-nullability-extension',
  language: 'cpp',
)

glsllang = find_program('glslangValidator')

shader_targets = []

foreach s : shaders
  target_name = '@0@.spv'.format(s).split('/')
  shaders += custom_target(
    target_name.get(target_name.length()-1),
    command : [glsllang, '-V', '@INPUT@', '-o', '@OUTPUT@'],
    build_by_default: true,
    input : s, 
    output : '@BASENAME@.spv'
  )
endforeach

shaders = declare_dependency(
  sources: shader_targets
)

exe = executable('maidenless',
  'source/maidenless.c',

  'source/client/cl_client.c',

  'source/vk/vk.c',
  'source/vk/vk_gbuffer.c',
  'source/vk/vk_shading.c',

  'source/game/g_game.c',
  'source/game/g_collision.c',

  'external/toml.c',
  'external/cgltf.c',
  'external/vk_mem_alloc.cpp',
  'external/stbi_image.c',

  install : true,
  include_directories: [include_directories('source/'), include_directories('external/')],
  dependencies: [sdl2, opengl, vulkan, dl, m, shaders])