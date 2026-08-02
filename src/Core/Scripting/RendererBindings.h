#pragma once

struct lua_State;
class Renderer2D;

namespace RendererBindings {
    void Register(lua_State* L, Renderer2D* renderer);
} // End of Namespace RendererBindings