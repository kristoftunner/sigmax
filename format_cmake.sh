#!/bin/bash

DIRNAME="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd $DIRNAME
cmake_files=$(find . -name "CMakeLists.txt")
for file in $cmake_files; do
    cmake-format -i $file
done

cmake_files=$(find . -name "*.cmake")
for file in $cmake_files; do
    cmake-format -i $file
done
