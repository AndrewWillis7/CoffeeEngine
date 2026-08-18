#pragma once
#include <vector>

class ActorRegistry;
class RigidBody2D;

// Dynamic, per-pixel, non-baked 2D lighting -- the Noita-torch/campfire
// ask. This is an engine-wide subsystem, not a Lua-creatable per-actor
// thing (same category as Renderer2D), owned in main.cpp and ticked once
// a frame via the bare Lua global UpdateLighting(deltaTime) -- same
// "engine-wide subsystem, called once a frame from a bare global" shape
// SyncCamera() already uses (see ScriptBindings.cpp).
//
// Every Update() call:
//   1. Scans every RigidBody2D for one with a LightEmitterConfig attached
//      (RigidBody2D::lightEmitter, same non-owning-pointer tag convention
//      as playerConfig/camera/collisionShape -- see LightEmitterConfig.h).
//   2. For each light, radially raymarches outward from its position (a
//      full circle for Type::Point, an angular arc for Type::Cone) through
//      world space, against every sprite-backed body within its radius.
//   3. At the first solid pixel a given ray touches, tints it toward the
//      light's color (PixelSprite::AccumulateLightTint) with distance/
//      angle falloff -- then, UNLESS that pixel's owning body has
//      RigidBody2D::lightBlocking set, the ray keeps marching past it
//      (so light passes through decoration but not walls). A blocking
//      body's surface still gets tinted (it's lit, facing the source) but
//      the ray stops there, casting a hard shadow behind it.
//
// Nothing here is baked or cached across frames -- move the light (or
// whatever it's shining on) and the very next Update() call reflects it,
// per the "walk up to the campfire" requirement. The only per-frame
// memory is, per (light, body) pair, the pixel rect it touched LAST
// frame (m_PrevLitRects below) -- purely so a light that moved away (or
// was removed/unset) cleanly erases its own stale tint instead of
// leaving a ghost behind; it plays no part in the actual lighting math.
//
// PERFORMANCE NOTE (flagged, not hidden): there's no broad-phase/spatial
// index anywhere in the engine yet (see the "on the horizon" list), so
// both the light scan and the per-light candidate-body scan below are
// O(n) over every body, same convention ActorRegistry::GetPlayerActor()
// already uses -- and the raymarch itself is O(rays * steps *
// candidates) PER light. Fine for a handful of torches at engine scale
// today; revisit (spatial partitioning for candidates, fewer/adaptive
// rays, a max-lights-per-frame budget) if actor/light counts grow.
class LightingSystem {
public:
    void Update(ActorRegistry& actors, float deltaTime);

private:
    // One light-frame's worth of "which body, which pixel rect" a single
    // light touched -- rebuilt fresh each Update() call from what THIS
    // frame's raymarch actually hit, then swapped into m_PrevLitRects at
    // the end so next frame's reset pass knows what to clear even if the
    // light moved, changed shape, or vanished entirely in between.
    struct LitRect {
        RigidBody2D* body = nullptr;
        int minX = 0, minY = 0, maxX = 0, maxY = 0;
    };

    std::vector<LitRect> m_PrevLitRects;

    float m_Time = 0.0f; // accumulated for per-light flicker sampling
};