#pragma once

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace LuaBinding {
    template <typename T>
    struct PtrUserdata
    {
        T* ptr;
    };
    
    template <typename T>
    void RegisterMetatable(lua_State* L, const char* metatableName, const luaL_Reg* methods) {
        if (luaL_newmetatable(L, metatableName)) {
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
            luaL_setfuncs(L, methods, 0);
        }
        lua_pop(L, 1);
    }

    template <typename T>
    void PushPtr(lua_State* L, const char* metatableName, T* value) {
        auto* ud = static_cast<PtrUserdata<T>*>(lua_newuserdatauv(L, sizeof(PtrUserdata<T>), 0));
        ud->ptr = value;
        luaL_setmetatable(L, metatableName);
    }

    template <typename T>
    T* CheckPtr(lua_State* L, int index, const char* metatableName) {
        return static_cast<PtrUserdata<T>*>(luaL_checkudata(L, index, metatableName))->ptr;
    }
} // End Namespace LuaBinding