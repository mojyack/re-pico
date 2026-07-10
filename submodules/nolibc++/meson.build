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
    'src/string-view.cpp',
    'src/string.cpp',
    'tests/noxx-support.cpp',
    'tests/string.cpp',
  )
)

executable('vector-test',
  files(
    'src/malloc.cpp',
    'tests/noxx-support.cpp',
    'tests/vector.cpp',
  )
)

executable('variant-test',
  files(
    'tests/noxx-support.cpp',
    'tests/variant.cpp',
  )
)

executable('format-test',
  files(
    'src/malloc.cpp',
    'src/string-view.cpp',
    'src/string.cpp',
    'tests/format.cpp',
    'tests/noxx-support.cpp',
  )
)
