#pragma once

struct lua_State;
class ActorRegistry;

namespace CollisionShape2DBindings {
    void Register(lua_State* L, ActorRegistry* actors);
} // End of Namespace CollisionShape2DBindings