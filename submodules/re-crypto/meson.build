project(
    're-crypt',
    'cpp',
    version: '1.0.0',
    default_options: ['warning_level=3', 'werror=false', 'cpp_std=c++23'],
)

add_project_arguments('-DNOXX_TEST', language: 'cpp')
add_project_arguments(
    '-Wno-missing-designated-field-initializers',
    language: 'cpp',
)

inc = include_directories('src')

crypto_files = files(
    'src/crypto/aes-cmac.cpp',
    'src/crypto/aes-keywrap.cpp',
    'src/crypto/aes.cpp',
    'src/crypto/hmac-sha256.cpp',
    'src/crypto/kdf.cpp',
    'src/crypto/p256.cpp',
    'src/crypto/sha256.cpp',
)

crypto_lib = static_library(
    're-crypto',
    crypto_files,
    include_directories: inc,
)

support_files = files('submodules/nolibc++/tests/noxx-support.cpp')

foreach name : [
    'sha256',
    'hmac-sha256',
    'aes',
    'aes-cmac',
    'aes-keywrap',
    'p256',
]
    exe = executable(
        name + '-test',
        files('tests/' + name + '.cpp') + support_files,
        include_directories: inc,
        link_with: crypto_lib,
    )
    test(name, exe)
endforeach
