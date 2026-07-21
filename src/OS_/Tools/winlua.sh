#!/bin/bash

set -e

LUA_VERSION="5.4.6"
TARGET_DIR="src/Core"

echo "=== Downloading Lua v${LUA_VERSION} ==="
curl -L -O "https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz"

echo "=== Extracting source ==="
tar -xzf "lua-${LUA_VERSION}.tar.gz"

cd "lua-${LUA_VERSION}/src"

echo "=== Removing CLI sources ==="
rm -f lua.c luac.c

echo "=== Compiling Lua ==="
gcc -O2 -Wall -c *.c

echo "=== Building lua54.dll ==="
gcc -shared -o lua54.dll *.o \
    -Wl,--out-implib,liblua54.a

echo "=== Copying output ==="
mkdir -p "../../${TARGET_DIR}"

cp lua54.dll "../../${TARGET_DIR}/"
cp liblua54.a "../../${TARGET_DIR}/"

cd ../..

echo "=== Cleaning up ==="
rm -rf "lua-${LUA_VERSION}"
rm -f "lua-${LUA_VERSION}.tar.gz"

echo "Success! lua54.dll created in ${TARGET_DIR}"