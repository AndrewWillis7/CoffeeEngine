#include "RigidBody2DBindings.h"
#include "LuaBinding.h"
#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Physics/CollisionShape2D.h"
#include "Core/Gameplay/PlayerActorConfig.h"
#include "Renderer/Shader.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

constexpr const char* kMetatableName = "Coffee.RigidBody2D";
constexpr const char* kShaderMetatableName = "Coffee.Shader"; // must match ShaderBindings.cpp
constexpr const char* kCollisionShapeMetatableName = "Coffee.CollisionShape2D"; // must match CollisionShape2DBindings.cpp
constexpr const char* kPlayerConfigMetatableName = "Coffee.PlayerActorConfig"; // must match PlayerActorConfigBindings.cpp
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

RigidBody2D* CheckSelf(lua_State* L, int index) {
    return LuaBinding::CheckPtr<RigidBody2D>(L, index, kMetatableName);
}

// RigidBody2D.new(x, y, width, height) -- captures ActorRegistry* as an
// upvalue, same trick GraphicsBindings uses for IGraphicsContext.
int Lua_New(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_optnumber(L, 3, 50.0));
    float h = static_cast<float>(luaL_optnumber(L, 4, 50.0));

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    RigidBody2D* body = actors->CreateRigidBody(x, y, w, h);
    LuaBinding::PushPtr<RigidBody2D>(L, kMetatableName, body);
    return 1;
}

int Lua_AddForce(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    float fx = static_cast<float>(luaL_checknumber(L, 2));
    float fy = static_cast<float>(luaL_checknumber(L, 3));
    self->AddForce(Vector2(fx, fy));
    return 0;
}

int Lua_Integrate(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    float dt = static_cast<float>(luaL_checknumber(L, 2));
    self->Integrate(dt);
    return 0;
}

int Lua_GetPosition(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    lua_pushnumber(L, self->transform.position.x);
    lua_pushnumber(L, self->transform.position.y);
    return 2;
}

int Lua_SetPosition(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    self->transform.position.x = static_cast<float>(luaL_checknumber(L, 2));
    self->transform.position.y = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

// Rotation crosses the Lua boundary in degrees -- Transform2D itself stores
// radians (see the comment in Core/Math/Transform2D.h).
int Lua_GetRotation(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    lua_pushnumber(L, self->transform.rotation * kRadToDeg);
    return 1;
}

int Lua_SetRotation(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    self->transform.rotation = static_cast<float>(luaL_checknumber(L, 2)) * kDegToRad;
    return 0;
}

int Lua_GetVelocity(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    lua_pushnumber(L, self->velocity.x);
    lua_pushnumber(L, self->velocity.y);
    return 2;
}

int Lua_SetVelocity(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    self->velocity.x = static_cast<float>(luaL_checknumber(L, 2));
    self->velocity.y = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

int Lua_GetAngularVelocity(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    lua_pushnumber(L, self->angularVelocity * kRadToDeg);
    return 1;
}

int Lua_SetAngularVelocity(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    self->angularVelocity = static_cast<float>(luaL_checknumber(L, 2)) * kDegToRad;
    return 0;
}

int Lua_SetSize(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    self->size.x = static_cast<float>(luaL_checknumber(L, 2));
    self->size.y = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

int Lua_SetColor(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    self->color.r = static_cast<float>(luaL_checknumber(L, 2));
    self->color.g = static_cast<float>(luaL_checknumber(L, 3));
    self->color.b = static_cast<float>(luaL_checknumber(L, 4));
    self->color.a = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    return 0;
}

int Lua_SetMass(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    self->mass = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

int Lua_SetDrag(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    self->drag = static_cast<float>(luaL_checknumber(L, 2));
    return 0;
}

// body:SetShader(shader) attaches a shader (created via Shader.new / .CreateGlow);
// body:SetShader(nil) clears it, reverting to Renderer2D's default flat shader.
int Lua_SetShader(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    if (lua_isnoneornil(L, 2)) {
        self->shader = nullptr;
    } else {
        self->shader = LuaBinding::CheckPtr<Shader>(L, 2, kShaderMetatableName);
    }
    return 0;
}

// body:SetCollisionShape(shape) attaches a collider (created via
// CollisionShape2D.NewBox / .NewCircle); body:SetCollisionShape(nil) clears it.
int Lua_SetCollisionShape(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    if (lua_isnoneornil(L, 2)) {
        self->collisionShape = nullptr;
    } else {
        self->collisionShape = LuaBinding::CheckPtr<CollisionShape2D>(L, 2, kCollisionShapeMetatableName);
    }
    return 0;
}

// body:GetCollisionShape() -- returns the attached shape, or nil.
int Lua_GetCollisionShape(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    if (!self->collisionShape) {
        lua_pushnil(L);
    } else {
        LuaBinding::PushPtr<CollisionShape2D>(L, kCollisionShapeMetatableName, self->collisionShape);
    }
    return 1;
}

// body:CollidesWith(otherBody) -- true if both have a collisionShape set
// and those shapes currently overlap.
int Lua_CollidesWith(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    RigidBody2D* other = CheckSelf(L, 2);
    lua_pushboolean(L, self->CollidesWith(*other));
    return 1;
}

// body:SetPlayerConfig(config) marks this body as the player (created via
// PlayerActorConfig.new()); body:SetPlayerConfig(nil) un-marks it.
int Lua_SetPlayerConfig(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    if (lua_isnoneornil(L, 2)) {
        self->playerConfig = nullptr;
    } else {
        self->playerConfig = LuaBinding::CheckPtr<PlayerActorConfig>(L, 2, kPlayerConfigMetatableName);
    }
    return 0;
}

// body:GetPlayerConfig() -- returns the attached config, or nil.
int Lua_GetPlayerConfig(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    if (!self->playerConfig) {
        lua_pushnil(L);
    } else {
        LuaBinding::PushPtr<PlayerActorConfig>(L, kPlayerConfigMetatableName, self->playerConfig);
    }
    return 1;
}

int Lua_IsPlayer(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    lua_pushboolean(L, self->playerConfig != nullptr);
    return 1;
}

// body:ResolveCollisionWith(otherBody) -- if overlapping, pushes both
// bodies apart (mass-weighted; mass <= 0 means immovable, e.g. a wall).
// Box shapes only for now. Returns true if the bodies were overlapping.
int Lua_ResolveCollisionWith(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    RigidBody2D* other = CheckSelf(L, 2);
    lua_pushboolean(L, self->ResolveCollisionWith(*other));
    return 1;
}

// body:ResolveWindowBounds(windowWidth, windowHeight) -- clamps the body
// fully inside [0,0]..[windowWidth,windowHeight], zeroing velocity on
// whichever axis got clamped. Typical use: eWindow:GetWidth()/GetHeight()
// for the current window size. Returns true if a clamp happened.
int Lua_ResolveWindowBounds(lua_State* L) {
    RigidBody2D* self = CheckSelf(L, 1);
    float width = static_cast<float>(luaL_checknumber(L, 2));
    float height = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushboolean(L, self->ResolveWindowBounds(width, height));
    return 1;
}

const luaL_Reg kMethods[] = {
    {"AddForce", Lua_AddForce},
    {"Integrate", Lua_Integrate},
    {"GetPosition", Lua_GetPosition},
    {"SetPosition", Lua_SetPosition},
    {"GetRotation", Lua_GetRotation},
    {"SetRotation", Lua_SetRotation},
    {"GetVelocity", Lua_GetVelocity},
    {"SetVelocity", Lua_SetVelocity},
    {"GetAngularVelocity", Lua_GetAngularVelocity},
    {"SetAngularVelocity", Lua_SetAngularVelocity},
    {"SetSize", Lua_SetSize},
    {"SetColor", Lua_SetColor},
    {"SetMass", Lua_SetMass},
    {"SetDrag", Lua_SetDrag},
    {"SetShader", Lua_SetShader},
    {"SetCollisionShape", Lua_SetCollisionShape},
    {"GetCollisionShape", Lua_GetCollisionShape},
    {"CollidesWith", Lua_CollidesWith},
    {"SetPlayerConfig", Lua_SetPlayerConfig},
    {"GetPlayerConfig", Lua_GetPlayerConfig},
    {"IsPlayer", Lua_IsPlayer},
    {"ResolveCollisionWith", Lua_ResolveCollisionWith},
    {"ResolveWindowBounds", Lua_ResolveWindowBounds},
    {nullptr, nullptr}
};

} // namespace

void RigidBody2DBindings::Register(lua_State* L, ActorRegistry* actors) {
    LuaBinding::RegisterMetatable<RigidBody2D>(L, kMetatableName, kMethods);

    lua_newtable(L);
    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, Lua_New, 1);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "RigidBody2D");
}