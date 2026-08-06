#include "PhysicsBindings.h"
#include "Core/Physics/RigidBody2D.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

// Physics.SetGravity(x, y) -- sets the engine-wide gravity acceleration
// (px/s^2), applied every RigidBody2D::Integrate() call to any body with
// mass > 0. This is a single global value shared by every RigidBody2D
// (not per-instance), so it doesn't need an ActorRegistry upvalue --
// same reason Vector2Bindings.cpp registers its functions plain.
int Lua_SetGravity(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    RigidBody2D::SetGravity(Vector2(x, y));
    return 0;
}

// Physics.GetGravity() -- returns the current gravity vector as (x, y).
int Lua_GetGravity(lua_State* L) {
    Vector2 gravity = RigidBody2D::GetGravity();
    lua_pushnumber(L, gravity.x);
    lua_pushnumber(L, gravity.y);
    return 2;
}

} // namespace

void PhysicsBindings::Register(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, Lua_SetGravity);
    lua_setfield(L, -2, "SetGravity");

    lua_pushcfunction(L, Lua_GetGravity);
    lua_setfield(L, -2, "GetGravity");

    lua_setglobal(L, "Physics");
}