#!/bin/bash

DIRNAME="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cmake -S . -B build
cd build && make -j
cd $DIRNAME
