#!/bin/bash

DIRNAME="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
${DIRNAME}/configure.sh
${DIRNAME}/build.sh
