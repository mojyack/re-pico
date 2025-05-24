project('nolibc++', 'cpp', version: '1.0.0', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++23'])
add_project_arguments('-DNOXX_TEST', language: 'cpp')

executable('malloc-test',
  files(
    'src/malloc.cpp',
    'tests/malloc.cpp',
  )
)

executable('string-test',
  files(
    'src/malloc.cpp',
    'src/string.cpp',
    'tests/string.cpp',
    'tests/noxx-support.cpp',
  )
)
