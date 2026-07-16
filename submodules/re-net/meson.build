project(
    're-net',
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

net_files = files(
    'src/net/arp.cpp',
    'src/net/icmp.cpp',
    'src/net/ip.cpp',
    'src/net/packet.cpp',
    'src/net/stack.cpp',
    # the stack is coop-driven; vendor coop's runtime alongside it
    'src/coop/runner.cpp',
)

noxx_files = files(
    'src/noxx/malloc.cpp',
    'src/noxx/string-view.cpp',
    'src/noxx/string.cpp',
)

net_lib = static_library(
    're-net',
    net_files + noxx_files,
    include_directories: inc,
)

support_files = files(
    'submodules/nolibc++/tests/noxx-support.cpp',
    'submodules/coop/tests/coop-support.cpp',
)

foreach name : [
    'packet',
    'net',
]
    exe = executable(
        name + '-test',
        files('tests/' + name + '.cpp') + support_files,
        include_directories: inc,
        link_with: net_lib,
    )
    test(name, exe)
endforeach

# linux tap harness; run manually against a tap device (needs a pre-created
# tap or CAP_NET_ADMIN), so it is not registered as a test
executable(
    'tap-host',
    files('tests/tap-host.cpp') + support_files,
    include_directories: inc,
    link_with: net_lib,
)
