#include "SceneFields.h"
#include "Core/ActorRegistry.h"
#include "Core/Physics/RigidBody2D.h"
#include "Core/Physics/CollisionShape2D.h"
#include "Core/Gameplay/Camera2D.h"
#include "Core/Gameplay/LightEmitterConfig.h"
#include "Core/Gameplay/PlayerActorConfig.h"
#include "Core/Gameplay/Terrain/TerrainChunk.h"
#include "Core/Gameplay/DebugQuadConfig.h"
#include "Core/Scripting/ScriptTunable.h"
#include "Renderer/PixelSprite.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

namespace SceneFields {
namespace {

constexpr float kDegToRad = 0.01745329252f;
constexpr float kRadToDeg = 57.29577951f;

const char* const kColliderTypes[] = {"box", "circle"};
const char* const kLightTypes[] = {"point", "cone"};

// Script field ids start here, well clear of the table however long it grows.
constexpr int kScriptBase = 10000;
constexpr const char* kScriptPrefix = "script.";
constexpr size_t kScriptPrefixLength = 7;

std::string Fixed(float value, int decimals) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    return buffer;
}

std::string NameOf(const RigidBody2D& body) {
    return body.name.empty() ? std::string("an unnamed body") : body.name;
}

// --- Presence tests ---------------------------------------------------------
// A terrain body resolves as a heightmap and deliberately carries no box (see
// RigidBody2D::terrain), so the collider fields stay out of its way entirely.
bool CanCollide(const RigidBody2D& b)  { return !b.terrain; }
bool HasCollider(const RigidBody2D& b) { return b.collisionShape != nullptr; }
bool HasBox(const RigidBody2D& b)      { return b.collisionShape && b.collisionShape->GetType() == CollisionShape2D::Type::Box; }
bool HasCircle(const RigidBody2D& b)   { return b.collisionShape && b.collisionShape->GetType() == CollisionShape2D::Type::Circle; }
bool HasLight(const RigidBody2D& b)    { return b.lightEmitter != nullptr; }
bool HasCone(const RigidBody2D& b)     { return b.lightEmitter && b.lightEmitter->type == LightEmitterConfig::Type::Cone; }
bool HasActor(const RigidBody2D& b)    { return b.playerConfig != nullptr; }
bool HasCamera(const RigidBody2D& b)   { return b.camera != nullptr; }
bool HasTerrain(const RigidBody2D& b)  { return b.terrain && b.sprite; }
bool HasDebugQuad(const RigidBody2D& b) { return b.debugQuad != nullptr; }
bool HasSprite(const RigidBody2D& b)   { return b.sprite != nullptr && !b.terrain; }
bool IsPart(const RigidBody2D& b)      { return b.partOf != nullptr; }
bool HasMass(const RigidBody2D& b)     { return b.mass > 0.0f; }

// --- Context locks ----------------------------------------------------------

// Where a body is, and how it is turned: meaningless to edit on a part (its
// owner rewrites it every frame) or on a camera that follows something.
std::string PlacementLock(const RigidBody2D& b) {
    if (b.partOf) {
        return "Drawn for " + NameOf(*b.partOf) + ", whose script moves it every frame. "
               "Clicking it in the scene selects " + NameOf(*b.partOf) + ".";
    }
    if (b.camera && b.camera->followTarget && b.camera->followSmoothing > 0.0f) {
        return "Follows " + NameOf(*b.camera->followTarget) + " every frame. Use Focus offset to frame "
               "the view, or set Follow smoothing to 0 to place the camera by hand.";
    }
    return {};
}

// How big a body draws: also meaningless on a part, and a camera has no
// shape at all -- its view is its viewport times its zoom.
std::string ShapeLock(const RigidBody2D& b) {
    if (b.camera) return "A camera has no shape. What it shows is Viewport times Zoom out.";
    if (b.debugQuad) return "A debug quad sizes in whole cells. Use Cells wide / Cells tall.";
    return PlacementLock(b);
}

// The table. Order is load order as well as draw order: collider.enabled must
// come before the collider.* fields it brings into existence, and each type
// switch before the fields that depend on which type it is.
const Field kFields[] = {
    // --- Transform ---------------------------------------------------------
    {.key = "position", .label = "Position", .group = Group::Transform, .type = Type::Vec2,
     .speed = 0.25f, .decimals = 0,
     .get = [](const RigidBody2D& b) { return Value::Of(b.transform.position); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.transform.position = v.V2(); },
     .lockedWhy = PlacementLock},
    {.key = "rotation", .label = "Rotation", .group = Group::Transform, .type = Type::Angle,
     .speed = 0.5f, .decimals = 0,
     .get = [](const RigidBody2D& b) { return Value::Of(b.transform.rotation); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.transform.rotation = v.F(); },
     .lockedWhy = ShapeLock},
    {.key = "scale", .label = "Scale", .group = Group::Transform, .type = Type::Vec2,
     .speed = 0.01f, .decimals = 2,
     .get = [](const RigidBody2D& b) { return Value::Of(b.transform.scale); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.transform.scale = v.V2(); },
     .lockedWhy = ShapeLock},
    {.key = "size", .label = "Size", .group = Group::Transform, .type = Type::Vec2,
     .speed = 0.25f, .decimals = 0, .minValue = 0.0f, .maxValue = 4096.0f,
     .get = [](const RigidBody2D& b) { return Value::Of(b.size); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.size = v.V2(); },
     .lockedWhy = ShapeLock},

    // --- Body --------------------------------------------------------------
    {.key = "info.name", .label = "Name", .group = Group::Body, .type = Type::Info, .section = "Identity",
     .describe = [](const RigidBody2D& b) { return b.name.empty() ? std::string("(none)") : b.name; },
     .lockReason = "Set by the scripts with SetName. It is also what saved edits are keyed by, "
                   "so it stays exactly as the scripts wrote it."},
    {.key = "info.partOf", .label = "Part of", .group = Group::Body, .type = Type::Info,
     .present = IsPart,
     .describe = [](const RigidBody2D& b) { return NameOf(*b.partOf); },
     .lockReason = "Declared by the scripts with SetPartOf: this body is drawn on its owner's behalf."},
    {.key = "info.sprite", .label = "Sprite", .group = Group::Body, .type = Type::Info,
     .present = HasSprite,
     .describe = [](const RigidBody2D& b) {
         return std::to_string(b.sprite->GetWidth()) + " x " + std::to_string(b.sprite->GetHeight());
     },
     .lockReason = "The texels the scripts painted. Size and Scale change how big it draws; "
                   "the pixels themselves come from the script."},
    {.key = "info.shader", .label = "Shader", .group = Group::Body, .type = Type::Info,
     .describe = [](const RigidBody2D& b) { return std::string(b.shader ? "custom" : "default"); },
     .lockReason = "Chosen by the scripts with SetShader."},
    {.key = "color", .label = "Tint", .group = Group::Body, .type = Type::Color, .section = "Look",
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f,
     .get = [](const RigidBody2D& b) { return Value::Of(b.color); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.color = v.C(); }},
    {.key = "lightBlocking", .label = "Blocks light", .group = Group::Body, .type = Type::Bool,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightBlocking); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightBlocking = v.B(); }},
    {.key = "mass", .label = "Mass", .group = Group::Body, .type = Type::Float, .section = "Physics",
     .speed = 0.01f, .decimals = 2, .minValue = 0.0f, .maxValue = 1000.0f,
     .get = [](const RigidBody2D& b) { return Value::Of(b.mass); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.mass = v.F(); }},
    {.key = "drag", .label = "Drag", .group = Group::Body, .type = Type::Float,
     .speed = 0.01f, .decimals = 2, .minValue = 0.0f, .maxValue = 100.0f,
     .get = [](const RigidBody2D& b) { return Value::Of(b.drag); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.drag = v.F(); }},
    {.key = "raycastTarget", .label = "Raycast target", .group = Group::Body, .type = Type::Bool,
     .get = [](const RigidBody2D& b) { return Value::Of(b.raycastTarget); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.raycastTarget = v.B(); }},
    {.key = "info.velocity", .label = "Velocity", .group = Group::Body, .type = Type::Info,
     .describe = [](const RigidBody2D& b) { return Fixed(b.velocity.x, 1) + ", " + Fixed(b.velocity.y, 1); },
     .lockReason = "Physics state, rewritten every frame by movement and collision. "
                   "Mass and Drag are what shape it."},
    {.key = "info.grounded", .label = "Grounded", .group = Group::Body, .type = Type::Info,
     .present = HasMass,
     .describe = [](const RigidBody2D& b) { return std::string(b.IsGrounded() ? "yes" : "no"); },
     .lockReason = "Worked out by collision every frame."},

    // --- Collider ----------------------------------------------------------
    {.key = "collider.enabled", .label = "Has collider", .group = Group::Collider, .type = Type::Bool,
     .present = CanCollide,
     .get = [](const RigidBody2D& b) { return Value::Of(b.collisionShape != nullptr); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry& actors) {
         if (v.B() == (b.collisionShape != nullptr)) return;
         if (!v.B()) {
             // Detached, not freed: the registry owns it and frees it with
             // the rest on the next reload.
             b.collisionShape = nullptr;
             return;
         }
         // Sized to what is drawn, which is what "give this a collider" means.
         const Vector2 half = b.size * b.transform.scale * 0.5f;
         b.collisionShape = actors.CreateBoxCollisionShape(std::fabs(half.x), std::fabs(half.y));
     },
     .lockedWhy = [](const RigidBody2D& b) {
         return b.partOf ? std::string("A part never collides: its owner does.") : std::string();
     }},
    {.key = "collider.type", .label = "Shape", .group = Group::Collider, .type = Type::Enum,
     .enumNames = kColliderTypes, .enumCount = 2, .present = HasCollider,
     .get = [](const RigidBody2D& b) {
         return Value::Of(b.collisionShape->GetType() == CollisionShape2D::Type::Circle ? 1 : 0);
     },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) {
         const auto type = v.I() == 1 ? CollisionShape2D::Type::Circle : CollisionShape2D::Type::Box;
         if (type == b.collisionShape->GetType()) return;
         // Carry the size across so switching shape doesn't also resize it:
         // the circle inscribed in the box, or the box around the circle.
         if (type == CollisionShape2D::Type::Circle) {
             const Vector2 half = b.collisionShape->GetHalfExtents();
             b.collisionShape->SetRadius(half.x > half.y ? half.y : half.x);
         } else {
             const float r = b.collisionShape->GetRadius();
             b.collisionShape->SetHalfExtents({r, r});
         }
         b.collisionShape->SetType(type);
     }},
    {.key = "collider.offset", .label = "Offset", .group = Group::Collider, .type = Type::Vec2,
     .speed = 0.25f, .decimals = 1, .present = HasCollider,
     .get = [](const RigidBody2D& b) { return Value::Of(b.collisionShape->GetOffset()); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.collisionShape->SetOffset(v.V2()); }},
    {.key = "collider.halfExtents", .label = "Half size", .group = Group::Collider, .type = Type::Vec2,
     .speed = 0.25f, .decimals = 1, .minValue = 0.0f, .maxValue = 4096.0f, .present = HasBox,
     .get = [](const RigidBody2D& b) { return Value::Of(b.collisionShape->GetHalfExtents()); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.collisionShape->SetHalfExtents(v.V2()); }},
    {.key = "collider.radius", .label = "Radius", .group = Group::Collider, .type = Type::Float,
     .speed = 0.25f, .decimals = 1, .minValue = 0.0f, .maxValue = 4096.0f, .present = HasCircle,
     .get = [](const RigidBody2D& b) { return Value::Of(b.collisionShape->GetRadius()); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.collisionShape->SetRadius(v.F()); }},

    // --- Light -------------------------------------------------------------
    {.key = "light.type", .label = "Light", .group = Group::Light, .type = Type::Enum,
     .enumNames = kLightTypes, .enumCount = 2, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->type == LightEmitterConfig::Type::Cone ? 1 : 0); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) {
         b.lightEmitter->type = v.I() == 1 ? LightEmitterConfig::Type::Cone : LightEmitterConfig::Type::Point;
     }},
    {.key = "light.color", .label = "Color", .group = Group::Light, .type = Type::Color,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->color); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->color = v.C(); }},
    {.key = "light.radius", .label = "Radius", .group = Group::Light, .type = Type::Float,
     .speed = 1.0f, .decimals = 0, .minValue = 0.0f, .maxValue = 4000.0f, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->radius); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->radius = v.F(); }},
    {.key = "light.brightness", .label = "Brightness", .group = Group::Light, .type = Type::Float,
     .speed = 0.01f, .decimals = 2, .minValue = 0.0f, .maxValue = 5.0f, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->brightness); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->brightness = v.F(); }},
    {.key = "light.falloff", .label = "Falloff", .group = Group::Light, .type = Type::Float,
     .speed = 0.01f, .decimals = 2, .minValue = 0.1f, .maxValue = 8.0f, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->falloffExponent); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->falloffExponent = v.F(); }},
    {.key = "light.toneSteps", .label = "Tone steps", .group = Group::Light, .type = Type::Int,
     .minValue = 0.0f, .maxValue = 8.0f, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->toneSteps); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->toneSteps = v.I(); }},
    {.key = "light.coneAngle", .label = "Cone width", .group = Group::Light, .type = Type::Angle,
     .speed = 0.5f, .decimals = 0, .minValue = 0.0f, .maxValue = 360.0f, .present = HasCone,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->coneAngleRad); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->coneAngleRad = v.F(); }},
    {.key = "light.coneDirection", .label = "Cone aim", .group = Group::Light, .type = Type::Angle,
     .speed = 0.5f, .decimals = 0, .present = HasCone,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->coneDirectionRad); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->coneDirectionRad = v.F(); }},
    {.key = "light.useOwnerRotation", .label = "Aim turns with body", .group = Group::Light, .type = Type::Bool,
     .present = HasCone,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->useOwnerRotation); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->useOwnerRotation = v.B(); }},
    {.key = "light.flicker", .label = "Flicker", .group = Group::Light, .type = Type::Bool,
     .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->flicker); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->flicker = v.B(); }},
    {.key = "light.flickerSpeed", .label = "Flicker hz", .group = Group::Light, .type = Type::Float,
     .speed = 0.05f, .decimals = 1, .minValue = 0.0f, .maxValue = 60.0f, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->flickerSpeed); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->flickerSpeed = v.F(); }},
    {.key = "light.flickerAmount", .label = "Flicker amount", .group = Group::Light, .type = Type::Float,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->flickerIntensityAmount); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->flickerIntensityAmount = v.F(); }},
    {.key = "light.flickerShift", .label = "Flicker tint", .group = Group::Light, .type = Type::Color,
     .speed = 0.005f, .decimals = 2, .minValue = -1.0f, .maxValue = 1.0f, .present = HasLight,
     .get = [](const RigidBody2D& b) { return Value::Of(b.lightEmitter->flickerColorShift); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.lightEmitter->flickerColorShift = v.C(); }},

    // --- Actor -------------------------------------------------------------
    {.key = "actor.moveSpeed", .label = "Walk speed", .group = Group::Actor, .type = Type::Float,
     .speed = 0.25f, .decimals = 1, .minValue = 0.0f, .maxValue = 2000.0f, .present = HasActor,
     .get = [](const RigidBody2D& b) { return Value::Of(b.playerConfig->moveSpeed); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.playerConfig->moveSpeed = v.F(); }},
    {.key = "actor.jumpForce", .label = "Jump force", .group = Group::Actor, .type = Type::Float,
     .speed = 1.0f, .decimals = 0, .minValue = 0.0f, .maxValue = 5000.0f, .present = HasActor,
     .get = [](const RigidBody2D& b) { return Value::Of(b.playerConfig->jumpForce); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.playerConfig->jumpForce = v.F(); }},
    {.key = "info.input", .label = "Input", .group = Group::Actor, .type = Type::Info,
     .present = HasActor,
     .describe = [](const RigidBody2D&) { return std::string("held off"); },
     .lockReason = "Switched off while edit mode is on, so WASD pans the view instead of walking. "
                   "Outside edit mode the scripts decide it."},

    // --- Camera ------------------------------------------------------------
    {.key = "camera.active", .label = "Active", .group = Group::Camera, .type = Type::Bool,
     .present = HasCamera,
     .get = [](const RigidBody2D& b) { return Value::Of(b.camera->active); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.camera->active = v.B(); }},
    {.key = "camera.viewport", .label = "Viewport", .group = Group::Camera, .type = Type::Vec2,
     .speed = 0.5f, .decimals = 0, .minValue = 16.0f, .maxValue = 4096.0f, .present = HasCamera,
     .get = [](const RigidBody2D& b) { return Value::Of(b.camera->viewportSize); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.camera->viewportSize = v.V2(); }},
    {.key = "camera.aspect", .label = "Aspect", .group = Group::Camera, .type = Type::Vec2,
     .speed = 0.05f, .decimals = 2, .minValue = 0.0f, .maxValue = 64.0f, .present = HasCamera,
     .get = [](const RigidBody2D& b) { return Value::Of(b.camera->targetAspect); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.camera->targetAspect = v.V2(); }},
    {.key = "camera.zoomOut", .label = "Zoom out", .group = Group::Camera, .type = Type::Int,
     .minValue = 1.0f, .maxValue = 8.0f, .present = HasCamera,
     .get = [](const RigidBody2D& b) { return Value::Of(b.camera->GetZoomOut()); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.camera->SetZoomOut(v.I()); }},
    {.key = "camera.followSmoothing", .label = "Follow smoothing", .group = Group::Camera, .type = Type::Float,
     .speed = 0.05f, .decimals = 2, .minValue = 0.0f, .maxValue = 50.0f, .present = HasCamera,
     .get = [](const RigidBody2D& b) { return Value::Of(b.camera->followSmoothing); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.camera->followSmoothing = v.F(); }},
    {.key = "camera.focusOffset", .label = "Focus offset", .group = Group::Camera, .type = Type::Vec2,
     .speed = 0.25f, .decimals = 0, .present = HasCamera,
     .get = [](const RigidBody2D& b) { return Value::Of(b.camera->focusOffset); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.camera->focusOffset = v.V2(); }},
    {.key = "info.follows", .label = "Follows", .group = Group::Camera, .type = Type::Info,
     .present = HasCamera,
     .describe = [](const RigidBody2D& b) {
         return b.camera->followTarget ? NameOf(*b.camera->followTarget) : std::string("nothing");
     },
     .lockReason = "Set by the scripts with camera:Follow(body)."},

    // --- Terrain -----------------------------------------------------------
    {.key = "info.chunk", .label = "Chunk", .group = Group::Terrain, .type = Type::Info,
     .present = HasTerrain,
     .describe = [](const RigidBody2D& b) {
         return std::to_string(b.sprite->GetWidth()) + " x " + std::to_string(b.sprite->GetHeight());
     },
     .lockReason = "Fixed by the sprite the scripts created the chunk on."},
    {.key = "info.blades", .label = "Blades", .group = Group::Terrain, .type = Type::Info,
     .present = HasTerrain,
     .describe = [](const RigidBody2D& b) { return std::to_string(b.terrain->GetBladeCount()); },
     .lockReason = "Grown by generation. Grass density and heights decide how many."},
    {.key = "terrain.seed", .label = "Seed", .group = Group::Terrain, .type = Type::Int, .section = "Surface",
     .speed = 0.2f, .decimals = 0, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->seed); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->seed = v.I(); },
     .regenerates = true},
    {.key = "terrain.surfaceAmplitude", .label = "Relief", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.05f, .decimals = 1, .minValue = 0.0f, .maxValue = 64.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->surfaceAmplitude); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->surfaceAmplitude = v.F(); },
     .regenerates = true},
    {.key = "terrain.surfaceFrequency", .label = "Roughness", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.0005f, .decimals = 3, .minValue = 0.001f, .maxValue = 0.5f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->surfaceFrequency); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->surfaceFrequency = v.F(); },
     .regenerates = true},
    {.key = "terrain.surfaceOctaves", .label = "Octaves", .group = Group::Terrain, .type = Type::Int,
     .minValue = 1.0f, .maxValue = 8.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->surfaceOctaves); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->surfaceOctaves = v.I(); },
     .regenerates = true},
    {.key = "terrain.surfaceLacunarity", .label = "Lacunarity", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.01f, .decimals = 2, .minValue = 1.0f, .maxValue = 4.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->surfaceLacunarity); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->surfaceLacunarity = v.F(); },
     .regenerates = true},
    {.key = "terrain.surfaceGain", .label = "Gain", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->surfaceGain); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->surfaceGain = v.F(); },
     .regenerates = true},
    {.key = "terrain.surfaceOffset", .label = "Surface depth", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.25f, .decimals = 0, .minValue = 0.0f, .maxValue = 512.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->surfaceOffset); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->surfaceOffset = v.F(); },
     .regenerates = true},
    {.key = "terrain.dirtDark", .label = "Dirt dark", .group = Group::Terrain, .type = Type::Color, .section = "Dirt",
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->dirtDark); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->dirtDark = v.C(); },
     .regenerates = true},
    {.key = "terrain.dirtLight", .label = "Dirt light", .group = Group::Terrain, .type = Type::Color,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->dirtLight); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->dirtLight = v.C(); },
     .regenerates = true},
    {.key = "terrain.rockColor", .label = "Rock", .group = Group::Terrain, .type = Type::Color,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->rockColor); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->rockColor = v.C(); },
     .regenerates = true},
    {.key = "terrain.dirtFrequency", .label = "Dirt grain", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.002f, .decimals = 3, .minValue = 0.001f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->dirtFrequency); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->dirtFrequency = v.F(); },
     .regenerates = true},
    {.key = "terrain.dirtOctaves", .label = "Dirt octaves", .group = Group::Terrain, .type = Type::Int,
     .minValue = 1.0f, .maxValue = 8.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->dirtOctaves); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->dirtOctaves = v.I(); },
     .regenerates = true},
    {.key = "terrain.dirtToneSteps", .label = "Dirt tones", .group = Group::Terrain, .type = Type::Int,
     .minValue = 1.0f, .maxValue = 16.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->dirtToneSteps); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->dirtToneSteps = v.I(); },
     .regenerates = true},
    {.key = "terrain.rockChance", .label = "Rock chance", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.001f, .decimals = 3, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->rockChance); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->rockChance = v.F(); },
     .regenerates = true},
    {.key = "terrain.depthDarkening", .label = "Depth shade", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->depthDarkening); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->depthDarkening = v.F(); },
     .regenerates = true},
    {.key = "terrain.topsoilColor", .label = "Topsoil", .group = Group::Terrain, .type = Type::Color,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->topsoilColor); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->topsoilColor = v.C(); },
     .regenerates = true},
    {.key = "terrain.topsoilDepth", .label = "Topsoil depth", .group = Group::Terrain, .type = Type::Int,
     .minValue = 0.0f, .maxValue = 16.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->topsoilDepth); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->topsoilDepth = v.I(); },
     .regenerates = true},
    {.key = "terrain.grassDark", .label = "Grass dark", .group = Group::Terrain, .type = Type::Color, .section = "Grass",
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->grassDark); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->grassDark = v.C(); },
     .regenerates = true},
    {.key = "terrain.grassLight", .label = "Grass light", .group = Group::Terrain, .type = Type::Color,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->grassLight); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->grassLight = v.C(); },
     .regenerates = true},
    {.key = "terrain.grassMinHeight", .label = "Shortest", .group = Group::Terrain, .type = Type::Int,
     .minValue = 1.0f, .maxValue = static_cast<float>(TerrainChunk::kMaxBladeHeight), .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->grassMinHeight); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->grassMinHeight = v.I(); },
     .regenerates = true},
    {.key = "terrain.grassMaxHeight", .label = "Tallest", .group = Group::Terrain, .type = Type::Int,
     .minValue = 1.0f, .maxValue = static_cast<float>(TerrainChunk::kMaxBladeHeight), .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->grassMaxHeight); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->grassMaxHeight = v.I(); },
     .regenerates = true},
    {.key = "terrain.grassDensity", .label = "Density", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.005f, .decimals = 2, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->grassDensity); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->grassDensity = v.F(); },
     .regenerates = true},
    {.key = "terrain.swayAmplitude", .label = "Sway", .group = Group::Terrain, .type = Type::Float,
     .section = "Wind and touch",
     .speed = 0.02f, .decimals = 2, .minValue = 0.0f, .maxValue = 8.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->swayAmplitude); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->swayAmplitude = v.F(); }},
    {.key = "terrain.swaySpeed", .label = "Sway speed", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.02f, .decimals = 2, .minValue = 0.0f, .maxValue = 10.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->swaySpeed); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->swaySpeed = v.F(); }},
    {.key = "terrain.swayPhasePerTexel", .label = "Wave spacing", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.002f, .decimals = 3, .minValue = 0.0f, .maxValue = 1.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->swayPhasePerTexel); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->swayPhasePerTexel = v.F(); }},
    {.key = "terrain.bendStiffness", .label = "Stiffness", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.5f, .decimals = 0, .minValue = 0.0f, .maxValue = 1000.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->bendStiffness); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->bendStiffness = v.F(); }},
    {.key = "terrain.bendDamping", .label = "Damping", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.05f, .decimals = 1, .minValue = 0.0f, .maxValue = 100.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->bendDamping); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->bendDamping = v.F(); }},
    {.key = "terrain.maxBend", .label = "Max bend", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.02f, .decimals = 1, .minValue = 0.0f, .maxValue = 8.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->maxBend); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->maxBend = v.F(); }},
    {.key = "terrain.disturbStrength", .label = "Push", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.5f, .decimals = 0, .minValue = 0.0f, .maxValue = 1000.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->disturbStrength); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->disturbStrength = v.F(); }},
    {.key = "terrain.disturbPadding", .label = "Push reach", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.05f, .decimals = 1, .minValue = 0.0f, .maxValue = 32.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->disturbPadding); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->disturbPadding = v.F(); }},
    {.key = "terrain.disturbSpeedScale", .label = "Push per speed", .group = Group::Terrain, .type = Type::Float,
     .speed = 0.0002f, .decimals = 4, .minValue = 0.0f, .maxValue = 0.2f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->disturbSpeedScale); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->disturbSpeedScale = v.F(); }},
    {.key = "terrain.maxStepHeight", .label = "Max step", .group = Group::Terrain, .type = Type::Float,
     .section = "Collision",
     .speed = 0.1f, .decimals = 1, .minValue = 0.0f, .maxValue = 64.0f, .present = HasTerrain,
     .get = [](const RigidBody2D& b) { return Value::Of(b.terrain->maxStepHeight); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) { b.terrain->maxStepHeight = v.F(); }},

    // --- Asset ---------------------------------------------------------------
    // A placed debug quad's size is not free-form: it steps in whole
    // DebugQuadConfig::kCellSize texels per axis, so these are plain bounded
    // Ints rather than the Transform group's continuous Size field.
    {.key = "quad.cellsX", .label = "Cells wide", .group = Group::Asset, .type = Type::Int,
     .minValue = 1.0f, .maxValue = static_cast<float>(DebugQuadConfig::kMaxCells), .present = HasDebugQuad,
     .get = [](const RigidBody2D& b) { return Value::Of(b.debugQuad->cellsX); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) {
         b.debugQuad->cellsX = std::clamp(v.I(), 1, DebugQuadConfig::kMaxCells);
     },
     .regenerates = true},
    {.key = "quad.cellsY", .label = "Cells tall", .group = Group::Asset, .type = Type::Int,
     .minValue = 1.0f, .maxValue = static_cast<float>(DebugQuadConfig::kMaxCells), .present = HasDebugQuad,
     .get = [](const RigidBody2D& b) { return Value::Of(b.debugQuad->cellsY); },
     .set = [](RigidBody2D& b, const Value& v, ActorRegistry&) {
         b.debugQuad->cellsY = std::clamp(v.I(), 1, DebugQuadConfig::kMaxCells);
     },
     .regenerates = true},

    // --- World -------------------------------------------------------------
    // Engine-wide rather than per body, so they hang off WorldProxy().
    {.key = "world.gravity", .label = "Gravity", .group = Group::World, .type = Type::Vec2,
     .speed = 1.0f, .decimals = 0,
     .get = [](const RigidBody2D&) { return Value::Of(RigidBody2D::GetGravity()); },
     .set = [](RigidBody2D&, const Value& v, ActorRegistry&) { RigidBody2D::SetGravity(v.V2()); },
     .world = true},
};

constexpr size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);

// --- Script fields ----------------------------------------------------------
// One per distinct exposed name seen this session. A deque, so the Field and
// the strings it points into never move as more are added.
struct ScriptField {
    std::string key;   // "script." + name
    std::string label; // name
    Field field;
};

std::deque<ScriptField>& ScriptFields() {
    static std::deque<ScriptField> fields;
    return fields;
}

// The tunable's own hints win: the same name exposed again (after a reload,
// or by a later body) refreshes them.
void ApplyHints(Field& field, const ScriptTunable& tunable) {
    switch (tunable.kind) {
        case ScriptTunable::Kind::Bool:    field.type = Type::Bool; break;
        case ScriptTunable::Kind::Integer: field.type = Type::Int; break;
        default:                           field.type = Type::Float; break;
    }
    field.minValue = tunable.minValue;
    field.maxValue = tunable.maxValue;
    field.decimals = tunable.kind == ScriptTunable::Kind::Integer ? 0 : tunable.decimals;
    float step = tunable.step;
    if (step <= 0.0f) {
        const float span = tunable.maxValue - tunable.minValue;
        step = span > 0.0f ? span / 300.0f : 0.05f;
        if (tunable.kind == ScriptTunable::Kind::Integer) step = std::max(step, 0.1f);
    }
    field.speed = step;
}

int InternScript(const std::string& name) {
    std::deque<ScriptField>& fields = ScriptFields();
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].label == name) return kScriptBase + static_cast<int>(i);
    }
    fields.push_back({kScriptPrefix + name, name, Field{}});
    ScriptField& added = fields.back();
    added.field = Field{.key = added.key.c_str(), .label = added.label.c_str(),
                        .group = Group::Script, .type = Type::Float, .script = true};
    return kScriptBase + static_cast<int>(fields.size() - 1);
}

std::string Number(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.6g", static_cast<double>(value));
    return buffer;
}

// Succeeds only on exactly `count` numbers with nothing but blanks after, so a
// hand-edited line with a typo is refused rather than half-read.
bool ReadFloats(const std::string& text, float* out, int count) {
    const char* cursor = text.c_str();
    for (int i = 0; i < count; ++i) {
        char* end = nullptr;
        const float value = std::strtof(cursor, &end);
        if (end == cursor) return false;
        out[i] = value;
        cursor = end;
    }
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    return *cursor == '\0';
}

std::string Trimmed(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const size_t last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

} // namespace

const char* GroupName(Group group) {
    switch (group) {
        case Group::Transform: return "Transform";
        case Group::Body:      return "Body";
        case Group::Collider:  return "Collider";
        case Group::Light:     return "Light";
        case Group::Actor:     return "Actor";
        case Group::Script:    return "Script constants";
        case Group::Camera:    return "Camera";
        case Group::Terrain:   return "Terrain";
        case Group::Asset:     return "Asset";
        case Group::World:     return "World";
        default:               return "";
    }
}

size_t Count() { return kFieldCount; }

const Field& At(int id) {
    if (id >= kScriptBase) return ScriptFields()[static_cast<size_t>(id - kScriptBase)].field;
    return kFields[id];
}

int IndexOf(const std::string& key) {
    for (size_t i = 0; i < kFieldCount; ++i) {
        if (key == kFields[i].key) return static_cast<int>(i);
    }
    if (key.size() > kScriptPrefixLength && key.compare(0, kScriptPrefixLength, kScriptPrefix) == 0) {
        return InternScript(key.substr(kScriptPrefixLength));
    }
    return -1;
}

std::vector<int> FieldsOf(const RigidBody2D& body) {
    std::vector<int> ids;
    ids.reserve(kFieldCount + body.tunables.size());
    for (size_t i = 0; i < kFieldCount; ++i) ids.push_back(static_cast<int>(i));
    for (const ScriptTunable& tunable : body.tunables) {
        const int id = InternScript(tunable.name);
        ApplyHints(ScriptFields()[static_cast<size_t>(id - kScriptBase)].field, tunable);
        ids.push_back(id);
    }
    return ids;
}

RigidBody2D& WorldProxy() {
    static RigidBody2D proxy;
    return proxy;
}

const ScriptTunable* TunableFor(const Field& field, const RigidBody2D& body) {
    if (!field.script) return nullptr;
    const char* name = field.key + kScriptPrefixLength;
    for (const ScriptTunable& tunable : body.tunables) {
        if (tunable.name == name) return &tunable;
    }
    return nullptr;
}

bool IsPresent(const Field& field, const RigidBody2D& body) {
    // World fields on the proxy and nowhere else; everything else anywhere but.
    if (field.world != (&body == &WorldProxy())) return false;
    if (field.script) {
        float unused = 0.0f;
        const ScriptTunable* tunable = TunableFor(field, body);
        return tunable && tunable->Read(unused);
    }
    return !field.present || field.present(body);
}

bool IsEditable(const Field& field) {
    return field.script || field.set != nullptr;
}

std::string LockReason(const Field& field, const RigidBody2D& body) {
    if (field.type == Type::Info) return field.lockReason ? field.lockReason : "";
    return field.lockedWhy ? field.lockedWhy(body) : std::string();
}

Value Get(const Field& field, const RigidBody2D& body) {
    if (field.script) {
        float value = 0.0f;
        if (const ScriptTunable* tunable = TunableFor(field, body)) tunable->Read(value);
        return Value::Of(value);
    }
    return field.get ? field.get(body) : Value{};
}

void RegenerateTerrain(RigidBody2D& body) {
    if (body.terrain && body.sprite) body.terrain->Generate(*body.sprite);
}

void RegenerateDebugQuad(RigidBody2D& body, ActorRegistry& actors) {
    if (!body.debugQuad) return;
    const int width = body.debugQuad->cellsX * DebugQuadConfig::kCellSize;
    const int height = body.debugQuad->cellsY * DebugQuadConfig::kCellSize;
    // PixelSprite's dimensions are fixed at construction, so a resize means a
    // fresh one -- cheap and, unlike terrain chunks, never referenced by name
    // elsewhere, so the old one is simply left for the next Clear() to sweep.
    body.sprite = actors.CreateSolidSprite(width, height, 0.13f, 0.13f, 0.16f, 1.0f);
    body.size = Vector2(static_cast<float>(width), static_cast<float>(height));
    if (body.sprite) GenerateDebugQuadTexture(*body.sprite, body.debugQuad->cellsX, body.debugQuad->cellsY);
}

void Set(const Field& field, RigidBody2D& body, const Value& value, ActorRegistry& actors, bool regenerate) {
    if (!IsPresent(field, body)) return;
    if (field.script) {
        if (const ScriptTunable* tunable = TunableFor(field, body)) tunable->Write(value.F());
        return;
    }
    if (!field.set) return;
    field.set(body, value, actors);
    if (regenerate && field.regenerates) {
        RegenerateTerrain(body);
        RegenerateDebugQuad(body, actors);
    }
}

std::string Format(const Field& field, const Value& value) {
    switch (field.type) {
        case Type::Float: return Number(value.F());
        case Type::Angle: return Number(value.F() * kRadToDeg);
        case Type::Int:   return std::to_string(value.I());
        case Type::Bool:  return value.B() ? "true" : "false";
        case Type::Vec2:  return Number(value.v[0]) + " " + Number(value.v[1]);
        case Type::Color:
            return Number(value.v[0]) + " " + Number(value.v[1]) + " " +
                   Number(value.v[2]) + " " + Number(value.v[3]);
        case Type::Enum: {
            const int index = value.I();
            return (index >= 0 && index < field.enumCount) ? field.enumNames[index] : std::to_string(index);
        }
        case Type::Info: return {};
    }
    return {};
}

bool Parse(const Field& field, const std::string& rawText, Value& out) {
    const std::string text = Trimmed(rawText);
    out = Value{};
    // A script field has not been exposed yet when its line is read, so its
    // type is still unknown: read one number, which covers all three kinds.
    if (field.script) {
        if (text == "true" || text == "false") { out = Value::Of(text == "true"); return true; }
        return ReadFloats(text, out.v, 1);
    }
    switch (field.type) {
        case Type::Float:
            return ReadFloats(text, out.v, 1);
        case Type::Angle:
            if (!ReadFloats(text, out.v, 1)) return false;
            out.v[0] *= kDegToRad;
            return true;
        case Type::Int:
            if (!ReadFloats(text, out.v, 1)) return false;
            out = Value::Of(out.I());
            return true;
        case Type::Bool:
            if (text == "true" || text == "1" || text == "yes" || text == "on") { out = Value::Of(true); return true; }
            if (text == "false" || text == "0" || text == "no" || text == "off") { out = Value::Of(false); return true; }
            return false;
        case Type::Vec2:
            return ReadFloats(text, out.v, 2);
        case Type::Color:
            return ReadFloats(text, out.v, 4);
        case Type::Enum:
            for (int i = 0; i < field.enumCount; ++i) {
                if (text == field.enumNames[i]) { out = Value::Of(i); return true; }
            }
            return false;
        case Type::Info:
            return false;
    }
    return false;
}

std::string Describe(const Field& field, const Value& value) {
    switch (field.type) {
        case Type::Float: return Fixed(value.F(), field.decimals);
        case Type::Angle: return Fixed(value.F() * kRadToDeg, field.decimals) + " deg";
        case Type::Int:   return std::to_string(value.I());
        case Type::Bool:  return value.B() ? "on" : "off";
        case Type::Vec2:  return Fixed(value.v[0], field.decimals) + ", " + Fixed(value.v[1], field.decimals);
        case Type::Color:
            return Fixed(value.v[0], 2) + " " + Fixed(value.v[1], 2) + " " +
                   Fixed(value.v[2], 2) + " " + Fixed(value.v[3], 2);
        case Type::Enum:  return Format(field, value);
        case Type::Info:  return {};
    }
    return {};
}

Snapshot Capture(const RigidBody2D& body) {
    Snapshot snapshot;
    for (int id : FieldsOf(body)) {
        const Field& field = At(id);
        // Info rows are never recorded: the editor zeroing a dragged body's
        // velocity, say, is not an edit anyone wants saved.
        if (!IsEditable(field)) continue;
        const bool present = IsPresent(field, body);
        snapshot.entries.push_back({id, present, present ? Get(field, body) : Value{}});
    }
    return snapshot;
}

} // namespace SceneFields
