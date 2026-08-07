#include "ScriptBindings.h"
#include "LuaBinding.h"
#include "Core/EngineContext.h"

#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Physics/CollisionShape2D.h"
#include "Core/Gameplay/PlayerActorConfig.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Transform2D.h"
#include "Core/Math/Color.h"
#include "Core/Input/UserInputService.h"
#include "Renderer/Shader.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/PixelSprite.h"
#include "IGraphicsContext.h"
#include "IWindow.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// =====================================================================
// Type registry -- every bound type's metatable name and value/pointer
// kind, in one place, ahead of all the binding code below that uses it.
// This used to be a `kMetatableName` constant copy-pasted (with a
// "must match" comment) into every file that touched the type; now it's
// one line, written once, and the compiler enforces the "must match"
// part for free.
// =====================================================================
namespace LuaBinding {
    template <> struct MetatableOf<Vector2>           { static constexpr const char* name = "Coffee.Vector2"; };
    template <> struct MetatableOf<RigidBody2D>        { static constexpr const char* name = "Coffee.RigidBody2D"; };
    template <> struct MetatableOf<Shader>              { static constexpr const char* name = "Coffee.Shader"; };
    template <> struct MetatableOf<CollisionShape2D>    { static constexpr const char* name = "Coffee.CollisionShape2D"; };
    template <> struct MetatableOf<PlayerActorConfig>   { static constexpr const char* name = "Coffee.PlayerActorConfig"; };
    template <> struct MetatableOf<IWindow>             { static constexpr const char* name = "Coffee.IWindow"; };
    template <> struct MetatableOf<PixelSprite>         { static constexpr const char* name = "Coffee.PixelSprite"; };

    template <> struct IsValueType<Vector2> : std::true_type {};

    // Project-specific scalar conversion -- MouseButton is an engine enum,
    // not something the generic template header should know about, but
    // the trait system is extensible from any translation unit.
    template <> struct Value<MouseButton> {
        static MouseButton Get(lua_State* L, int idx) { return static_cast<MouseButton>(luaL_checkinteger(L, idx)); }
        static void Push(lua_State* L, MouseButton v) { lua_pushinteger(L, static_cast<int>(v)); }
    };
}

namespace {

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

// =====================================================================
// Vector2 -- value type. Operators map straight onto Method<>; __mul's
// operand-order ambiguity and the custom __tostring format are the only
// two things that need a hand-written trampoline.
// =====================================================================

int Lua_Vector2New(lua_State* L) {
    float x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    LuaBinding::PushResult(L, Vector2(x, y));
    return 1;
}

int Lua_Vector2Set(lua_State* L) {
    Vector2* self = LuaBinding::GetSelf<Vector2>(L, 1);
    self->x = static_cast<float>(luaL_checknumber(L, 2));
    self->y = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

// Handles both `vec * number` and `number * vec` -- Lua calls __mul with
// whichever operand order was written, and only one side is guaranteed
// to be our userdata.
int Lua_Vector2Mul(lua_State* L) {
    bool firstIsVector = lua_isuserdata(L, 1);
    Vector2* vec = firstIsVector ? LuaBinding::GetSelf<Vector2>(L, 1) : LuaBinding::GetSelf<Vector2>(L, 2);
    float scalar = static_cast<float>(luaL_checknumber(L, firstIsVector ? 2 : 1));
    LuaBinding::PushResult(L, *vec * scalar);
    return 1;
}

int Lua_Vector2ToString(lua_State* L) {
    Vector2* self = LuaBinding::GetSelf<Vector2>(L, 1);
    lua_pushfstring(L, "Vector2(%f, %f)", static_cast<double>(self->x), static_cast<double>(self->y));
    return 1;
}

void RegisterVector2(lua_State* L) {
    LuaBinding::Class<Vector2>(L, LuaBinding::MetatableOf<Vector2>::name)
        .Property<&Vector2::x>("GetX", "SetX")
        .Property<&Vector2::y>("GetY", "SetY")
        .Raw("Set", &Lua_Vector2Set)
        .Method<&Vector2::Length>("Length")
        .Method<&Vector2::LengthSquared>("LengthSquared")
        .Method<&Vector2::Normalized>("Normalized")
        .Method<&Vector2::Dot>("Dot")
        .Method<&Vector2::Distance>("Distance")
        .Method<&Vector2::operator+>("__add")
        // operator- is overloaded (binary AND unary negate), so it needs a
        // disambiguating cast -- everything else above has only one overload.
        .Method<static_cast<Vector2(Vector2::*)(const Vector2&) const>(&Vector2::operator-)>("__sub")
        .Method<&Vector2::operator==>("__eq")
        .Raw("__mul", &Lua_Vector2Mul)
        .Raw("__tostring", &Lua_Vector2ToString)
        .Finish();

    LuaBinding::Table(L).Raw("new", &Lua_Vector2New).Finish("Vector2");
}

// =====================================================================
// RigidBody2D -- pointer type, owned by ActorRegistry. Most of its
// surface is direct field access (Property/Vec2Property/PtrProperty) or
// a plain method call; only position/rotation (nested inside `transform`,
// rotation also unit-converted) and the optional-size constructor need a
// hand-written trampoline.
// =====================================================================

int Lua_RigidBody2DNew(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_optnumber(L, 3, 50.0));
    float h = static_cast<float>(luaL_optnumber(L, 4, 50.0));

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    LuaBinding::Value<RigidBody2D*>::Push(L, actors->CreateRigidBody(x, y, w, h));
    return 1;
}

int Lua_RigidBody2DGetPosition(lua_State* L) {
    RigidBody2D* self = LuaBinding::GetSelf<RigidBody2D>(L, 1);
    lua_pushnumber(L, self->transform.position.x);
    lua_pushnumber(L, self->transform.position.y);
    return 2;
}

int Lua_RigidBody2DSetPosition(lua_State* L) {
    RigidBody2D* self = LuaBinding::GetSelf<RigidBody2D>(L, 1);
    self->transform.position.x = static_cast<float>(luaL_checknumber(L, 2));
    self->transform.position.y = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

// Rotation crosses the Lua boundary in degrees -- Transform2D itself stores
// radians (see the comment in Core/Math/Transform2D.h).
int Lua_RigidBody2DGetRotation(lua_State* L) {
    lua_pushnumber(L, LuaBinding::GetSelf<RigidBody2D>(L, 1)->transform.rotation * kRadToDeg);
    return 1;
}

int Lua_RigidBody2DSetRotation(lua_State* L) {
    LuaBinding::GetSelf<RigidBody2D>(L, 1)->transform.rotation = static_cast<float>(luaL_checknumber(L, 2)) * kDegToRad;
    return 0;
}

int Lua_RigidBody2DSetColor(lua_State* L) {
    RigidBody2D* self = LuaBinding::GetSelf<RigidBody2D>(L, 1);
    self->color.r = static_cast<float>(luaL_checknumber(L, 2));
    self->color.g = static_cast<float>(luaL_checknumber(L, 3));
    self->color.b = static_cast<float>(luaL_checknumber(L, 4));
    self->color.a = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    return 0;
}

int Lua_RigidBody2DIsPlayer(lua_State* L) {
    lua_pushboolean(L, LuaBinding::GetSelf<RigidBody2D>(L, 1)->playerConfig != nullptr);
    return 1;
}

int Lua_RigidBody2DGetSprite(lua_State* L) {
    LuaBinding::Value<PixelSprite*>::Push(L, LuaBinding::GetSelf<RigidBody2D>(L, 1)->sprite);
    return 1;
}

// Hand-written rather than a plain PtrProperty because attaching a sprite
// also defaults the body's draw size to the sprite's native pixel size --
// PtrProperty's generated setter is a bare field assignment with no room
// for that. SetSize() afterward still overrides this like any other body.
int Lua_RigidBody2DSetSprite(lua_State* L) {
    RigidBody2D* self = LuaBinding::GetSelf<RigidBody2D>(L, 1);
    PixelSprite* sprite = LuaBinding::Value<PixelSprite*>::Get(L, 2);
    self->sprite = sprite;
    if (sprite) self->size = Vector2(static_cast<float>(sprite->GetWidth()), static_cast<float>(sprite->GetHeight()));
    return 0;
}

void RegisterRigidBody2D(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<RigidBody2D>(L, LuaBinding::MetatableOf<RigidBody2D>::name)
        .Raw("GetPosition", &Lua_RigidBody2DGetPosition)
        .Raw("SetPosition", &Lua_RigidBody2DSetPosition)
        .Raw("GetRotation", &Lua_RigidBody2DGetRotation)
        .Raw("SetRotation", &Lua_RigidBody2DSetRotation)
        .Raw("SetColor", &Lua_RigidBody2DSetColor)
        .Raw("IsPlayer", &Lua_RigidBody2DIsPlayer)
        .Raw("GetSprite", &Lua_RigidBody2DGetSprite)
        .Raw("SetSprite", &Lua_RigidBody2DSetSprite)
        .Vec2Property<&RigidBody2D::velocity>("GetVelocity", "SetVelocity")
        .Vec2Property<&RigidBody2D::size>("GetSize", "SetSize")
        .Property<&RigidBody2D::mass>("GetMass", "SetMass")
        .Property<&RigidBody2D::drag>("GetDrag", "SetDrag")
        .ScaledProperty<&RigidBody2D::angularVelocity, kRadToDeg, kDegToRad>("GetAngularVelocity", "SetAngularVelocity")
        .PtrProperty<&RigidBody2D::shader>("GetShader", "SetShader")
        .PtrProperty<&RigidBody2D::collisionShape>("GetCollisionShape", "SetCollisionShape")
        .PtrProperty<&RigidBody2D::playerConfig>("GetPlayerConfig", "SetPlayerConfig")
        .Method<&RigidBody2D::AddForce>("AddForce")
        .Method<&RigidBody2D::Integrate>("Integrate")
        .Method<&RigidBody2D::IsGrounded>("IsGrounded")
        .Method<&RigidBody2D::CollidesWith>("CollidesWith")
        .Method<&RigidBody2D::ResolveCollisionWith>("ResolveCollisionWith")
        .Method<&RigidBody2D::ResolveWindowBounds>("ResolveWindowBounds")
        .Finish();

    LuaBinding::Table(L).RawWithContext("new", actors, &Lua_RigidBody2DNew).Finish("RigidBody2D");
}

// =====================================================================
// Shader -- pointer type, owned by ActorRegistry. new()/CreateGlow() and
// every SetX uniform setter map 1:1 onto ActorRegistry/Shader methods, so
// nothing here needs a hand-written trampoline at all.
// =====================================================================

void RegisterShader(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<Shader>(L, LuaBinding::MetatableOf<Shader>::name)
        .Method<&Shader::SetFloat>("SetFloat")
        .Method<&Shader::SetVec2>("SetVec2")
        .Method<&Shader::SetVec3>("SetVec3")
        .Method<&Shader::SetVec4>("SetVec4")
        .Property<&Shader::overdrawScale>("GetOverdrawScale", "SetOverdrawScale")
        .Finish();

    LuaBinding::Table(L)
        .Function<&ActorRegistry::CreateShader>("new", actors)
        .Function<&ActorRegistry::CreateGlowShader>("CreateGlow", actors)
        .Finish("Shader");
}

// =====================================================================
// CollisionShape2D -- pointer type, owned by ActorRegistry. NewBox/
// NewCircle both have optional offset args backed by C++ default
// parameters, which a function pointer's type can't see (defaults aren't
// part of the type), so those two stay hand-written; GetType's enum ->
// string mapping is a one-off too.
// =====================================================================

int Lua_CollisionShape2DNewBox(lua_State* L) {
    float hw = static_cast<float>(luaL_checknumber(L, 1));
    float hh = static_cast<float>(luaL_checknumber(L, 2));
    float ox = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 4, 0.0));

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    LuaBinding::Value<CollisionShape2D*>::Push(L, actors->CreateBoxCollisionShape(hw, hh, ox, oy));
    return 1;
}

int Lua_CollisionShape2DNewCircle(lua_State* L) {
    float radius = static_cast<float>(luaL_checknumber(L, 1));
    float ox = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float oy = static_cast<float>(luaL_optnumber(L, 3, 0.0));

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    LuaBinding::Value<CollisionShape2D*>::Push(L, actors->CreateCircleCollisionShape(radius, ox, oy));
    return 1;
}

int Lua_CollisionShape2DGetType(lua_State* L) {
    CollisionShape2D* self = LuaBinding::GetSelf<CollisionShape2D>(L, 1);
    lua_pushstring(L, self->GetType() == CollisionShape2D::Type::Box ? "Box" : "Circle");
    return 1;
}

void RegisterCollisionShape2D(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<CollisionShape2D>(L, LuaBinding::MetatableOf<CollisionShape2D>::name)
        .Raw("GetType", &Lua_CollisionShape2DGetType)
        .Finish();

    LuaBinding::Table(L)
        .RawWithContext("NewBox", actors, &Lua_CollisionShape2DNewBox)
        .RawWithContext("NewCircle", actors, &Lua_CollisionShape2DNewCircle)
        .Finish("CollisionShape2D");
}

// =====================================================================
// PixelSprite -- pointer type, owned by ActorRegistry (see the
// m_PixelSprites comment in ActorRegistry.h for why it's exempt from
// Clear()). GetWidth/GetHeight/Flush map 1:1 onto methods; PunchCircle/
// SetPixel/IsSolid take plain numbers rather than a Vector2/Color
// userdata (matches every other hot-path scalar-shaped call in this
// file, e.g. Lua_RigidBody2DSetColor), so all three stay hand-written.
// =====================================================================

int Lua_PixelSpriteLoad(lua_State* L) {
    const char* filepath = luaL_checkstring(L, 1);

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    LuaBinding::Value<PixelSprite*>::Push(L, actors->GetOrLoadPixelSprite(filepath));
    return 1;
}

int Lua_PixelSpritePunchCircle(lua_State* L) {
    PixelSprite* self = LuaBinding::GetSelf<PixelSprite>(L, 1);
    int cx = static_cast<int>(luaL_checknumber(L, 2));
    int cy = static_cast<int>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    self->PunchCircle(cx, cy, radius);
    return 0;
}

int Lua_PixelSpriteSetPixel(lua_State* L) {
    PixelSprite* self = LuaBinding::GetSelf<PixelSprite>(L, 1);
    int x = static_cast<int>(luaL_checknumber(L, 2));
    int y = static_cast<int>(luaL_checknumber(L, 3));
    float r = static_cast<float>(luaL_checknumber(L, 4));
    float g = static_cast<float>(luaL_checknumber(L, 5));
    float b = static_cast<float>(luaL_checknumber(L, 6));
    float a = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    self->SetPixel(x, y, Color(r, g, b, a));
    return 0;
}

int Lua_PixelSpriteIsSolid(lua_State* L) {
    PixelSprite* self = LuaBinding::GetSelf<PixelSprite>(L, 1);
    int x = static_cast<int>(luaL_checknumber(L, 2));
    int y = static_cast<int>(luaL_checknumber(L, 3));
    lua_pushboolean(L, self->IsSolid(x, y));
    return 1;
}

void RegisterPixelSprite(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<PixelSprite>(L, LuaBinding::MetatableOf<PixelSprite>::name)
        .Method<&PixelSprite::GetWidth>("GetWidth")
        .Method<&PixelSprite::GetHeight>("GetHeight")
        .Method<&PixelSprite::Flush>("Flush")
        .Raw("PunchCircle", &Lua_PixelSpritePunchCircle)
        .Raw("SetPixel", &Lua_PixelSpriteSetPixel)
        .Raw("IsSolid", &Lua_PixelSpriteIsSolid)
        .Finish();

    LuaBinding::Table(L).RawWithContext("Load", actors, &Lua_PixelSpriteLoad).Finish("Sprite");
}

// =====================================================================
// PlayerActorConfig -- pointer type, owned by ActorRegistry. Every field
// is a direct public float/bool, so this whole binding is Property<>
// calls plus a zero-arg factory.
// =====================================================================

void RegisterPlayerActorConfig(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<PlayerActorConfig>(L, LuaBinding::MetatableOf<PlayerActorConfig>::name)
        .Property<&PlayerActorConfig::moveSpeed>("GetMoveSpeed", "SetMoveSpeed")
        .Property<&PlayerActorConfig::jumpForce>("GetJumpForce", "SetJumpForce")
        .Property<&PlayerActorConfig::inputEnabled>("IsInputEnabled", "SetInputEnabled")
        .Finish();

    LuaBinding::Table(L).Function<&ActorRegistry::CreatePlayerConfig>("new", actors).Finish("PlayerActorConfig");
}

// =====================================================================
// Graphics -- bare globals (SetClearColor(...), not Graphics.SetClearColor),
// bound to a captured IGraphicsContext*. SetClearColor maps 1:1 onto the
// C++ method; DrawDebugQuad assembles a Transform2D/Vector2/Color out of
// 9 scalar Lua args (plus a degrees->radians conversion), which isn't a
// positional 1:1 mapping, so it stays hand-written.
// =====================================================================

int Lua_DrawDebugQuad(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    float h = static_cast<float>(luaL_checknumber(L, 4));
    float rotationDegrees = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float r = static_cast<float>(luaL_checknumber(L, 6));
    float g = static_cast<float>(luaL_checknumber(L, 7));
    float b = static_cast<float>(luaL_checknumber(L, 8));
    float a = static_cast<float>(luaL_optnumber(L, 9, 1.0));

    auto* graphics = static_cast<IGraphicsContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (graphics) {
        Transform2D transform;
        transform.position = Vector2(x, y);
        transform.rotation = rotationDegrees * kDegToRad;
        graphics->DrawDebugQuad(transform, Vector2(w, h), Color(r, g, b, a));
    }
    return 0;
}

void RegisterGraphics(lua_State* L, IGraphicsContext* graphics) {
    LuaBinding::BindFunction<&IGraphicsContext::SetClearColor>(L, "SetClearColor", graphics);
    LuaBinding::BindRawFunction(L, "DrawDebugQuad", graphics, &Lua_DrawDebugQuad);
}

// =====================================================================
// Window -- eWindow global. GetWidth/GetHeight/SetIcon map straight onto
// IWindow's (virtual) methods -- pointer-to-member-function dispatch is
// virtual automatically, no special-casing needed for that.
// =====================================================================

void RegisterWindow(lua_State* L, IWindow* window) {
    LuaBinding::Class<IWindow>(L, LuaBinding::MetatableOf<IWindow>::name)
        .Method<&IWindow::GetWidth>("GetWidth")
        .Method<&IWindow::GetHeight>("GetHeight")
        .Method<&IWindow::SetIcon>("SetIcon")
        .Finish();

    LuaBinding::Value<IWindow*>::Push(L, window);
    lua_setglobal(L, "eWindow");
}

// =====================================================================
// Renderer -- bare global DrawBody(body). Pulls transform/size/color/
// shader off the RigidBody2D and forwards to DrawQuad, or -- if a
// PixelSprite is attached -- flushes its pending edits and forwards to
// DrawTexturedQuad instead. Needs both a Renderer2D* (to draw) and an
// ActorRegistry* (to resolve the "Textured" named shader for sprite
// bodies that never had an explicit shader set), so this bypasses
// BindRawFunction/Table::RawWithContext -- both only support one
// captured context pointer -- and pushes both closures by hand.
// =====================================================================

int Lua_DrawBody(lua_State* L) {
    auto* body = LuaBinding::Value<RigidBody2D*>::Get(L, 1);
    auto* renderer = static_cast<Renderer2D*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(2)));
    if (!renderer) return 0;

    if (body->sprite) {
        // Uploads any SetPixel/PunchCircle edits made earlier this frame
        // before we draw, so a punch and its DrawBody() in the same
        // Update() call show up in the same frame instead of one frame late.
        body->sprite->Flush();
        Shader* texShader = body->shader ? body->shader : (actors ? actors->GetOrCreateNamedShader("Textured") : nullptr);
        if (texShader) {
            renderer->DrawTexturedQuad(body->transform, body->size, body->color, texShader, body->sprite->GetTexture());
            return 0;
        }
    }

    renderer->DrawQuad(body->transform, body->size, body->color, body->shader);
    return 0;
}

void RegisterRenderer(lua_State* L, Renderer2D* renderer, ActorRegistry* actors) {
    lua_pushlightuserdata(L, renderer);
    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, &Lua_DrawBody, 2);
    lua_setglobal(L, "DrawBody");
}

// =====================================================================
// Actors -- table of ActorRegistry-wide queries, both mechanical.
// =====================================================================

void RegisterActorRegistry(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Table(L)
        .Function<&ActorRegistry::GetPlayerActor>("GetPlayer", actors)
        .Function<&ActorRegistry::DumpTree>("Dump", actors)
        .Finish("Actors");
}

// =====================================================================
// Input -- table bound to a captured UserInputService*. Every Is*
// query maps 1:1 onto a method call; GetMousePosition/
// GetKeysPressedThisFrame return shapes (raw x,y / a Lua array table)
// that don't match a mechanical single push, so they stay hand-written.
// =====================================================================

int Lua_InputGetMousePosition(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    Vector2 pos = input ? input->GetMousePosition() : Vector2::Zero();
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    return 2;
}

// Input.GetKeysPressedThisFrame() -- returns an array table of every
// keycode that went down this frame. Mainly for figuring out what a key's
// raw code is on your machine: hold it and print the table's contents.
int Lua_InputGetKeysPressedThisFrame(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_newtable(L);
    if (!input) return 1;

    int i = 1;
    for (int keycode : input->GetKeysPressedThisFrame()) {
        lua_pushinteger(L, keycode);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

void RegisterInput(lua_State* L, UserInputService* input) {
    LuaBinding::Table(L)
        .Function<&UserInputService::IsKeyDown>("IsKeyDown", input)
        .Function<&UserInputService::IsKeyPressed>("IsKeyPressed", input)
        .Function<&UserInputService::IsKeyReleased>("IsKeyReleased", input)
        .Function<&UserInputService::IsMouseButtonDown>("IsMouseButtonDown", input)
        .Function<&UserInputService::IsMouseButtonPressed>("IsMouseButtonPressed", input)
        .Function<&UserInputService::IsMouseButtonReleased>("IsMouseButtonReleased", input)
        .Function<&UserInputService::GetScrollDelta>("GetScrollDelta", input)
        .RawWithContext("GetMousePosition", input, &Lua_InputGetMousePosition)
        .RawWithContext("GetKeysPressedThisFrame", input, &Lua_InputGetKeysPressedThisFrame)
        // Named constants so scripts can write Input.IsMouseButtonDown(Input.MouseLeft)
        // instead of a magic number.
        .Constant("MouseLeft", static_cast<int>(MouseButton::Left))
        .Constant("MouseRight", static_cast<int>(MouseButton::Right))
        .Constant("MouseMiddle", static_cast<int>(MouseButton::Middle))
        .Finish("Input");
}

// =====================================================================
// Physics -- table of static RigidBody2D gravity accessors. Both cross
// the Lua boundary as two raw floats rather than a Vector2 (matches
// every other hot-path Vector2-shaped getter/setter in this file), and
// RigidBody2D::SetGravity/GetGravity are static (no context to capture),
// so both stay hand-written plain functions -- same as the original.
// =====================================================================

// Physics.SetGravity(x, y) -- sets the engine-wide gravity acceleration
// (px/s^2), applied every RigidBody2D::Integrate() call to any body with
// mass > 0.
int Lua_PhysicsSetGravity(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    RigidBody2D::SetGravity(Vector2(x, y));
    return 0;
}

int Lua_PhysicsGetGravity(lua_State* L) {
    Vector2 gravity = RigidBody2D::GetGravity();
    lua_pushnumber(L, gravity.x);
    lua_pushnumber(L, gravity.y);
    return 2;
}

void RegisterPhysics(lua_State* L) {
    LuaBinding::Table(L)
        .Raw("SetGravity", &Lua_PhysicsSetGravity)
        .Raw("GetGravity", &Lua_PhysicsGetGravity)
        .Finish("Physics");
}

} // namespace

void ScriptBindings::RegisterAll(lua_State* L, EngineContext& context) {
    RegisterGraphics(L, context.graphics);
    RegisterWindow(L, context.window);
    RegisterVector2(L);
    RegisterRigidBody2D(L, context.actors);
    RegisterShader(L, context.actors);
    RegisterPixelSprite(L, context.actors);
    RegisterRenderer(L, context.renderer, context.actors);
    RegisterCollisionShape2D(L, context.actors);
    RegisterPlayerActorConfig(L, context.actors);
    RegisterActorRegistry(L, context.actors);
    RegisterInput(L, context.input);
    RegisterPhysics(L);
}