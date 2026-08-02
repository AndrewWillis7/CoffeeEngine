#include "GraphicsBindings.h"
#include "IGraphicsContext.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {
    // LUA Trampoline for ClearColor

int Lua_SetClearColor(lua_State* L) {
    float r = static_cast<float>(luaL_checknumber(L, 1));
    float g = static_cast<float>(luaL_checknumber(L, 2));
    float b = static_cast<float>(luaL_checknumber(L, 3));

    auto* graphics = static_cast<IGraphicsContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (graphics)
        graphics->SetClearColor(r, g, b);

    return 0;
}
} // End of Namespace

void GraphicsBindings::Register(lua_State* L, IGraphicsContext* graphics) {
    lua_pushlightuserdata(L, graphics);
    lua_pushcclosure(L, Lua_SetClearColor, 1);
    lua_setglobal(L, "SetClearColor");
}