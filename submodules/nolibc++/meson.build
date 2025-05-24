project('nolibc++', 'cpp', version: '1.0.0', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++23'])
add_project_arguments('-DNOXX_TEST', language: 'cpp')

executable('malloc',
  files(
    'src/malloc.cpp',
    'tests/malloc.cpp',
  )
)
