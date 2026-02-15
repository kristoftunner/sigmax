#!/bin/bash

DIRNAME="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd $DIRNAME
git ls-files '*.cpp' '*.cc' '*.cxx' '*.h' '*.hpp' '*.hh' | xargs clang-format --verbose -i