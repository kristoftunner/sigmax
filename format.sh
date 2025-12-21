#!/bin/bash

git ls-files '*.cpp' '*.cc' '*.cxx' '*.h' '*.hpp' '*.hh' | xargs clang-format --verbose -i