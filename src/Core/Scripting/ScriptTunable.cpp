#include "ScriptTunable.h"
#include <cmath>
#include <iostream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

bool ScriptTunable::Read(float& out) const {
    if (!L || tableRef == LUA_NOREF) return false;

    lua_rawgeti(L, LUA_REGISTRYINDEX, tableRef);
    lua_getfield(L, -1, key.c_str());
    bool ok = false;
    if (kind == Kind::Bool) {
        if (lua_isboolean(L, -1)) {
            out = lua_toboolean(L, -1) ? 1.0f : 0.0f;
            ok = true;
        }
    } else if (lua_type(L, -1) == LUA_TNUMBER) {
        out = static_cast<float>(lua_tonumber(L, -1));
        ok = true;
    }
    lua_pop(L, 2);
    return ok;
}

void ScriptTunable::Write(float value) const {
    if (!L || tableRef == LUA_NOREF) return;
    if (minValue < maxValue) value = value < minValue ? minValue : (value > maxValue ? maxValue : value);

    lua_rawgeti(L, LUA_REGISTRYINDEX, tableRef);
    switch (kind) {
        case Kind::Bool:    lua_pushboolean(L, value != 0.0f); break;
        // A real Lua integer, not 3.0: scripts use these as counts and indices,
        // and math.type would tell the difference.
        case Kind::Integer: lua_pushinteger(L, static_cast<lua_Integer>(std::lround(value))); break;
        default:            lua_pushnumber(L, value); break;
    }
    lua_setfield(L, -2, key.c_str());

    if (onChangeRef != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, onChangeRef);
        lua_pushvalue(L, -2); // the table, as the callback's argument
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            std::cerr << "Engine Warning: onChange for exposed '" << name << "' failed: "
                      << lua_tostring(L, -1) << "\n";
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1); // the table
}
