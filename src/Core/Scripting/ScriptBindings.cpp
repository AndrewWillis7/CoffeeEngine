#include "ScriptBindings.h"
#include "LuaBinding.h"
#include "Core/EngineContext.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Physics/CollisionShape2D.h"
#include "Core/Gameplay/PlayerActorConfig.h"
#include "Core/Gameplay/Camera2D.h"
#include "Core/Gameplay/LightEmitterConfig.h"
#include "Core/Gameplay/LightingSystem.h"
#include "Core/Gameplay/Terrain/TerrainChunk.h"
#include "Core/Gameplay/Terrain/TerrainSystem.h"
#include "Core/Physics/Raycast.h"
#include "Core/Gameplay/LegIK.h"
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

// --- Type registry: every bound type's metatable name, in one place. ------
// One line per type, written once, with the compiler enforcing the match that
// used to be a copy-pasted constant and a "must match" comment per file.
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

    // MouseButton is an engine enum the generic header shouldn't know about, but
    // the trait system is extensible from any translation unit.
    template <> struct Value<MouseButton> {
        static MouseButton Get(lua_State* L, int idx) { return static_cast<MouseButton>(luaL_checkinteger(L, idx)); }
        static void Push(lua_State* L, MouseButton v) { lua_pushinteger(L, static_cast<int>(v)); }
    };
}

namespace {

constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

// --- Vector2: value type. Operators map straight onto Method<>; only __mul's
// operand order and __tostring's format need hand-written trampolines. -----

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

// Both `vec * number` and `number * vec`: only one side is our userdata.
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

// --- RigidBody2D: pointer type, owned by ActorRegistry. Mostly direct field
// access; only transform-nested position/rotation/scale and the optional-size
// constructor need trampolines. -------------------------------------------

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

// Degrees at the boundary; Transform2D stores radians.
int Lua_RigidBody2DGetRotation(lua_State* L) {
    lua_pushnumber(L, LuaBinding::GetSelf<RigidBody2D>(L, 1)->transform.rotation * kRadToDeg);
    return 1;
}

int Lua_RigidBody2DSetRotation(lua_State* L) {
    LuaBinding::GetSelf<RigidBody2D>(L, 1)->transform.rotation = static_cast<float>(luaL_checknumber(L, 2)) * kDegToRad;
    return 0;
}

// A visual multiplier on top of SetSize()'s logical/collision size, decoupled
// so a visual scale never silently resizes the collider. sy defaults to sx.
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

// A label only -- the engine never reads it back. The debug explorer lists
// bodies by it, so naming an actor is what turns a flat pointer dump into a
// readable scene tree.
int Lua_RigidBody2DSetName(lua_State* L) {
    LuaBinding::GetSelf<RigidBody2D>(L, 1)->name = luaL_checkstring(L, 2);
    return 0;
}

int Lua_RigidBody2DGetName(lua_State* L) {
    lua_pushstring(L, LuaBinding::GetSelf<RigidBody2D>(L, 1)->name.c_str());
    return 1;
}

int Lua_RigidBody2DIsPlayer(lua_State* L) {
    lua_pushboolean(L, LuaBinding::GetSelf<RigidBody2D>(L, 1)->playerConfig != nullptr);
    return 1;
}

int Lua_RigidBody2DGetSprite(lua_State* L) {
    LuaBinding::Value<PixelSprite*>::Push(L, LuaBinding::GetSelf<RigidBody2D>(L, 1)->sprite);
    return 1;
}

// Not a plain PtrProperty: attaching a sprite also defaults the body's draw
// size to the sprite's native size, which a bare field assignment can't do.
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
        .Raw("SetName", &Lua_RigidBody2DSetName)
        .Raw("GetName", &Lua_RigidBody2DGetName)
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
        .Property<&RigidBody2D::raycastTarget>("IsRaycastTarget", "SetRaycastTarget")
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

// Sprite.NewSolid(w, h, r, g, b, a) -- a blank in-memory sprite, no PNG. Every
// basic body uses this instead of a flat quad so it is pixel-addressable, and
// therefore lightable and eventually destructible, by default.
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

// sprite:DrawTaperedLimb(x0, y0, x1, y1, t0, t1, bulge, bulgeAt, r, g, b, a)
int Lua_PixelSpriteDrawTaperedLimb(lua_State* L) {
    PixelSprite* self = LuaBinding::GetSelf<PixelSprite>(L, 1);
    self->DrawTaperedLimb(
        static_cast<int>(luaL_checknumber(L, 2)),
        static_cast<int>(luaL_checknumber(L, 3)),
        static_cast<int>(luaL_checknumber(L, 4)),
        static_cast<int>(luaL_checknumber(L, 5)),
        static_cast<int>(luaL_checknumber(L, 6)),
        static_cast<int>(luaL_checknumber(L, 7)),
        static_cast<float>(luaL_optnumber(L, 8, 0.0)),
        static_cast<float>(luaL_optnumber(L, 9, 0.35)),
        Color(static_cast<float>(luaL_checknumber(L, 10)),
              static_cast<float>(luaL_checknumber(L, 11)),
              static_cast<float>(luaL_checknumber(L, 12)),
              static_cast<float>(luaL_optnumber(L, 13, 1.0))));
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
        .Raw("DrawTaperedLimb", &Lua_PixelSpriteDrawTaperedLimb)
        .Finish();

    LuaBinding::Table(L)
        .RawWithContext("Load", actors, &Lua_PixelSpriteLoad)
        .RawWithContext("NewSolid", actors, &Lua_PixelSpriteNewSolid)
        .Finish("Sprite");
}



// --- PlayerActorConfig: all direct fields, so Property<> plus a factory. ---

void RegisterPlayerActorConfig(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Class<PlayerActorConfig>(L, LuaBinding::MetatableOf<PlayerActorConfig>::name)
        .Property<&PlayerActorConfig::moveSpeed>("GetMoveSpeed", "SetMoveSpeed")
        .Property<&PlayerActorConfig::jumpForce>("GetJumpForce", "SetJumpForce")
        .Property<&PlayerActorConfig::inputEnabled>("IsInputEnabled", "SetInputEnabled")
        .Finish();

    LuaBinding::Table(L).Function<&ActorRegistry::CreatePlayerConfig>("new", actors).Finish("PlayerActorConfig");
}

// --- Camera2D: mechanical field bindings, except zoomOut, which is Method<>
// rather than Property<> because it sits behind a clamping setter. ---------

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

// --- LightEmitterConfig: attached via RigidBody2D::lightEmitter, consumed by
// LightingSystem each frame. type/color/flickerColorShift are hand-written (an
// enum<->string map and 4-scalar RGBA); the cone angles cross in degrees via
// ScaledProperty; everything else is a plain Property<>. -------------------

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

// --- Lighting: a bare global, because it needs two captured context pointers
// and Table::Function only carries one. Hand-pushed closure. ---------------

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

// --- TerrainChunk: attached via RigidBody2D::terrain, ticked by TerrainSystem.
// Almost all mechanical Property<>, with two exceptions this file has
// elsewhere too: colors cross as raw scalars rather than userdata, and the
// methods taking PixelSprite&/RigidBody2D& are plain Method<> because
// Extract<> already handles bound-type references.
//
// Note what is NOT bound: nothing writes m_SurfaceY or the blade list. Those
// are generated from `seed` and the sprite size, and letting a script poke
// them would let the collision heightmap and the visible pixels disagree. ---

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

// --- Terrain: same two-upvalue closure as UpdateLighting, for the same reason.
// Call once a frame, AFTER gameplay has moved and BEFORE UpdateLighting(), so
// the grass reacts to this frame's positions and the pixels it writes get lit
// this frame rather than next. -------------------------------------------

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

// --- Graphics: bare globals over a captured IGraphicsContext*. DrawDebugQuad
// assembles a transform out of 9 scalars, so it stays hand-written. --------

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

// --- Window: the eWindow global. Pointer-to-member dispatch is already
// virtual, so IWindow's methods map straight across. -----------------------

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

// --- Renderer: DrawBody(body) forwards to DrawQuad, or to DrawTexturedQuad
// when a sprite is attached. Needs both the renderer and the registry (to
// resolve the "Textured" shader), so it is a hand-pushed two-upvalue closure.
//
// SyncCamera() resolves the active camera and pushes it, its targetAspect, the
// "Border" shader and any border sprite into the renderer for the rest of the
// frame's world draws. Call it once a frame, after camera-follow and before
// drawing. Deliberately NOT folded into DrawBody(): that would re-run an O(n)
// registry scan per object drawn, O(n^2) a frame, for a once-a-frame value. --

int Lua_DrawBody(lua_State* L) {
    auto* body = LuaBinding::Value<RigidBody2D*>::Get(L, 1);
    auto* renderer = static_cast<Renderer2D*>(lua_touserdata(L, lua_upvalueindex(1)));
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(2)));
    if (!renderer) return 0;

    if (body->sprite) {
        // So a punch and its DrawBody() in the same Update() land in the same
        // frame rather than one late.
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

        // Same flush-then-hand-over-the-Texture* shape as Lua_DrawBody.
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

// --- Actors: registry-wide queries, all mechanical 1:1 method forwards. ---

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

// --- Input: over a captured UserInputService*. The Is* queries map 1:1;
// GetMousePosition and GetKeysPressedThisFrame return shapes a single push
// can't express, so they stay hand-written. --------------------------------

int Lua_InputGetMousePosition(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    Vector2 pos = input ? input->GetMousePosition() : Vector2::Zero();
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    return 2;
}

// An array of every keycode that went down this frame -- mainly for finding a
// key's raw code on your machine: hold it and print the table.
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
        // So scripts write Input.MouseLeft rather than a magic number.
        .Constant("MouseLeft", static_cast<int>(MouseButton::Left))
        .Constant("MouseRight", static_cast<int>(MouseButton::Right))
        .Constant("MouseMiddle", static_cast<int>(MouseButton::Middle))
        .Finish("Input");
}

// --- Physics: gravity accessors plus the ground raycast. Gravity crosses as
// two raw floats and its accessors are static, so both stay plain functions;
// the raycast takes the registry as an upvalue. ----------------------------

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

// Physics.RaycastDown(x, fromY, maxY [, ignoreBody]) -> surfaceY, body
// Returns nil on a miss. See Core/Physics/Raycast.h.
int Lua_PhysicsRaycastDown(lua_State* L) {
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    float x     = static_cast<float>(luaL_checknumber(L, 1));
    float fromY = static_cast<float>(luaL_checknumber(L, 2));
    float maxY  = static_cast<float>(luaL_checknumber(L, 3));

    RigidBody2D* ignore = lua_isnoneornil(L, 4)
        ? nullptr
        : LuaBinding::Value<RigidBody2D*>::Get(L, 4);

    Physics::GroundHit hit = Physics::RaycastDown(*actors, x, fromY, maxY, ignore);
    if (!hit.hit) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, hit.y);
    LuaBinding::Value<RigidBody2D*>::Push(L, const_cast<RigidBody2D*>(hit.body));
    return 2;
}

void RegisterPhysics(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Table(L)
        .Raw("SetGravity", &Lua_PhysicsSetGravity)
        .Raw("GetGravity", &Lua_PhysicsGetGravity)
        .RawWithContext("RaycastDown", actors, &Lua_PhysicsRaycastDown)
        .Finish("Physics");
}

// --- IK: stateless numeric kernels for objects/leg_rig.lua. No metatable, no
// ownership -- every one is a pure function over floats. -------------------

// IK.SolveTwoBone(hipX, hipY, ankleX, ankleY, L1, L2, side)
//   -> kneeX, kneeY, ankleX, ankleY   (all whole texels)
int Lua_IKSolveTwoBone(lua_State* L) {
    LegIK::Joints j = LegIK::SolveTwoBone(
        static_cast<float>(luaL_checknumber(L, 1)),
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_checknumber(L, 5)),
        static_cast<float>(luaL_checknumber(L, 6)),
        static_cast<float>(luaL_optnumber(L, 7, 1.0)));
    lua_pushnumber(L, j.kneeX);
    lua_pushnumber(L, j.kneeY);
    lua_pushnumber(L, j.ankleX);
    lua_pushnumber(L, j.ankleY);
    return 4;
}

// IK.GaitPose(phase, stanceRatio, swingFrames) -> sweep, lift, load, push, roll
int Lua_IKGaitPose(lua_State* L) {
    LegIK::GaitSample s = LegIK::SampleGait(
        static_cast<float>(luaL_checknumber(L, 1)),
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<int>(luaL_optinteger(L, 3, 0)));
    lua_pushnumber(L, s.sweep);
    lua_pushnumber(L, s.lift);
    lua_pushnumber(L, s.load);
    lua_pushnumber(L, s.push);
    lua_pushnumber(L, s.roll);
    return 5;
}

// IK.KneeBulge(L1, L2, dMin) -> texels
int Lua_IKKneeBulge(lua_State* L) {
    lua_pushnumber(L, LegIK::KneeBulge(
        static_cast<float>(luaL_checknumber(L, 1)),
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_optnumber(L, 3, 0.0))));
    return 1;
}

// IK.Approach(current, target, rate, dt) -> value
int Lua_IKApproach(lua_State* L) {
    lua_pushnumber(L, LegIK::Approach(
        static_cast<float>(luaL_checknumber(L, 1)),
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4))));
    return 1;
}

// IK.SolveLegFrame fuses the whole per-leg world-space solve -- GaitPose, the
// ground raycast, two Approach calls and the arithmetic around them -- into one
// crossing. Stage 1 of the two-stage solve at the top of leg_rig.lua:
// continuous world space, no texel rounding yet (stage 2 is SolveTwoBone,
// already a single call, run once weight distribution has settled the hips).
//
// Every persistent per-leg value is threaded through as an explicit in/out
// pair rather than held here -- Lua still owns the leg table.
//
// Mirrors the old Lua loop body operation for operation, evaluation order
// included, so results are bit-identical. See scripts/api/IK.txt section 7.
//
// Args (31): phase, stanceRatio, swingFrames,
//            hipX, hipY, facing, amp,
//            legLength, footBaseWidth,
//            grounded, snapDistance, ownerBody,
//            blend, stepHeight, airFactor, airReach,
//            pushHeight, pushToe, footLean, heelLean,
//            smoothing, dt,
//            hasFoot, prevFootX, prevFootY, prevOffX, prevOffY,
//            hasGroundY, prevGroundY, lastFacing, lastMode
// Returns (14): load, lift, ankleLift, footW, footOff,
//               footX, footY, groundY (nil if airborne this frame),
//               offX, offY, soleDX, soleDY, lastMode, slack
//               (slack is +inf when not grounded this frame, so Lua's
//               existing `if slack < minSlack then` needs no mode check)
namespace {
constexpr int kLegModeAir = 0;
constexpr int kLegModeGround = 1;

inline float RoundHalfUp(float v) { return std::floor(v + 0.5f); }
inline float ClampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
} // namespace

int Lua_IKSolveLegFrame(lua_State* L) {
    auto* actors = static_cast<ActorRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
    luaL_argcheck(L, actors != nullptr, 1, "engine has no ActorRegistry bound");

    int i = 1;
    const float phase        = static_cast<float>(luaL_checknumber(L, i++));
    const float stanceRatio  = static_cast<float>(luaL_checknumber(L, i++));
    const int   swingFrames  = static_cast<int>(luaL_checkinteger(L, i++));
    const float hipX         = static_cast<float>(luaL_checknumber(L, i++));
    const float hipY         = static_cast<float>(luaL_checknumber(L, i++));
    const float facing       = static_cast<float>(luaL_checknumber(L, i++));
    const float amp          = static_cast<float>(luaL_checknumber(L, i++));
    const float legLength    = static_cast<float>(luaL_checknumber(L, i++));
    const float footBaseW    = static_cast<float>(luaL_checknumber(L, i++));
    const bool  grounded     = lua_toboolean(L, i++) != 0;
    const float snapDistance = static_cast<float>(luaL_checknumber(L, i++));
    RigidBody2D* ignoreBody  = LuaBinding::Value<RigidBody2D*>::Get(L, i++);
    const float blend        = static_cast<float>(luaL_checknumber(L, i++));
    const float stepHeight   = static_cast<float>(luaL_checknumber(L, i++));
    const float airFactor    = static_cast<float>(luaL_checknumber(L, i++));
    const float airReach     = static_cast<float>(luaL_checknumber(L, i++));
    const float pushHeight   = static_cast<float>(luaL_checknumber(L, i++));
    const float pushToe      = static_cast<float>(luaL_checknumber(L, i++));
    const float footLean     = static_cast<float>(luaL_checknumber(L, i++));
    const float heelLean     = static_cast<float>(luaL_checknumber(L, i++));
    const float smoothing    = static_cast<float>(luaL_checknumber(L, i++));
    const float dt           = static_cast<float>(luaL_checknumber(L, i++));
    const bool  hasFoot      = lua_toboolean(L, i++) != 0;
    const float prevFootX    = static_cast<float>(luaL_optnumber(L, i++, 0.0));
    const float prevFootY    = static_cast<float>(luaL_optnumber(L, i++, 0.0));
    const float prevOffX     = static_cast<float>(luaL_optnumber(L, i++, 0.0));
    const float prevOffY     = static_cast<float>(luaL_optnumber(L, i++, 0.0));
    const bool  hasGroundY   = lua_toboolean(L, i++) != 0;
    const float prevGroundY  = static_cast<float>(luaL_optnumber(L, i++, 0.0));
    const float lastFacing   = static_cast<float>(luaL_optnumber(L, i++, 0.0));
    const int   lastMode     = static_cast<int>(luaL_optinteger(L, i++, kLegModeAir));

    const LegIK::GaitSample gait = LegIK::SampleGait(phase, stanceRatio, swingFrames);

    const float ankleLift = RoundHalfUp(pushHeight * gait.push * blend);
    const float footW = std::max(1.0f, footBaseW - RoundHalfUp(pushToe * gait.push * blend));

    const float rr = (gait.roll + 1.0f) * 0.5f;
    const float rolled = -heelLean + (footLean + heelLean) * rr;
    const float footOff = RoundHalfUp(footLean + (rolled - footLean) * blend);

    const float targetX = hipX + gait.sweep * amp * facing;

    int mode = kLegModeAir;
    float targetY = 0.0f;
    float groundY = 0.0f;
    bool groundYValid = false;

    if (grounded) {
        const float maxY = hipY + legLength + snapDistance;
        const Physics::GroundHit hit = Physics::RaycastDown(*actors, targetX, hipY, maxY, ignoreBody);
        if (hit.hit) {
            mode = kLegModeGround;
            groundY = hasGroundY ? LegIK::Approach(prevGroundY, hit.y, smoothing, dt) : hit.y;
            groundYValid = true;
            targetY = groundY - gait.lift * stepHeight * blend;
        } else {
            targetY = hipY + legLength * airReach;
        }
    } else {
        targetY = hipY + legLength * airReach * airFactor / airReach;
    }

    targetY = ClampF(targetY, hipY + legLength * 0.25f, hipY + legLength);

    float offX, offY;
    if (!hasFoot) {
        offX = 0.0f;
        offY = 0.0f;
    } else {
        if (facing != lastFacing || mode != lastMode) {
            offX = prevFootX - targetX;
            offY = prevFootY - targetY;
        } else {
            offX = prevOffX;
            offY = prevOffY;
        }
        offX = LegIK::Approach(offX, 0.0f, smoothing, dt);
        offY = LegIK::Approach(offY, 0.0f, smoothing, dt);
    }

    const float footX = targetX + offX;
    const float footY = targetY + offY;

    const float soleDX = RoundHalfUp(footX - hipX);
    const float soleDY = RoundHalfUp(footY - hipY);

    const float slack = (mode == kLegModeGround)
        ? (legLength - soleDY)
        : std::numeric_limits<float>::infinity();

    lua_pushnumber(L, gait.load);
    lua_pushnumber(L, gait.lift);
    lua_pushnumber(L, ankleLift);
    lua_pushnumber(L, footW);
    lua_pushnumber(L, footOff);
    lua_pushnumber(L, footX);
    lua_pushnumber(L, footY);
    if (groundYValid) lua_pushnumber(L, groundY); else lua_pushnil(L);
    lua_pushnumber(L, offX);
    lua_pushnumber(L, offY);
    lua_pushnumber(L, soleDX);
    lua_pushnumber(L, soleDY);
    lua_pushinteger(L, mode);
    lua_pushnumber(L, slack);
    return 14;
}

// IK.RasterizeHip fuses the per-column FillRect loop -- one crossing per
// pixel-wide sheared column -- into one call. Each leg table must already
// carry this frame's `hipX` and `hipRow`, which SolveLeg leaves on it;
// `hipColRows` is baked once at construction.
//
// Reproduces the forward-local-then-mirror rule from IK.txt section 13
// exactly: centroid, slope, block edge and each column's row are computed and
// rounded in forward-local space, and facing is applied only to the final x.
// Getting that order wrong is what made the pelvis pop asymmetrically on a turn.
//
// Args: sprite, legs, hipWidth, hipRear, hipRise, canvasW, leanX, facing,
//       r, g, b, a, hipColRows
int Lua_IKRasterizeHip(lua_State* L) {
    PixelSprite* sprite = LuaBinding::Value<PixelSprite*>::Get(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const int legsIdx = 2;
    const int n = static_cast<int>(luaL_len(L, legsIdx));

    const int hipWidth   = static_cast<int>(luaL_checkinteger(L, 3));
    const float hipRear  = static_cast<float>(luaL_checknumber(L, 4));
    const float hipRise  = static_cast<float>(luaL_checknumber(L, 5));
    const float canvasW  = static_cast<float>(luaL_checknumber(L, 6));
    const float leanX    = static_cast<float>(luaL_checknumber(L, 7));
    const float facing   = static_cast<float>(luaL_checknumber(L, 8));
    const float r        = static_cast<float>(luaL_checknumber(L, 9));
    const float g        = static_cast<float>(luaL_checknumber(L, 10));
    const float b        = static_cast<float>(luaL_checknumber(L, 11));
    const float a        = static_cast<float>(luaL_optnumber(L, 12, 1.0));
    luaL_checktype(L, 13, LUA_TTABLE);
    const int rowsIdx = 13;

    if (hipWidth <= 0 || n <= 0) return 0;

    float sumF = 0.0f, sumRow = 0.0f;
    float rearF = 0.0f, rearRow = 0.0f, foreF = 0.0f, foreRow = 0.0f;
    bool haveRear = false, haveFore = false;

    for (int li = 1; li <= n; ++li) {
        lua_rawgeti(L, legsIdx, li);
        lua_getfield(L, -1, "hipX");
        const float f = static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, -1, "hipRow");
        const float row = static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        lua_pop(L, 1); // leg table

        sumF += f;
        sumRow += row;
        if (!haveRear || f < rearF) { rearF = f; rearRow = row; haveRear = true; }
        if (!haveFore || f > foreF) { foreF = f; foreRow = row; haveFore = true; }
    }

    const float centerF = sumF / static_cast<float>(n);
    const float midRow  = sumRow / static_cast<float>(n);

    const float slope = (foreRow - rearRow) / std::max(1.0f, static_cast<float>(hipWidth - 1));

    const float leftF = RoundHalfUp(centerF - hipRear - (hipWidth - 1) * 0.5f);
    const float top   = midRow - hipRise;

    const float midCol = canvasW * 0.5f + leanX;
    const Color color(r, g, b, a);

    for (int c = 0; c < hipWidth; ++c) {
        lua_rawgeti(L, rowsIdx, c);
        const int rows = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        if (rows <= 0) continue;

        const float f = leftF + static_cast<float>(c);
        const int x = static_cast<int>(midCol + f * facing);
        const int y = static_cast<int>(RoundHalfUp(top + (f - centerF) * slope));
        sprite->FillRect(x, y, 1, rows, color);
    }

    return 0;
}

void RegisterLegIK(lua_State* L, ActorRegistry* actors) {
    LuaBinding::Table(L)
        .Raw("SolveTwoBone", &Lua_IKSolveTwoBone)
        .Raw("GaitPose",     &Lua_IKGaitPose)
        .Raw("KneeBulge",    &Lua_IKKneeBulge)
        .Raw("Approach",     &Lua_IKApproach)
        .RawWithContext("SolveLegFrame", actors, &Lua_IKSolveLegFrame)
        .Raw("RasterizeHip", &Lua_IKRasterizeHip)
        .Finish("IK");
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
    RegisterPhysics(L, context.actors);
    RegisterLegIK(L, context.actors);
}