#!/bin/bash

# not recursive, otherwise errors may occur
git submodule update --init

# patch submodules urls (ssh-address -> https-address)
cd modules/SIMDOps
cp ../../simdops_modules_patch .gitmodules

# sync urls and update modules
git submodule sync
git submodule update --init

# generate tsl with the availaber vector extensions
# python3 tools/tslgen/main.py --targets sse sse2 sse3 ssse3 sse4.1 sse4.2 avx avx2 avx512 --no-workaround-warnings -o ../../libs/tsl

