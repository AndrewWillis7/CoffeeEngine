#include "PlayerActorConfigBindings.h"
#include "LuaBinding.h"
#include "Core/ActorRegistry.h"
#include "Core/Gameplay/PlayerActorConfig.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

// Must match kPlayerConfigMetatableName in RigidBody2DBindings.cpp
constexpr const char* kMetatableName = "Coffee.PlayerActorConfig";

PlayerActorConfig* CheckSelf(lua_State* L, int index) {
    return LuaBinding::CheckPtr<PlayerActorConfig>(L, index, kMetatableName);
}

// PlayerActorConfig.new()
int Lua_New(lua_State* L) {
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    PlayerActorConfig* config = actors->CreatePlayerConfig();
    LuaBinding::PushPtr<PlayerActorConfig>(L, kMetatableName, config);
    return 1;
}

int Lua_SetMoveSpeed(lua_State* L) {
    PlayerActorConfig* self = CheckSelf(L, 1);
    self->moveSpeed = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

int Lua_GetMoveSpeed(lua_State* L) {
    PlayerActorConfig* self = CheckSelf(L, 1);
    lua_pushnumber(L, self->moveSpeed);
    return 1;
}

int Lua_SetJumpForce(lua_State* L) {
    PlayerActorConfig* self = CheckSelf(L, 1);
    self->jumpForce = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

int Lua_GetJumpForce(lua_State* L) {
    PlayerActorConfig* self = CheckSelf(L, 1);
    lua_pushnumber(L, self->jumpForce);
    return 1;
}

int Lua_SetInputEnabled(lua_State* L) {
    PlayerActorConfig* self = CheckSelf(L, 1);
    self->inputEnabled = lua_toboolean(L, 2);
    return 0;
}

int Lua_IsInputEnabled(lua_State* L) {
    PlayerActorConfig* self = CheckSelf(L, 1);
    lua_pushboolean(L, self->inputEnabled);
    return 1;
}

const luaL_Reg kMethods[] = {
    {"SetMoveSpeed", Lua_SetMoveSpeed},
    {"GetMoveSpeed", Lua_GetMoveSpeed},
    {"SetJumpForce", Lua_SetJumpForce},
    {"GetJumpForce", Lua_GetJumpForce},
    {"SetInputEnabled", Lua_SetInputEnabled},
    {"IsInputEnabled", Lua_IsInputEnabled},
    {nullptr, nullptr}
};

} // namespace

void PlayerActorConfigBindings::Register(lua_State* L, ActorRegistry* actors) {
    LuaBinding::RegisterMetatable<PlayerActorConfig>(L, kMetatableName, kMethods);

    lua_newtable(L);
    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, Lua_New, 1);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "PlayerActorConfig");
}