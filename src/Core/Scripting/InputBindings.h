#pragma once

struct lua_State;
class UserInputService;

namespace InputBindings {
    void Register(lua_State* L, UserInputService* input);
} // End of Namespace InputBindings