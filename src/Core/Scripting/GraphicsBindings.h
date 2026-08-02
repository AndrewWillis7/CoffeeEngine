#pragma once 

struct lua_State;
class IGraphicsContext;

namespace GraphicsBindings {
    void Register(lua_State* L, IGraphicsContext* graphics);
} // End of Namespace Graphics Bindings