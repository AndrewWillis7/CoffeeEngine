#pragma once

struct lua_State;
class ActorRegistry;

namespace PlayerActorConfigBindings {
    void Register(lua_State* L, ActorRegistry* actors);
} // End of Namespace PlayerActorConfigBindings