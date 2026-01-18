#!/bin/bash

DIRNAME="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ${DIRNAME}/sigmax_exports
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
