#pragma once
#include <new>

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

    // --- Value Types (LUA constructs and owns the instance, e.g. Vector2) ---

    template <typename T>
    void RegisterValueMetatable(lua_State* L, const char* metatableName, const luaL_Reg* methods) {
        if (luaL_newmetatable(L, metatableName)) {
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
            luaL_setfuncs(L, methods, 0);

            lua_pushcfunction(L, [](lua_State* gcL) -> int {
                static_cast<T*>(lua_touserdata(gcL, 1))->~T();
                return 0;
            });
            lua_setfield(L, -2, "__gc");
        }
        lua_pop(L, 1);
    }

    template <typename T>
    void PushNew(lua_State* L, const char* metatableName, T value) {
        void* mem = lua_newuserdatauv(L, sizeof(T), 0);
        new (mem) T(value);
        luaL_setmetatable(L, metatableName);
    }

    template <typename T>
    T* CheckValue(lua_State* L, int index, const char* metatableName) {
        return static_cast<T*>(luaL_checkudata(L, index, metatableName));
    }

} // End Namespace LuaBinding