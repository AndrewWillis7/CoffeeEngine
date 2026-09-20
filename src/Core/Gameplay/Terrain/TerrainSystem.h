#pragma once
#include <vector>

class ActorRegistry;
class RigidBody2D;

// Ticks every TerrainChunk once a frame. Engine-wide subsystem, wired exactly
// like LightingSystem: owned in main.cpp, reached through EngineContext, driven
// from Lua by the bare global UpdateTerrain(deltaTime).
//
// Its job is pairing up the two halves of the grass simulation a chunk can't see
// on its own -- which bodies count as disturbers this frame, and which sprite
// each chunk paints into. A chunk deliberately holds neither reference itself,
// since both would dangle at the next ActorRegistry::Clear().
//
// Call order matters: this must run BEFORE UpdateLighting(). Grass is written
// with SetPixel, which writes the base color into both buffers, so grass moved
// after the lighting pass draws unlit for one frame and strobes.
//
// The two scans are O(n) over every body, for want of a broad-phase index. The
// per-chunk work is bounded by blade count rather than sprite area: a chunk only
// touches the pixels its blades wrote last frame plus this frame's, so a
// 320-wide chunk costs ~2,700 SetPixel calls regardless of dirt depth.
class TerrainSystem {
public:
    // What the last Update() covered, for the debug panel. All of it falls out
    // of the two passes below, so collecting it costs nothing.
    struct Stats {
        int chunks = 0;
        int disturbers = 0;
        int blades = 0;
        float milliseconds = 0.0f;
    };

    void Update(ActorRegistry& actors, float deltaTime);

    const Stats& GetStats() const { return m_Stats; }

private:
    Stats m_Stats;

    // Rebuilt every Update(), a member purely to reuse the allocation. Never read
    // across frames, so unlike LightingSystem::m_PrevLitRects nothing here can
    // dangle over an ActorRegistry::Clear() -- hence no Reset() to remember.
    std::vector<const RigidBody2D*> m_Disturbers;
};