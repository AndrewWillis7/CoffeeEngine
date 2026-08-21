#include "ScriptBindings.h"
#include "LuaBinding.h"
#include "Core/EngineContext.h"

#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Physics/CollisionShape2D.h"
#include "Core/Gameplay/PlayerActorConfig.h"
#include "Core/Gameplay/Camera2D.h"
#include "Core/Gameplay/LightEmitterConfig.h"
#include "Core/Gameplay/LightingSystem.h"
#include "Core/Gameplay/Terrain/TerrainChunk.h"
#include "Core/Gameplay/Terrain/TerrainSystem.h"
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
    template <> struct MetatableOf<Camera2D>            { static constexpr const char* name = "Coffee.Camera2D"; };
    template <> struct MetatableOf<IWindow>             { static constexpr const char* name = "Coffee.IWindow"; };
    template <> struct MetatableOf<PixelSprite>         { static constexpr const char* name = "Coffee.PixelSprite"; };
    template <> struct MetatableOf<LightEmitterConfig>  { static constexpr const char* name = "Coffee.LightEmitterConfig"; };
    template <> struct MetatableOf<TerrainChunk>        { static constexpr const char* name = "Coffee.TerrainChunk"; };

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
// a plain method call; only position/rotation/scale (nested inside
// `transform`, rotation also unit-converted) and the optional-size
// constructor need a hand-written trampoline.
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

// Scale is a per-object visual multiplier on top of GetSize()/SetSize()'s
// logical/collision size -- deliberately decoupled (see Renderer2D's
// drawSize computation) so scaling a sprite up/down for a visual effect
// never silently resizes its CollisionShape2D underneath it. sy defaults
// to sx when omitted, so body:SetScale(2) means uniform 2x rather than
// forcing every non-uniform-scale caller to repeat the same number twice.
int Lua_RigidBody2DGetScale(lua_State* L) {
    RigidBody2D* self = LuaBinding::GetSelf<RigidBody2D>(L, 1);
    lua_pushnumber(L, self->transform.scale.x);
    lua_pushnumber(L, self->transform.scale.y);
    return 2;
}

int Lua_RigidBody2DSetScale(lua_State* L) {
    RigidBody2D* self = LuaBinding::GetSelf<RigidBody2D>(L, 1);
    float sx = static_cast<float>(luaL_checknumber(L, 2));
    float sy = static_cast<float>(luaL_optnumber(L, 3, sx));
    self->transform.scale.x = sx;
    self->transform.scale.y = sy;
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
        .Raw("GetScale", &Lua_RigidBody2DGetScale)
        .Raw("SetScale", &Lua_RigidBody2DSetScale)
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
        .PtrProperty<&RigidBody2D::camera>("GetCamera", "SetCamera")
        .PtrProperty<&RigidBody2D::lightEmitter>("GetLightEmitter", "SetLightEmitter")
        .PtrProperty<&RigidBody2D::terrain>("GetTerrain", "SetTerrain")
        .Property<&RigidBody2D::lightBlocking>("IsLightBlocking", "SetLightBlocking")
        .Method<&RigidBody2D::AddForce>("AddForce")
        .Method<&RigidBody2D::Integrate>("Integrate")
        .Method<&RigidBody2D::IsGrounded>("IsGrounded")
        .Method<&RigidBody2D::CollidesWith>("CollidesWith")
        .Method<&RigidBody2D::ResolveCollisionWith>("ResolveCollisionWith")
        .Method<&RigidBody2D::ResolveWindowBounds>("ResolveWindowBounds")
        .Method<&RigidBody2D::UpdateCamera>("UpdateCamera")
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

// Sprite.NewSolid(w, h, r, g, b, a) -- builds a blank, in-memory sprite
// filled solid with the given color, no PNG involved (see PixelSprite's
// (int,int,Color) constructor and ActorRegistry::CreateSolidSprite).
// This is the "split a basic flat-color square into individual pixels"
// primitive -- StaticBody/Prop/Player/ArtObject all call this instead of
// leaving a plain flat-color quad, so every basic body is pixel-
// addressable (and therefore lightable/eventually destructible) by
// default. a defaults to fully opaque, same convention as
// Lua_RigidBody2DSetColor.
int Lua_PixelSpriteNewSolid(lua_State* L) {
    int w = static_cast<int>(luaL_checkinteger(L, 1));
    int h = static_cast<int>(luaL_checkinteger(L, 2));
    float r = static_cast<float>(luaL_checknumber(L, 3));
    float g = static_cast<float>(luaL_checknumber(L, 4));
    float b = static_cast<float>(luaL_checknumber(L, 5));
    float a = static_cast<float>(luaL_optnumber(L, 6, 1.0));

    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    LuaBinding::Value<PixelSprite*>::Push(L, actors->CreateSolidSprite(w, h, r, g, b, a));
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

// sprite:Clear() -- wipe back to transparent. See PixelSprite::Clear.
int Lua_PixelSpriteClear(lua_State* L) {
    LuaBinding::GetSelf<PixelSprite>(L, 1)->Clear();
    return 0;
}

// sprite:FillRect(x, y, w, h, r, g, b, a) -- (x, y) is the TOP-LEFT, not
// a center, matching PixelSprite's own (0,0)-is-top-left convention.
int Lua_PixelSpriteFillRect(lua_State* L) {
    PixelSprite* self = LuaBinding::GetSelf<PixelSprite>(L, 1);
    int x = static_cast<int>(luaL_checknumber(L, 2));
    int y = static_cast<int>(luaL_checknumber(L, 3));
    int w = static_cast<int>(luaL_checknumber(L, 4));
    int h = static_cast<int>(luaL_checknumber(L, 5));
    float r = static_cast<float>(luaL_checknumber(L, 6));
    float g = static_cast<float>(luaL_checknumber(L, 7));
    float b = static_cast<float>(luaL_checknumber(L, 8));
    float a = static_cast<float>(luaL_optnumber(L, 9, 1.0));
    self->FillRect(x, y, w, h, Color(r, g, b, a));
    return 0;
}

// sprite:DrawLimb(x0, y0, x1, y1, thickness, r, g, b, a) -- the
// on-the-grid replacement for rotating a quad. See PixelSprite::DrawLimb.
int Lua_PixelSpriteDrawLimb(lua_State* L) {
    PixelSprite* self = LuaBinding::GetSelf<PixelSprite>(L, 1);
    int x0 = static_cast<int>(luaL_checknumber(L, 2));
    int y0 = static_cast<int>(luaL_checknumber(L, 3));
    int x1 = static_cast<int>(luaL_checknumber(L, 4));
    int y1 = static_cast<int>(luaL_checknumber(L, 5));
    int thickness = static_cast<int>(luaL_checknumber(L, 6));
    float r = static_cast<float>(luaL_checknumber(L, 7));
    float g = static_cast<float>(luaL_checknumber(L, 8));
    float b = static_cast<float>(luaL_checknumber(L, 9));
    float a = static_cast<float>(luaL_optnumber(L, 10, 1.0));
    self->DrawLimb(x0, y0, x1, y1, thickness, Color(r, g, b, a));
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
        .Raw("Clear", &Lua_PixelSpriteClear)
        .Raw("FillRect", &Lua_PixelSpriteFillRect)
        .Raw("DrawLimb", &Lua_PixelSpriteDrawLimb)
        .Finish();

    LuaBinding::Table(L)
        .RawWithContext("Load", actors, &Lua_PixelSpriteLoad)
        .RawWithContext("NewSolid", actors, &Lua_PixelSpriteNewSolid)
        .Finish("Sprite");
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
// Camera2D -- pointer type, owned by ActorRegistry. viewportSize/
// targetAspect/focusOffset are direct Vector2 fields (Vec2Property, same
// hot-path "two raw numbers" convention as RigidBody2D::velocity/size);
// followTarget is a direct RigidBody2D* field (PtrProperty, nil-clears-it
// setter, same convention as RigidBody2D::shader/collisionShape/
// playerConfig); followSmoothing/active are plain scalar fields. zoomOut
// is bound as Method<>, not Property<>, because it's a private field
// behind a clamping setter (SetZoomOut rejects <= 0) -- see Camera2D.h.
// =====================================================================

void RegisterCamera2D(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<Camera2D>(L, LuaBinding::MetatableOf<Camera2D>::name)
        .Vec2Property<&Camera2D::viewportSize>("GetViewportSize", "SetViewportSize")
        .Vec2Property<&Camera2D::targetAspect>("GetTargetAspect", "SetTargetAspect")
        .Vec2Property<&Camera2D::focusOffset>("GetFocusOffset", "SetFocusOffset")
        .PtrProperty<&Camera2D::followTarget>("GetFollowTarget", "SetFollowTarget")
        .Property<&Camera2D::followSmoothing>("GetFollowSmoothing", "SetFollowSmoothing")
        .Property<&Camera2D::active>("IsActive", "SetActive")
        .Method<&Camera2D::GetZoomOut>("GetZoomOut")
        .Method<&Camera2D::SetZoomOut>("SetZoomOut")
        .Finish();

    LuaBinding::Table(L).Function<&ActorRegistry::CreateCamera>("new", actors).Finish("Camera2D");
}

// =====================================================================
// LightEmitterConfig -- pointer type, owned by ActorRegistry. Same
// pattern as PlayerActorConfig: attach via RigidBody2D::lightEmitter
// (LuaBinding::PtrProperty, see RegisterRigidBody2D above), consumed
// every frame by LightingSystem, never touched by DrawBody itself.
//
// type/color/flickerColorShift are hand-written (an enum<->string
// mapping and 4-scalar RGBA reads/writes, same shape as
// Lua_CollisionShape2DGetType and Lua_RigidBody2DSetColor respectively
// -- neither is a mechanical 1:1 Property<> mapping). coneAngleRad/
// coneDirectionRad cross the Lua boundary in DEGREES via ScaledProperty,
// same "radians internally, degrees at the boundary" convention
// RigidBody2D::SetRotation/angularVelocity already use. Everything else
// is a plain public float/bool, so those stay Property<>.
// =====================================================================

int Lua_LightEmitterGetType(lua_State* L) {
    LightEmitterConfig* self = LuaBinding::GetSelf<LightEmitterConfig>(L, 1);
    lua_pushstring(L, self->type == LightEmitterConfig::Type::Cone ? "Cone" : "Point");
    return 1;
}

int Lua_LightEmitterSetType(lua_State* L) {
    LightEmitterConfig* self = LuaBinding::GetSelf<LightEmitterConfig>(L, 1);
    std::string typeStr = luaL_checkstring(L, 2);
    self->type = (typeStr == "Cone") ? LightEmitterConfig::Type::Cone : LightEmitterConfig::Type::Point;
    return 0;
}

int Lua_LightEmitterGetColor(lua_State* L) {
    LightEmitterConfig* self = LuaBinding::GetSelf<LightEmitterConfig>(L, 1);
    lua_pushnumber(L, self->color.r);
    lua_pushnumber(L, self->color.g);
    lua_pushnumber(L, self->color.b);
    lua_pushnumber(L, self->color.a);
    return 4;
}

int Lua_LightEmitterSetColor(lua_State* L) {
    LightEmitterConfig* self = LuaBinding::GetSelf<LightEmitterConfig>(L, 1);
    self->color.r = static_cast<float>(luaL_checknumber(L, 2));
    self->color.g = static_cast<float>(luaL_checknumber(L, 3));
    self->color.b = static_cast<float>(luaL_checknumber(L, 4));
    self->color.a = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    return 0;
}

int Lua_LightEmitterGetFlickerColorShift(lua_State* L) {
    LightEmitterConfig* self = LuaBinding::GetSelf<LightEmitterConfig>(L, 1);
    lua_pushnumber(L, self->flickerColorShift.r);
    lua_pushnumber(L, self->flickerColorShift.g);
    lua_pushnumber(L, self->flickerColorShift.b);
    lua_pushnumber(L, self->flickerColorShift.a);
    return 4;
}

int Lua_LightEmitterSetFlickerColorShift(lua_State* L) {
    LightEmitterConfig* self = LuaBinding::GetSelf<LightEmitterConfig>(L, 1);
    self->flickerColorShift.r = static_cast<float>(luaL_checknumber(L, 2));
    self->flickerColorShift.g = static_cast<float>(luaL_checknumber(L, 3));
    self->flickerColorShift.b = static_cast<float>(luaL_checknumber(L, 4));
    self->flickerColorShift.a = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    return 0;
}

void RegisterLightEmitterConfig(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<LightEmitterConfig>(L, LuaBinding::MetatableOf<LightEmitterConfig>::name)
        .Raw("GetType", &Lua_LightEmitterGetType)
        .Raw("SetType", &Lua_LightEmitterSetType)
        .Raw("GetColor", &Lua_LightEmitterGetColor)
        .Raw("SetColor", &Lua_LightEmitterSetColor)
        .Raw("GetFlickerColorShift", &Lua_LightEmitterGetFlickerColorShift)
        .Raw("SetFlickerColorShift", &Lua_LightEmitterSetFlickerColorShift)
        .Property<&LightEmitterConfig::radius>("GetRadius", "SetRadius")
        .Property<&LightEmitterConfig::brightness>("GetBrightness", "SetBrightness")
        .Property<&LightEmitterConfig::falloffExponent>("GetFalloffExponent", "SetFalloffExponent")
        .ScaledProperty<&LightEmitterConfig::coneAngleRad, kRadToDeg, kDegToRad>("GetConeAngle", "SetConeAngle")
        .ScaledProperty<&LightEmitterConfig::coneDirectionRad, kRadToDeg, kDegToRad>("GetConeDirection", "SetConeDirection")
        .Property<&LightEmitterConfig::useOwnerRotation>("GetUseOwnerRotation", "SetUseOwnerRotation")
        .Property<&LightEmitterConfig::flicker>("IsFlickering", "SetFlicker")
        .Property<&LightEmitterConfig::flickerSpeed>("GetFlickerSpeed", "SetFlickerSpeed")
        .Property<&LightEmitterConfig::flickerIntensityAmount>("GetFlickerIntensityAmount", "SetFlickerIntensityAmount")
        .Property<&LightEmitterConfig::toneSteps>("GetToneSteps", "SetToneSteps")
        .Finish();

    LuaBinding::Table(L).Function<&ActorRegistry::CreateLightEmitter>("new", actors).Finish("LightEmitterConfig");
}

// =====================================================================
// Lighting -- bare global UpdateLighting(deltaTime), not a table
// function -- same "called once a frame, needs more than one captured
// context pointer" shape as SyncCamera() above, and the same reasoning:
// re-resolving anything per-object here instead of once a frame would be
// wasteful. Needs both a LightingSystem* (to run the pass) and an
// ActorRegistry* (for LightingSystem::Update to scan), so this bypasses
// BindFunction/Table::Function the same way Lua_DrawBody/Lua_SyncCamera
// already do, and pushes both closures by hand.
// =====================================================================

int Lua_UpdateLighting(lua_State* L) {
    float dt = static_cast<float>(luaL_checknumber(L, 1));
    auto* lighting = static_cast<LightingSystem*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(2)));
    if (lighting && actors) lighting->Update(*actors, dt);
    return 0;
}

void RegisterLighting(lua_State* L, LightingSystem* lighting, ActorRegistry* actors) {
    lua_pushlightuserdata(L, lighting);
    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, &Lua_UpdateLighting, 2);
    lua_setglobal(L, "UpdateLighting");
}

// =====================================================================
// TerrainChunk -- pointer type, owned by ActorRegistry. Same shape as
// LightEmitterConfig: attach via RigidBody2D::terrain (see
// RegisterRigidBody2D above), consumed every frame by TerrainSystem.
//
// Almost the entire surface is mechanical Property<> mapping onto plain
// public float/int fields, because that's what the config IS. The two
// exceptions are the same two this file already has elsewhere:
//   - Colors cross as 3-4 raw scalars, not a userdata (same convention
//     as Lua_RigidBody2DSetColor and LightEmitterConfig's color), so each
//     one is a thin wrapper over a shared helper rather than a
//     Property<>.
//   - Generate/Update/ResolveBody/SurfaceWorldY all take references to
//     OTHER bound types (PixelSprite&, RigidBody2D&), which LuaBinding's
//     Extract<> already handles natively -- so those ARE plain Method<>
//     bindings despite looking like they'd need trampolines.
//
// Note what ISN'T bound: nothing writes m_SurfaceY or the blade list from
// Lua. Those are generated data derived from `seed` + the sprite's size,
// and letting a script poke them would let the collision heightmap and
// the pixels you can see disagree.
// =====================================================================

int SetTerrainColorField(lua_State* L, Color TerrainChunk::* field) {
    TerrainChunk* self = LuaBinding::GetSelf<TerrainChunk>(L, 1);
    (self->*field) = Color(static_cast<float>(luaL_checknumber(L, 2)),
                           static_cast<float>(luaL_checknumber(L, 3)),
                           static_cast<float>(luaL_checknumber(L, 4)),
                           static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    return 0;
}

int Lua_TerrainSetDirtDark(lua_State* L)   { return SetTerrainColorField(L, &TerrainChunk::dirtDark); }
int Lua_TerrainSetDirtLight(lua_State* L)  { return SetTerrainColorField(L, &TerrainChunk::dirtLight); }
int Lua_TerrainSetRockColor(lua_State* L)  { return SetTerrainColorField(L, &TerrainChunk::rockColor); }
int Lua_TerrainSetTopsoilColor(lua_State* L) { return SetTerrainColorField(L, &TerrainChunk::topsoilColor); }
int Lua_TerrainSetGrassDark(lua_State* L)  { return SetTerrainColorField(L, &TerrainChunk::grassDark); }
int Lua_TerrainSetGrassLight(lua_State* L) { return SetTerrainColorField(L, &TerrainChunk::grassLight); }

void RegisterTerrainChunk(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<TerrainChunk>(L, LuaBinding::MetatableOf<TerrainChunk>::name)
        // --- surface shape ---
        .Property<&TerrainChunk::seed>("GetSeed", "SetSeed")
        .Property<&TerrainChunk::surfaceFrequency>("GetSurfaceFrequency", "SetSurfaceFrequency")
        .Property<&TerrainChunk::surfaceAmplitude>("GetSurfaceAmplitude", "SetSurfaceAmplitude")
        .Property<&TerrainChunk::surfaceOctaves>("GetSurfaceOctaves", "SetSurfaceOctaves")
        .Property<&TerrainChunk::surfaceLacunarity>("GetSurfaceLacunarity", "SetSurfaceLacunarity")
        .Property<&TerrainChunk::surfaceGain>("GetSurfaceGain", "SetSurfaceGain")
        .Property<&TerrainChunk::surfaceOffset>("GetSurfaceOffset", "SetSurfaceOffset")
        // --- dirt ---
        .Property<&TerrainChunk::dirtFrequency>("GetDirtFrequency", "SetDirtFrequency")
        .Property<&TerrainChunk::dirtOctaves>("GetDirtOctaves", "SetDirtOctaves")
        .Property<&TerrainChunk::dirtToneSteps>("GetDirtToneSteps", "SetDirtToneSteps")
        .Property<&TerrainChunk::rockChance>("GetRockChance", "SetRockChance")
        .Property<&TerrainChunk::depthDarkening>("GetDepthDarkening", "SetDepthDarkening")
        .Property<&TerrainChunk::topsoilDepth>("GetTopsoilDepth", "SetTopsoilDepth")
        .Raw("SetDirtDark", &Lua_TerrainSetDirtDark)
        .Raw("SetDirtLight", &Lua_TerrainSetDirtLight)
        .Raw("SetRockColor", &Lua_TerrainSetRockColor)
        .Raw("SetTopsoilColor", &Lua_TerrainSetTopsoilColor)
        // --- grass ---
        .Property<&TerrainChunk::grassMinHeight>("GetGrassMinHeight", "SetGrassMinHeight")
        .Property<&TerrainChunk::grassMaxHeight>("GetGrassMaxHeight", "SetGrassMaxHeight")
        .Property<&TerrainChunk::grassDensity>("GetGrassDensity", "SetGrassDensity")
        .Property<&TerrainChunk::swayAmplitude>("GetSwayAmplitude", "SetSwayAmplitude")
        .Property<&TerrainChunk::swaySpeed>("GetSwaySpeed", "SetSwaySpeed")
        .Property<&TerrainChunk::swayPhasePerTexel>("GetSwayPhasePerTexel", "SetSwayPhasePerTexel")
        .Property<&TerrainChunk::bendStiffness>("GetBendStiffness", "SetBendStiffness")
        .Property<&TerrainChunk::bendDamping>("GetBendDamping", "SetBendDamping")
        .Property<&TerrainChunk::maxBend>("GetMaxBend", "SetMaxBend")
        .Property<&TerrainChunk::disturbStrength>("GetDisturbStrength", "SetDisturbStrength")
        .Property<&TerrainChunk::disturbPadding>("GetDisturbPadding", "SetDisturbPadding")
        .Property<&TerrainChunk::disturbSpeedScale>("GetDisturbSpeedScale", "SetDisturbSpeedScale")
        .Raw("SetGrassDark", &Lua_TerrainSetGrassDark)
        .Raw("SetGrassLight", &Lua_TerrainSetGrassLight)
        // --- collision ---
        .Property<&TerrainChunk::maxStepHeight>("GetMaxStepHeight", "SetMaxStepHeight")
        // --- lifecycle / queries ---
        .Method<&TerrainChunk::Generate>("Generate")
        .Method<&TerrainChunk::ResolveBody>("ResolveBody")
        .Method<&TerrainChunk::SurfaceWorldY>("SurfaceWorldY")
        .Method<&TerrainChunk::IsGenerated>("IsGenerated")
        .Method<&TerrainChunk::GetWidth>("GetWidth")
        .Method<&TerrainChunk::GetHeight>("GetHeight")
        .Method<&TerrainChunk::GetBladeCount>("GetBladeCount")
        .Finish();

    LuaBinding::Table(L).Function<&ActorRegistry::CreateTerrainChunk>("new", actors).Finish("TerrainChunk");
}

// =====================================================================
// Terrain -- bare global UpdateTerrain(deltaTime), exactly the same
// two-upvalue hand-rolled closure UpdateLighting() above uses, for
// exactly the same reason (needs both the subsystem and the registry it
// scans, and BindFunction/Table::Function only capture one context
// pointer).
//
// Call this once a frame, AFTER gameplay has moved (so the grass reacts
// to where the player actually is this frame) and BEFORE
// UpdateLighting() (so the pixels it just wrote get lit this frame
// rather than next).
// =====================================================================

int Lua_UpdateTerrain(lua_State* L) {
    float dt = static_cast<float>(luaL_checknumber(L, 1));
    auto* terrain = static_cast<TerrainSystem*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(2)));
    if (terrain && actors) terrain->Update(*actors, dt);
    return 0;
}

void RegisterTerrainSystem(lua_State* L, TerrainSystem* terrain, ActorRegistry* actors) {
    lua_pushlightuserdata(L, terrain);
    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, &Lua_UpdateTerrain, 2);
    lua_setglobal(L, "UpdateTerrain");
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
// Window -- eWindow global. GetWidth/GetHeight/SetIcon/SetFullscreen/
// IsFullscreen map straight onto IWindow's (virtual) methods -- pointer-
// to-member-function dispatch is virtual automatically, no special-
// casing needed for that.
// =====================================================================

void RegisterWindow(lua_State* L, IWindow* window) {
    LuaBinding::Class<IWindow>(L, LuaBinding::MetatableOf<IWindow>::name)
        .Method<&IWindow::GetWidth>("GetWidth")
        .Method<&IWindow::GetHeight>("GetHeight")
        .Method<&IWindow::SetIcon>("SetIcon")
        .Method<&IWindow::SetFullscreen>("SetFullscreen")
        .Method<&IWindow::IsFullscreen>("IsFullscreen")
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
//
// SyncCamera() -- resolves ActorRegistry's currently-active camera (see
// GetActiveCamera()) and pushes it into the renderer for the rest of this
// frame's world-space draws (including its targetAspect, the "Border"
// named shader, and any attached border sprite -- see SetBorderSprite --
// so the letterbox/pillarbox margins get whatever border effect is
// currently loaded), or clears it if no camera is active. Call once per
// frame from Lua (after any camera-follow update, before your Draw()
// calls) -- deliberately NOT done automatically inside DrawBody() itself:
// that would re-resolve the active camera on every single object drawn
// (an O(n) ActorRegistry scan per DrawBody call, O(n^2) per frame) for a
// value that only actually needs recomputing once a frame. Same
// two-upvalue hand-rolled closure as Lua_DrawBody, for the same reason
// (needs both a Renderer2D* and an ActorRegistry*).
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

int Lua_SyncCamera(lua_State* L) {
    auto* renderer = static_cast<Renderer2D*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(2)));
    if (!renderer || !actors) return 0;

    RigidBody2D* camBody = actors->GetActiveCamera();
    if (camBody && camBody->camera) {
        Shader* border = actors->GetOrCreateNamedShader("Border");

        // Same "flush pending edits, then hand over the raw Texture*"
        // shape Lua_DrawBody already uses for sprite-backed bodies above.
        Texture* borderTexture = nullptr;
        if (PixelSprite* borderSprite = actors->GetBorderSprite()) {
            borderSprite->Flush();
            borderTexture = borderSprite->GetTexture();
        }

        renderer->SetActiveCamera(camBody->transform.position, camBody->camera->EffectiveViewportSize(),
                                   camBody->camera->targetAspect, border, borderTexture);
    } else {
        renderer->ClearActiveCamera();
    }
    return 0;
}

void RegisterRenderer(lua_State* L, Renderer2D* renderer, ActorRegistry* actors) {
    lua_pushlightuserdata(L, renderer);
    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, &Lua_DrawBody, 2);
    lua_setglobal(L, "DrawBody");

    lua_pushlightuserdata(L, renderer);
    lua_pushlightuserdata(L, actors);
    lua_pushcclosure(L, &Lua_SyncCamera, 2);
    lua_setglobal(L, "SyncCamera");

    LuaBinding::BindFunction<&Renderer2D::SetPixelScale>(L, "SetPixelScale", renderer);
    LuaBinding::BindFunction<&Renderer2D::GetPixelScale>(L, "GetPixelScale", renderer);
}

// =====================================================================
// Actors -- table of ActorRegistry-wide queries and utilities.
// GetNamedShader/LoadShaderFromFile/SetBorderSprite/GetBorderSprite are
// all mechanical 1:1 method forwards; DumpTree too.
// =====================================================================

void RegisterActorRegistry(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Table(L)
        .Function<&ActorRegistry::GetPlayerActor>("GetPlayer", actors)
        .Function<&ActorRegistry::GetActiveCamera>("GetActiveCamera", actors)
        .Function<&ActorRegistry::GetOrCreateNamedShader>("GetNamedShader", actors)
        .Function<&ActorRegistry::LoadNamedShaderFromFile>("LoadShaderFromFile", actors)
        .Function<&ActorRegistry::SetBorderSprite>("SetBorderSprite", actors)
        .Function<&ActorRegistry::GetBorderSprite>("GetBorderSprite", actors)
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
    RegisterCamera2D(L, context.actors);
    RegisterLightEmitterConfig(L, context.actors);
    RegisterLighting(L, context.lighting, context.actors);
    RegisterTerrainChunk(L, context.actors);
    RegisterTerrainSystem(L, context.terrain, context.actors);
    RegisterActorRegistry(L, context.actors);
    RegisterInput(L, context.input);
    RegisterPhysics(L);
}