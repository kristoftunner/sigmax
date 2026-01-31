#!/bin/bash

PRJ_DIR="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd $PRJ_DIR
rm -rf build
${PRJ_DIR}/configure.sh
${PRJ_DIR}/build.sh
cd $PRJ_DIR