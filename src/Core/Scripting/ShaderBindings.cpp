#include "ShaderBindings.h"
#include "LuaBinding.h"
#include "Core/ActorRegistry.h"
#include "Renderer/Shader.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

// Must match RigidBody2DBindings.cpp's kShaderMetatableName.
constexpr const char* kMetatableName = "Coffee.Shader";

Shader* CheckSelf(lua_State* L, int index) {
    return LuaBinding::CheckPtr<Shader>(L, index, kMetatableName);
}

// Shader.new(vertexSource, fragmentSource) -- for hand-written GLSL.
int Lua_New(lua_State* L) {
    const char* vertexSrc = luaL_checkstring(L, 1);
    const char* fragmentSrc = luaL_checkstring(L, 2);

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    Shader* shader = actors->CreateShader(vertexSrc, fragmentSrc);
    LuaBinding::PushPtr<Shader>(L, kMetatableName, shader);
    return 1;
}

// Shader.CreateGlow() -- the engine's built-in glow effect, ready to attach
// to a RigidBody2D via body:SetShader(...). Tweak it further with
// SetVec3("u_GlowColor", ...) / SetFloat("u_GlowIntensity", ...).
int Lua_CreateGlow(lua_State* L) {
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    Shader* shader = actors->CreateGlowShader();
    LuaBinding::PushPtr<Shader>(L, kMetatableName, shader);
    return 1;
}

int Lua_SetFloat(lua_State* L) {
    Shader* self = CheckSelf(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    self->SetFloat(name, value);
    return 0;
}

int Lua_SetVec2(lua_State* L) {
    Shader* self = CheckSelf(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    self->SetVec2(name, x, y);
    return 0;
}

int Lua_SetVec3(lua_State* L) {
    Shader* self = CheckSelf(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    self->SetVec3(name, x, y, z);
    return 0;
}

int Lua_SetVec4(lua_State* L) {
    Shader* self = CheckSelf(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    float w = static_cast<float>(luaL_checknumber(L, 6));
    self->SetVec4(name, x, y, z, w);
    return 0;
}

int Lua_SetOverdrawScale(lua_State* L) {
    Shader* self = CheckSelf(L, 1);
    self->overdrawScale = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

const luaL_Reg kMethods[] = {
    {"SetFloat", Lua_SetFloat},
    {"SetVec2", Lua_SetVec2},
    {"SetVec3", Lua_SetVec3},
    {"SetVec4", Lua_SetVec4},
    {"SetOverdrawScale", Lua_SetOverdrawScale},
    {nullptr, nullptr}
};

} // namespace

void ShaderBindings::Register(lua_State* L, ActorRegistry* actors) {
    LuaBinding::RegisterMetatable<Shader>(L, kMetatableName, kMethods);

    lua_newtable(L);

    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, Lua_New, 1);
    lua_setfield(L, -2, "new");

    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, Lua_CreateGlow, 1);
    lua_setfield(L, -2, "CreateGlow");

    lua_setglobal(L, "Shader");
}