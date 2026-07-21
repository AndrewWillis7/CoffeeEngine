#!/bin/bash

# NOTE: This builds a portable Lua shared library (.so) for your engine subsystem

# Exit immediately if any command fails
set -e

LUA_VERSION="5.4.6"
TARGET_DIR="src/Core"

echo "=== 1. Downloading Lua v${LUA_VERSION} ==="
curl -R -O https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz

echo "=== 2. Extracting source code ==="
tar -zxf lua-${LUA_VERSION}.tar.gz
cd lua-${LUA_VERSION}/src

# Remove CLI binaries so we only compile the library core
rm -f lua.c luac.c

echo "=== 3. Compiling Lua C source files with position-independent code ==="
gcc -O2 -Wall -c -fPIC *.c

echo "=== 4. Creating shared library (liblua.so) ==="
gcc -shared -o liblua.so *.o

echo "=== 5. Moving liblua.so to ${TARGET_DIR} ==="
mkdir -p "../../${TARGET_DIR}"
cp liblua.so "../../${TARGET_DIR}/liblua.so"

echo "=== 6. Cleaning up temporary build files ==="
cd ../..
rm -rf lua-${LUA_VERSION} lua-${LUA_VERSION}.tar.gz

echo "Success! liblua.so is now built and placed in ${TARGET_DIR}."