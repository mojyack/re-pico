project('nolibc++', 'cpp', version: '1.0.0', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++23'])
add_project_arguments('-DNOXX_TEST', language: 'cpp')

exe = executable('malloc-test',
  files(
    'src/malloc.cpp',
    'tests/malloc.cpp',
  )
)
test('malloc', exe)

exe = executable('string-test',
  files(
    'src/malloc.cpp',
    'src/string-view.cpp',
    'src/string.cpp',
    'tests/noxx-support.cpp',
    'tests/string.cpp',
  )
)
test('string', exe)

exe = executable('vector-test',
  files(
    'src/malloc.cpp',
    'tests/noxx-support.cpp',
    'tests/vector.cpp',
  )
)
test('vector', exe)

exe = executable('variant-test',
  files(
    'tests/noxx-support.cpp',
    'tests/variant.cpp',
  )
)
test('variant', exe)

exe = executable('format-test',
  files(
    'src/malloc.cpp',
    'src/string-view.cpp',
    'src/string.cpp',
    'tests/format.cpp',
    'tests/noxx-support.cpp',
  )
)
test('format', exe)

exe = executable('charconv-test',
  files(
    'src/malloc.cpp',
    'src/string-view.cpp',
    'src/string.cpp',
    'tests/charconv.cpp',
    'tests/noxx-support.cpp',
  )
)
test('charconv', exe)

exe = executable('unique-ptr-test',
  files(
    'src/malloc.cpp',
    'tests/noxx-support.cpp',
    'tests/unique-ptr.cpp',
  )
)
test('unique-ptr', exe)
