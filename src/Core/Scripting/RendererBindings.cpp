#include "RendererBindings.h"
#include "LuaBinding.h"
#include "Core/Physics/RigidBody2D.h"
#include "Renderer/Renderer2D.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

constexpr const char* kBodyMetatableName = "Coffee.RigidBody2D"; // must match RigidBody2DBindings.cpp

// DrawBody(body) -- draws a RigidBody2D through the shader pipeline, using
// whatever shader is currently attached via body:SetShader(...) (or the
// engine's default flat shader if none was set).
int Lua_DrawBody(lua_State* L) {
    auto* body = LuaBinding::CheckPtr<RigidBody2D>(L, 1, kBodyMetatableName);
    auto* renderer = static_cast<Renderer2D*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (renderer) {
        renderer->DrawQuad(body->transform, body->size, body->color, body->shader);
    }
    return 0;
}

} // namespace

void RendererBindings::Register(lua_State* L, Renderer2D* renderer) {
    lua_pushlightuserdata(L, renderer);
    lua_pushcclosure(L, Lua_DrawBody, 1);
    lua_setglobal(L, "DrawBody");
}