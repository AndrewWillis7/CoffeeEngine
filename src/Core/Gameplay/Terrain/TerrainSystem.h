#pragma once
#include <vector>

class ActorRegistry;
class RigidBody2D;

// Ticks every TerrainChunk in the scene once a frame. Engine-wide
// subsystem, not a Lua-creatable per-actor thing -- same category (and
// same wiring) as LightingSystem: owned in main.cpp, handed to
// ScriptBindings through EngineContext, driven from Lua by the bare
// global UpdateTerrain(deltaTime).
//
// Its whole job is pairing up the two halves of the grass simulation that
// a TerrainChunk can't see on its own:
//   1. Which bodies count as DISTURBERS this frame -- the things whose
//      movement parts the grass. A chunk holds no references to other
//      actors (that would be a dangling pointer waiting for the next
//      ActorRegistry::Clear()), so the list is rebuilt fresh here every
//      frame from whatever's currently alive.
//   2. Which sprite each chunk should paint into -- the chunk stores no
//      PixelSprite*, for the same lifetime reason; it gets handed its
//      owning body's sprite at call time.
//
// CALL ORDER MATTERS: this must run BEFORE UpdateLighting() for the
// frame. Grass is written with PixelSprite::SetPixel, which writes the
// authored base color into both the base and lit buffers -- grass moved
// after the lighting pass would draw unlit for one frame and strobe.
//
// PERFORMANCE NOTE (flagged, not hidden): the two scans below are O(n)
// over every RigidBody2D, same convention ActorRegistry::GetPlayerActor()
// and LightingSystem::Update already use, and for the same reason --
// there's still no broad-phase anywhere in the engine. The per-chunk work
// itself is bounded by blade count, not sprite area: a chunk only ever
// touches the handful of pixels its blades wrote last frame plus the ones
// they write this frame (see TerrainChunk::EraseBlades), so a 320-wide
// chunk costs roughly 2,700 SetPixel calls a frame regardless of how deep
// the dirt goes.
class TerrainSystem {
public:
    void Update(ActorRegistry& actors, float deltaTime);

private:
    // Rebuilt every Update() call, kept as a member purely to reuse the
    // allocation -- never read across frames, so unlike
    // LightingSystem::m_PrevLitRects there's nothing here that can dangle
    // across an ActorRegistry::Clear() and therefore no Reset() to
    // remember to call on hot-reload.
    std::vector<const RigidBody2D*> m_Disturbers;
};