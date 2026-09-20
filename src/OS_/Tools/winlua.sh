#!/bin/bash
# Builds a STATIC Lua library for Windows -> src/Core/lua/win64/liblua.a
# Run from the MSYS2 UCRT64 shell (or via build_lua.bat). Works from any
# working directory. Headers are NOT copied: src/Core/lua/include is shared
# with the Linux build and must stay at the same Lua version (5.4.6).
#
# Cross-compiling from Linux instead:
#   CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar src/OS_/Tools/winlua.sh

set -e

LUA_VERSION="5.4.6"
CC="${CC:-gcc}"
AR="${AR:-ar}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TARGET_DIR="${REPO_ROOT}/src/Core/lua/win64"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT
cd "${WORK_DIR}"

echo "=== Downloading Lua v${LUA_VERSION} ==="
curl -fL -O "https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz"

echo "=== Extracting source ==="
tar -xzf "lua-${LUA_VERSION}.tar.gz"
cd "lua-${LUA_VERSION}/src"

echo "=== Removing CLI sources ==="
rm -f lua.c luac.c

echo "=== Compiling Lua (${CC}) ==="
# No LUA_BUILD_AS_DLL: static archive, so the vendored headers (which
# don't define it) declare LUA_API as plain extern -- they must agree.
"${CC}" -O2 -Wall -c *.c

echo "=== Archiving static library ==="
"${AR}" rcs liblua.a *.o

mkdir -p "${TARGET_DIR}"
cp liblua.a "${TARGET_DIR}/"

echo "Success! liblua.a installed in ${TARGET_DIR}"