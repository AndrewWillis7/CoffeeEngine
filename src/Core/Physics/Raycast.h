#pragma once

class ActorRegistry;
class RigidBody2D;

// Downward ground queries against the live actor set: AABB-reject every
// candidate, then refine to the exact surface -- heightmap for terrain, a
// per-pixel column walk for a sprite-backed box. Lives in C++ because the old
// Lua version crossed the binding boundary once per probed texel, per leg, per
// frame.
//
// Candidacy is not a caller-supplied list: a body qualifies when it has terrain
// or a collisionShape AND raycastTarget set, so feet agree with what the physics
// resolver would actually stand them on. Trigger volumes clear raycastTarget.
//
// Rotation and scale are ignored, matching the collision path.
namespace Physics {

struct GroundHit {
    bool hit = false;
    float y = 0.0f;                     // world Y of the surface
    const RigidBody2D* body = nullptr;  // what was hit; null when !hit
};

// Highest solid surface at world `x`, searched downward from `fromY` to
// `maxY`. `ignore` is skipped entirely (pass the querying actor's own
// body so a character can't stand on itself).
GroundHit RaycastDown(const ActorRegistry& actors,
                      float x, float fromY, float maxY,
                      const RigidBody2D* ignore = nullptr);

} // namespace Physics