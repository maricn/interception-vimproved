project('interception-vimproved', 'cpp')

add_project_arguments(
  '--std=c++20', '-O3', '-g',
  '-Wall', '-Wextra', '-Wpedantic', '-Werror',
  '-Wno-unused-parameter', '-Wno-type-limits',
  language: 'cpp'
)

executable(
  'interception-vimproved',
  'interception-vimproved.cpp',
  dependencies: dependency('yaml-cpp')
)
