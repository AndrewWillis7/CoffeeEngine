#include "ActorRegistryBindings.h"
#include "LuaBinding.h"
#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

constexpr const char* kBodyMetatableName = "Coffee.RigidBody2D"; // must match RigidBody2DBindings.cpp

// Actors.GetPlayer() -- returns the RigidBody2D marked with a
// PlayerActorConfig (see body:SetPlayerConfig), or nil if none exists yet.
int Lua_GetPlayer(lua_State* L) {
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    RigidBody2D* player = actors ? actors->GetPlayerActor() : nullptr;

    if (!player) {
        lua_pushnil(L);
    } else {
        LuaBinding::PushPtr<RigidBody2D>(L, kBodyMetatableName, player);
    }
    return 1;
}

// Actors.Dump() -- prints every RigidBody2D and its attached
// Shader/CollisionShape/PlayerActorConfig to stdout. Stand-in for a real
// inspector/tree view until one exists.
int Lua_Dump(lua_State* L) {
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (actors) actors->DumpTree();
    return 0;
}

} // namespace

void ActorRegistryBindings::Register(lua_State* L, ActorRegistry* actors) {
    lua_newtable(L);

    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, Lua_GetPlayer, 1);
    lua_setfield(L, -2, "GetPlayer");

    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, Lua_Dump, 1);
    lua_setfield(L, -2, "Dump");

    lua_setglobal(L, "Actors");
}