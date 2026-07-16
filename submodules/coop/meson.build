project(
    'coop',
    'cpp',
    version: '1.0.0',
    default_options: ['warning_level=3', 'werror=false', 'cpp_std=c++23'],
)

add_project_arguments('-DNOXX_TEST', language: 'cpp')
add_project_arguments(
    '-Wno-missing-designated-field-initializers',
    language: 'cpp',
)

# tests/include supplies the host hal/time.hpp shim
inc = include_directories('src', 'tests/include')

coop_files = files(
    'src/coop/runner.cpp',
)

noxx_files = files(
    'src/noxx/malloc.cpp',
)

coop_lib = static_library(
    'coop',
    coop_files + noxx_files,
    include_directories: inc,
)

support_files = files('tests/noxx-support.cpp')

foreach name : [
    'coop',
]
    exe = executable(
        name + '-test',
        files('tests/' + name + '.cpp') + support_files,
        include_directories: inc,
        link_with: coop_lib,
    )
    test(name, exe)
endforeach
