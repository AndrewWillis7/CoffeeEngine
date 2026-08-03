#pragma once

struct lua_State;
class ActorRegistry;

namespace ActorRegistryBindings {
    void Register(lua_State* L, ActorRegistry* actors);
} // End of Namespace ActorRegistryBindings