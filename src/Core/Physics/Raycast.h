#pragma once

class ActorRegistry;
class RigidBody2D;

// Downward ground queries against the live actor set. This is the engine
// half of what objects/leg_rig.lua used to do itself in Lua: walk every
// candidate body, reject by AABB, then refine to the exact surface --
// heightmap for terrain, per-pixel column walk for a sprite-backed box.
//
// Moving it here is not just line count. The Lua version crossed the
// binding boundary once per PROBED TEXEL (PixelSprite:IsSolid down the
// column), per leg, per frame; an eight-legged actor on deep terrain was
// paying hundreds of lua_call round-trips for a query that is a few
// hundred nanoseconds of straight-line C++.
//
// WHAT COUNTS AS GROUND is deliberately NOT a caller-supplied list
// anymore. A body is a candidate when it has terrain or a
// collisionShape attached AND RigidBody2D::raycastTarget is set. That
// makes the answer agree with what the physics resolver would actually
// stand you on, instead of relying on a level script keeping its
// `solids` table in sync with reality -- and it means feet now snap onto
// crates and props for free. Anything that should be invisible to feet
// (a trigger volume, a decorative collider) clears raycastTarget.
//
// Rotation is ignored, matching CollisionShape2D's own axis-aligned
// simplification and TerrainChunk's (a rotated heightfield stops being a
// heightfield). Scale is ignored too: sizes come from RigidBody2D::size,
// which is the same number the collision path uses.
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