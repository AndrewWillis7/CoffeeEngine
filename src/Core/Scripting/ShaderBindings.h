#pragma once

struct lua_State;
class ActorRegistry;

namespace ShaderBindings {
    void Register(lua_State* L, ActorRegistry* actors);
} // End of Namespace ShaderBindings