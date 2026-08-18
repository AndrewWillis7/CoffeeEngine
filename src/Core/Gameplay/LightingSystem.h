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
// Every Update() call, for each active light (a RigidBody2D with a
// LightEmitterConfig attached, found the same way playerConfig/camera/
// collisionShape already are):
//   1. Broad-phase: which sprite-backed bodies are even within
//      cfg->radius of this light's position, same coarse AABB-distance
//      filter as before.
//   2. For each candidate body, walk EVERY one of its own solid pixels
//      (not a sweep of angular rays -- see the note below) and, for each
//      one within the light's radius (and, for a Cone light, within its
//      angular wedge), do a short occlusion raymarch from the light
//      straight to that pixel: if a lightBlocking body's solid pixel
//      sits anywhere on that line first, this pixel is shadowed and
//      skipped; otherwise it's tinted via PixelSprite::AccumulateLightTint
//      with distance/angle falloff (optionally quantized into discrete
//      bands -- see LightEmitterConfig::toneSteps).
//
// This used to be done the other way around -- sweep a fixed number of
// angular rays outward from the light and see what they happened to hit
// -- which is cheap but has a real flaw: at a fixed angular resolution,
// the WORLD-SPACE gap between adjacent rays grows with distance, so a
// small object far from a wide-radius light (a spotlight lighting the
// player from clear across a level, say) only gets grazed by one or two
// rays and shows up as speckled diagonal streaks instead of a filled-in
// beam. Evaluating every candidate PIXEL directly instead of hoping a
// ray lands on it fixes that at the source, and as a side effect gives
// an exactly circular (not polygon-approximated-by-ray-count) radius
// cutoff for free -- which is what makes LightEmitterConfig::toneSteps'
// "3-tone light in a perfect circle" stylization actually look like a
// circle instead of a facsimile of one.
//
// Nothing here is baked or cached across frames -- move the light (or
// whatever it's shining on) and the very next Update() call reflects it,
// per the "walk up to the campfire" requirement. The only per-frame
// memory is, per (light, body) pair, the pixel rect it touched LAST
// frame (m_PrevLitRects below) -- purely so a light that moved away (or
// was removed/unset) cleanly erases its own stale tint instead of
// leaving a ghost behind; it plays no part in the actual lighting math.
//
// PERFORMANCE NOTE (flagged, not hidden): there's still no broad-phase/
// spatial index anywhere in the engine (see the "on the horizon" list),
// so both the light scan and the per-light candidate-body scan below
// are O(n) over every body, same convention ActorRegistry::
// GetPlayerActor() already uses. Per candidate, this now walks every
// pixel of its sprite (O(width*height)) rather than being bounded by a
// fixed ray/step budget -- fine for the handful-of-small-sprites scale
// this engine is at today (worst case here is the 100x15 floor, 1500
// pixels), but a much bigger sprite (a full Noita-style terrain chunk)
// would want its candidate loop tightened to just the pixel rect the
// light's world-space circle actually overlaps, rather than the whole
// sprite -- straightforward to add (intersect the light's world AABB
// against the candidate's, convert corners through WorldToPixel) but
// skipped for now since nothing in this engine is that large yet.
class LightingSystem {
public:
    void Update(ActorRegistry& actors, float deltaTime);

private:
    // One light-frame's worth of "which body, which pixel rect" a single
    // light touched -- rebuilt fresh each Update() call from what THIS
    // frame's evaluation actually lit, then swapped into m_PrevLitRects
    // at the end so next frame's reset pass knows what to clear even if
    // the light moved, changed shape, or vanished entirely in between.
    struct LitRect {
        RigidBody2D* body = nullptr;
        int minX = 0, minY = 0, maxX = 0, maxY = 0;
    };

    std::vector<LitRect> m_PrevLitRects;

    float m_Time = 0.0f; // accumulated for per-light flicker sampling
};