#!/bin/bash

SCRIPT_FILE_PATH="$(realpath -- "${BASH_SOURCE[0]}")"
SCRIPT_DIR_PATH="$(dirname -- "${SCRIPT_FILE_PATH}")"
source ${SCRIPT_DIR_PATH}/sigmax_exports
if [[ ${?} -ne 0 ]]; then
  echo "<<! Failed to source sigmax_exports"
  exit 1
fi
source ${SCRIPT_DIR_PATH}/sigmax_aliases
if [[ ${?} -ne 0 ]]; then
  echo "<<! Failed to source sigmax_aliases"
  exit 1
fi

sudo apt install build-essential

command -v perf >/dev/null 2>&1 && echo "" || echo "Please install perf according to the linux system you are using"
