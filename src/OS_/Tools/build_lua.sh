#!/bin/bash
set -e

LUA_VERSION="5.4.6"
TARGET_DIR="src/Core/lua"

echo "=== 1. Downloading Lua v${LUA_VERSION} ==="
curl -R -O https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz

echo "=== 2. Extracting source code ==="
tar -zxf lua-${LUA_VERSION}.tar.gz
cd lua-${LUA_VERSION}/src

rm -f lua.c luac.c

echo "=== 3. Compiling Lua C source files ==="
gcc -O2 -Wall -c *.c

echo "=== 4. Archiving static library (liblua.a) ==="
ar rcs liblua.a *.o

echo "=== 5. Installing to ${TARGET_DIR} ==="
mkdir -p "../../${TARGET_DIR}/include"
cp liblua.a "../../${TARGET_DIR}/"
cp lua.h luaconf.h lualib.h lauxlib.h lua.hpp "../../${TARGET_DIR}/include/"

cd ../..
rm -rf lua-${LUA_VERSION} lua-${LUA_VERSION}.tar.gz

echo "Success! liblua.a + headers installed in ${TARGET_DIR}"