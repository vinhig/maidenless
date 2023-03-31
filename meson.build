project('maidenless', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c11'])

cc = meson.get_compiler('c')
m = cc.find_library('m', required : false)
dl = cc.find_library('dl', required : false)

sdl2 = dependency('SDL2')
opengl = dependency('OpenGL')
vulkan = dependency('Vulkan')

exe = executable('maidenless',
  'source/maidenless.c',

  'source/client/cl_client.c',

  'source/vk/vk.c',

  install : true,
  include_directories: include_directories('source/'),
  dependencies: [sdl2, opengl, vulkan, dl, m])

test('basic', exe)
