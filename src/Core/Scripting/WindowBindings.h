#pragma once

struct lua_State;
class IWindow;

namespace WindowBindings {
    void Register(lua_State* L, IWindow* window);
} // End Namespace WindowsBindings
