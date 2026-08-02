#pragma once

struct lua_State;
class ActorRegistry;

namespace RigidBody2DBindings {
    void Register(lua_State* L, ActorRegistry* actors);
} // End of Namespace RigidBody2DBindings