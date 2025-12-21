#!/bin/bash

cmake_files=$(find . -name "CMakeLists.txt")
for file in $cmake_files; do
    cmake-format -i $file
done

cmake_files=$(find . -name "*.cmake")
for file in $cmake_files; do
    cmake-format -i $file
done
