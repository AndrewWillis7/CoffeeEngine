#pragma once
#include <vector>

#include "../Math/Vector2.h"

class ActorRegistry;
class RigidBody2D;
class LightEmitterConfig;
class PixelSprite;

// Dynamic, per-pixel, non-baked 2D lighting. An engine-wide subsystem like
// Renderer2D -- owned in main.cpp, ticked once a frame from the bare Lua global
// UpdateLighting(deltaTime), not something Lua creates per actor.
//
// Each Update(), for every body carrying a LightEmitterConfig:
//   1. Broad-phase by AABB distance to find sprite-backed bodies in reach.
//   2. Walk each candidate's own solid pixels, and for the ones inside the
//      radius (and the wedge, for a Cone) raymarch from the light to that pixel.
//      A lightBlocking body's solid pixel on that line shadows it; otherwise it
//      is tinted via PixelSprite::AccumulateLightTint with distance and angle
//      falloff, optionally quantized into bands (LightEmitterConfig::toneSteps).
//
// Driving this from candidate PIXELS rather than a sweep of angular rays is
// deliberate. At fixed angular resolution the world-space gap between adjacent
// rays grows with distance, so a small object far from a wide light gets grazed
// by one or two rays and reads as speckled diagonal streaks instead of a filled
// beam. Evaluating pixels directly also gives an exactly circular radius cutoff
// rather than a polygon approximated by ray count, which is what makes
// toneSteps' "3-tone light in a perfect circle" actually look like a circle.
//
// Nothing is baked or cached between frames. The only carry-over is, per
// (light, body) pair, the pixel rect touched LAST frame -- purely so a light
// that moved erases its own stale tint instead of leaving a ghost.
//
// Performance: the light scan and candidate scan are still O(n) over every
// body, since the engine has no broad-phase index yet. Bounded, though:
//   - the pixel walk is clamped to the rect the light's circle can reach on
//     that candidate, not the whole sprite;
//   - the occlusion march slab-clips against each blocker's world AABB before
//     stepping, so a clear line of sight costs one AABB test per blocker;
//   - world/pixel transforms are hoisted per body and skip trig when unrotated.
// Together these took Update from ~12.4ms to ~1.3ms/frame on the shipped scene.
// A real spatial hash is the next step once actor counts grow.
class LightingSystem {
public:
    void Update(ActorRegistry& actors, float deltaTime);

    // Drops last frame's lit-rect bookkeeping WITHOUT erasing through it. Must
    // be called whenever the bodies it points at are about to be destroyed --
    // concretely ScriptEngine::Reload() before ActorRegistry::Clear() -- or the
    // next Update()'s erase pass dereferences freed RigidBody2D. The only cost
    // is one frame of stale tint on sprites that are being thrown away anyway.
    void Reset() { m_PrevLitRects.clear(); }

private:
    // A pixel rect one light touched on one body, rebuilt every Update() so the
    // next frame's erase pass knows what to clear even if the light moved,
    // changed shape, or vanished.
    struct LitRect {
        RigidBody2D* body = nullptr;
        int minX = 0, minY = 0, maxX = 0, maxY = 0;
    };

    struct LightSample {
        RigidBody2D* body = nullptr;
        LightEmitterConfig* config = nullptr;
    };

    // A lightBlocking body plus everything the occlusion march needs, resolved
    // once per frame instead of per sample: the world AABB to slab-clip against,
    // and the world->pixel transform WorldToPixel would otherwise re-derive.
    struct Blocker {
        RigidBody2D* body;
        const PixelSprite* sprite;
        Vector2 boxMin, boxMax;
        Vector2 pos;
        float cosNeg, sinNeg;   // cos/sin of -rotation
        float invScaleX, invScaleY;
        float halfW, halfH;
        int width, height;
        bool rotated;
    };

    // Out-of-line, but a member so it can see Blocker.
    static bool IsOccluded(const Vector2& from, const Vector2& to,
                           const RigidBody2D* skipBody, const std::vector<Blocker>& blockers);

    std::vector<LitRect> m_PrevLitRects;

    // Kept as members purely to reuse their capacity across frames.
    std::vector<LitRect> m_LitRectScratch;
    std::vector<LightSample> m_Lights;
    std::vector<Blocker> m_Blockers;
    std::vector<RigidBody2D*> m_Candidates;

    float m_Time = 0.0f; // accumulated for per-light flicker sampling
};
