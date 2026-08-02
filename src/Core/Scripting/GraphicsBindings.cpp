#include "GraphicsBindings.h"
#include "IGraphicsContext.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Transform2D.h"
#include "Core/Math/Color.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

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

// LUA Trampoline for DrawDebugQuad
int Lua_DrawDebugQuad(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    float rotationDegrees = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float r = static_cast<float>(luaL_checknumber(L, 6));
    float g = static_cast<float>(luaL_checknumber(L, 7));
    float b = static_cast<float>(luaL_checknumber(L, 8));
    float a = static_cast<float>(luaL_optnumber(L, 9, 1.0));

    auto* graphics = static_cast<IGraphicsContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (graphics) {
        Transform2D transform;
        transform.position = Vector2(x, y);
        transform.rotation = rotationDegrees * kDegToRad;
        graphics->DrawDebugQuad(transform, Vector2(w, h), Color(r, g, b, a));
    }
    return 0;
}

} // End of Namespace

void GraphicsBindings::Register(lua_State* L, IGraphicsContext* graphics) {
    lua_pushlightuserdata(L, graphics);
    lua_pushcclosure(L, Lua_SetClearColor, 1);
    lua_setglobal(L, "SetClearColor");

    lua_pushlightuserdata(L, graphics);
    lua_pushcclosure(L, Lua_DrawDebugQuad, 1);
    lua_setglobal(L, "DrawDebugQuad");
}