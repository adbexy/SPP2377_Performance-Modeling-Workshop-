#!/usr/bin/env bash

target="simdops_query"
testing="ON"
build_type="ON"

for arg in "$@"; do
    case "$arg" in
        TESTING=ON|TESTING=OFF)
            testing="${arg#TESTING=}"
            ;;
    esac
    case "$arg" in
        BUILD_TYPE=Debug|BUILD_TYPE=Release)
            build_type="${arg#BUILD_TYPE=}"
            ;;
    esac
done

#set -ex

echo "Target:  $target"
echo "Testing: $testing"
echo "Build type: $build_type"

touch cmake_output.txt
cmake -DTESTING="$testing" -DCMAKE_BUILD_TYPE="$build_type" . > cmake_output.txt 2>&1

# cmake
cmake --build . --target "$target"
"./bin/$target" | tee output
tail -n4 output > my_results
