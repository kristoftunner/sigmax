#!/bin/bash
#
# Download, build and install Boost locally under ext/.
#
# The Ubuntu 22.04 system package is Boost 1.74, which predates Boost.JSON
# (added in 1.75), so we keep a self-contained Boost under ext/ instead.
# Only the compiled libraries we need are built; the full header tree is
# installed so header-only libs (Beast, Asio) are available too.
#
# Usage: scripts/build_boost.sh [--force]

set -euo pipefail

BOOST_VERSION="1.91.0"
BOOST_RELEASE="boost-1.91.0-1" # patched re-release of 1.91.0
BOOST_ARCHIVE="${BOOST_RELEASE}-b2-nodocs.tar.xz"
BOOST_SHA256="7334bb672b9c7aa135da88bcc3111a64a198be9a7d44eb5151d9248e7cfd43ef"
BOOST_URL="https://github.com/boostorg/boost/releases/download/${BOOST_RELEASE}/${BOOST_ARCHIVE}"

# Compiled libraries to build. Beast, Asio and System are header-only as of
# 1.91 (System no longer even appears in `b2 --show-libraries`), so only JSON
# and its Container dependency need building.
BOOST_LIBS="json,container"

# Match the toolchain the project itself is configured with, so the local
# Boost and sigmax agree on ABI and standard library.
CXX_COMPILER="${CXX:-g++-13}"
CXX_STANDARD="23"

SCRIPT_DIR_PATH="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR_PATH="$(dirname -- "${SCRIPT_DIR_PATH}")"
EXT_DIR="${REPO_DIR_PATH}/ext"
SRC_DIR="${EXT_DIR}/${BOOST_RELEASE}" # the archive's top-level directory
BUILD_DIR="${EXT_DIR}/boost-build"
PREFIX_DIR="${EXT_DIR}/boost"

FORCE=0
if [[ "${1:-}" == "--force" ]]; then FORCE=1; fi

if [[ ${FORCE} -eq 0 && -f "${PREFIX_DIR}/include/boost/json.hpp" ]]; then
    echo "==> Boost already installed at ${PREFIX_DIR} (use --force to rebuild)"
    exit 0
fi

mkdir -p "${EXT_DIR}"

# --- download --------------------------------------------------------------
if [[ ! -f "${EXT_DIR}/${BOOST_ARCHIVE}" ]]; then
    echo "==> Downloading ${BOOST_ARCHIVE}"
    curl -fL --progress-bar -o "${EXT_DIR}/${BOOST_ARCHIVE}.part" "${BOOST_URL}"
    mv "${EXT_DIR}/${BOOST_ARCHIVE}.part" "${EXT_DIR}/${BOOST_ARCHIVE}"
fi

echo "==> Verifying checksum"
echo "${BOOST_SHA256}  ${EXT_DIR}/${BOOST_ARCHIVE}" | sha256sum --check --status || {
    echo "<<! Checksum mismatch for ${BOOST_ARCHIVE}"
    exit 1
}

# --- extract ---------------------------------------------------------------
if [[ ! -d "${SRC_DIR}" ]]; then
    echo "==> Extracting to ${SRC_DIR}"
    tar -xf "${EXT_DIR}/${BOOST_ARCHIVE}" -C "${EXT_DIR}"
fi

# --- build -----------------------------------------------------------------
cd "${SRC_DIR}"

if [[ ! -x "${SRC_DIR}/b2" || ! -f "${SRC_DIR}/project-config.jam" ]]; then
    echo "==> Bootstrapping b2"
    ./bootstrap.sh --prefix="${PREFIX_DIR}" --with-libraries="${BOOST_LIBS}"
fi

# b2 resolves a bare `toolset=gcc` to whatever `g++` is on PATH (11.4 here),
# so point it explicitly at the compiler the project uses.
CXX_PATH="$(command -v "${CXX_COMPILER}")"
USER_CONFIG="${EXT_DIR}/user-config.jam"
echo "using gcc : sigmax : ${CXX_PATH} ;" > "${USER_CONFIG}"

echo "==> Building and installing Boost ${BOOST_VERSION} into ${PREFIX_DIR}"
echo "    toolset: ${CXX_PATH} (C++${CXX_STANDARD})"
./b2 install \
    --prefix="${PREFIX_DIR}" \
    --build-dir="${BUILD_DIR}" \
    --user-config="${USER_CONFIG}" \
    --with-json \
    --with-container \
    toolset=gcc-sigmax \
    cxxstd="${CXX_STANDARD}" \
    variant=release \
    link=static \
    threading=multi \
    -j"$(nproc)"

echo "==> Done. Boost ${BOOST_VERSION} installed at ${PREFIX_DIR}"
