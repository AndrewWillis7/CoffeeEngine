#include "CollisionShape2DBindings.h"
#include "LuaBinding.h"
#include "Core/ActorRegistry.h"
#include "Core/Physics/CollisionShape2D.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

// Must match kCollisionShapeMetatableName in RigidBody2DBindings.cpp
constexpr const char* kMetatableName = "Coffee.CollisionShape2D";

CollisionShape2D* CheckSelf(lua_State* L, int index) {
    return LuaBinding::CheckPtr<CollisionShape2D>(L, index, kMetatableName);
}

// CollisionShape2D.NewBox(halfWidth, halfHeight, [offsetX], [offsetY])
int Lua_NewBox(lua_State* L) {
    float hw = static_cast<float>(luaL_checknumber(L, 1));
    float hh = static_cast<float>(luaL_checknumber(L, 2));
    float ox = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 4, 0.0));

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    CollisionShape2D* shape = actors->CreateBoxCollisionShape(hw, hh, ox, oy);
    LuaBinding::PushPtr<CollisionShape2D>(L, kMetatableName, shape);
    return 1;
}

// CollisionShape2D.NewCircle(radius, [offsetX], [offsetY])
int Lua_NewCircle(lua_State* L) {
    float radius = static_cast<float>(luaL_checknumber(L, 1));
    float ox = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 3, 0.0));

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    CollisionShape2D* shape = actors->CreateCircleCollisionShape(radius, ox, oy);
    LuaBinding::PushPtr<CollisionShape2D>(L, kMetatableName, shape);
    return 1;
}

int Lua_GetType(lua_State* L) {
    CollisionShape2D* self = CheckSelf(L, 1);
    lua_pushstring(L, self->GetType() == CollisionShape2D::Type::Box ? "Box" : "Circle");
    return 1;
}

const luaL_Reg kMethods[] = {
    {"GetType", Lua_GetType},
    {nullptr, nullptr}
};

} // namespace

void CollisionShape2DBindings::Register(lua_State* L, ActorRegistry* actors) {
    LuaBinding::RegisterMetatable<CollisionShape2D>(L, kMetatableName, kMethods);

    lua_newtable(L);

    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, Lua_NewBox, 1);
    lua_setfield(L, -2, "NewBox");

    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, Lua_NewCircle, 1);
    lua_setfield(L, -2, "NewCircle");

    lua_setglobal(L, "CollisionShape2D");
}