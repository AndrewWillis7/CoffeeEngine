#include "KeyMap.h"
#include <unordered_map>
#include <iostream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {
    std::unordered_map<std::string, int>& Table() {
        static std::unordered_map<std::string, int> table;
        return table;
    }
}

void KeyMap::LoadAndExposeToLua(lua_State* L, const std::string& path) {
#if defined(_WIN32)
    lua_pushstring(L, "windows");
#else
    lua_pushstring(L, "linux");
#endif
    lua_setglobal(L, "PLATFORM");

    if (luaL_dofile(L, path.c_str()) != LUA_OK) {
        std::cerr << "Engine Warning: KeyMap failed to load '" << path << "': "
                    << lua_tostring(L, -1) << "\n";
        lua_pop(L, 1);
        return;
    }

    // dofile leaves whatever the chunk returned on top of the stack
    if (!lua_istable(L, -1)) {
        std::cerr << "Engine Warning: " << path << " did not return a table\n";
        lua_pop(L, 1);
        return;
    }
    lua_setglobal(L, "Keys");
}

void KeyMap::SyncFromLua(lua_State* L) {
    Table().clear();

    lua_getglobal(L, "Keys");
    if (!lua_istable(L, -1)) {
        std::cerr << "Engine Warning: Keymap::SyncFromLua -- no Keys table on the Lua state\n";
        lua_pop(L, 1);
        return;
    }

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING && lua_isnumber(L, -1)) {
            std::string name = lua_tostring(L, -2);
            int keycode = static_cast<int>(lua_tointeger(L, -1));
            Table()[name] = keycode;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

int KeyMap::Get(const std::string& name) {
    auto it = Table().find(name);
    return it != Table().end() ? it->second : 0;
}