#!/bin/bash

set -e

pushd tools

if [[ ! -e crc/crc ]]; then
    pushd crc
    submod clone
    c++ -std=c++23 -march=native -O2 main.cpp -o crc
    popd
fi
if [[ ! -e tools/uf2 ]]; then
    git clone https://github.com/microsoft/uf2 uf2
fi
