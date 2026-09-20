#pragma once
#include <vector>
#include <cstdint>
#include "../../Math/Color.h"
#include "../../Math/Vector2.h"

class PixelSprite;
class RigidBody2D;

// A procedurally generated patch of ground: noise-driven uneven dirt with
// per-pixel grass growing out of it. Attached via RigidBody2D::terrain like
// every other capability tag, owned by ActorRegistry, ticked once a frame by
// TerrainSystem. The owning body's position is the chunk's world centre and its
// `size` the texel dimensions, so chunks can be placed anywhere and several can
// coexist without any one of them being "the floor".
//
// Why the pixels live in a PixelSprite: everything here draws through
// SetPixel on the owning body's sprite, owning no texture, shader or draw call
// of its own. That makes terrain a first-class citizen of the existing systems
// rather than a parallel special case -- LightingSystem tints solid sprite
// pixels, so dirt and grass are lit for free with no terrain-specific code;
// PunchCircle already works on it; and 300 blades of grass cost one draw call.
// The consequence to watch is that the sprite must be tall enough to hold the
// dirt AND the grass standing above it -- see surfaceOffset.
//
// Collision is a heightfield, not an AABB. Uneven ground can't be a box, and
// CollisionShape2D is axis-aligned by construction, so terrain resolves itself:
// ResolveBody() samples the heightmap and hands the correction to
// RigidBody2D::ApplyCollisionCorrection, keeping the velocity and grounded
// conventions identical to the AABB path. A terrain body therefore has no
// collisionShape -- one would give it a flat lid fighting the real surface.
//
// One surface height per column, solid all the way down: overhangs and caves
// are out of scope. When destruction lets holes appear mid-column, this is the
// piece that has to become a real pixel-mask query.
class TerrainChunk {
public:
    // Hard cap on grassMaxHeight: each blade stores its last-drawn lateral
    // offsets inline, so the per-frame erase touches exactly the pixels the last
    // frame wrote instead of scrubbing a whole band.
    static constexpr int kMaxBladeHeight = 8;

    // --- Generation config. Set these, THEN call Generate(). ----------------

    // Same seed and sprite size always rebuild a byte-identical chunk, so a
    // hot-reload brings the same world back rather than a new one.
    int seed = 1337;

    // surfaceFrequency is noise cycles per texel: at 0.03 the broadest hump
    // spans ~33 texels, which reads as gentle rolling ground at 320x180. Toward
    // 0.1 gets jagged. FBM's normalized output clusters toward its mean, so only
    // about half of surfaceAmplitude is used in practice -- tune by eye.
    float surfaceFrequency = 0.022f;
    float surfaceAmplitude = 5.0f; // +/- texels around the mean surface
    int surfaceOctaves = 3;
    float surfaceLacunarity = 2.0f;
    float surfaceGain = 0.5f;

    // Texels from the sprite's TOP EDGE down to the MEAN surface -- the headroom
    // the grass stands in, so it must clear the worst case or blades get clipped
    // by the sprite's own edge. Generate() raises it to the minimum and warns
    // rather than silently producing decapitated grass.
    float surfaceOffset = 12.0f;

    // Dirt body: two tones blended by 2D FBM, posterized into dirtToneSteps
    // levels. rockChance scatters white-noise specks of rockColor through it.
    Color dirtDark = Color(0.24f, 0.15f, 0.10f, 1.0f);
    Color dirtLight = Color(0.42f, 0.29f, 0.18f, 1.0f);
    Color rockColor = Color(0.33f, 0.31f, 0.31f, 1.0f);
    float dirtFrequency = 0.16f;
    int dirtOctaves = 3;
    int dirtToneSteps = 5;
    float rockChance = 0.035f;

    // How much darker the dirt gets toward the bottom. Purely an authored base
    // gradient, independent of LightingSystem, so an UNLIT chunk still has depth.
    float depthDarkening = 0.45f;

    // A band of grass-colored soil under the surface, so blades appear to grow
    // out of the ground rather than balance on a hard dirt/green seam.
    Color topsoilColor = Color(0.16f, 0.30f, 0.13f, 1.0f);
    int topsoilDepth = 2; // texels; 0 disables

    // --- Grass config -------------------------------------------------------

    // Blades are one texel wide and at most one per column; grassDensity is the
    // per-column probability of getting one, which is what stops the surface
    // reading as a solid green stripe.
    Color grassDark = Color(0.15f, 0.34f, 0.13f, 1.0f);
    Color grassLight = Color(0.40f, 0.66f, 0.24f, 1.0f);
    int grassMinHeight = 3;
    int grassMaxHeight = 5;
    float grassDensity = 0.82f;

    // Idle sway. Each tip springs toward swayAmplitude * sin(time * swaySpeed +
    // column * swayPhasePerTexel); the per-column phase is what makes the wind
    // travel along the ground as a wave instead of every blade leaning in
    // lockstep. Amplitude is tip displacement in texels -- keep it near 1-2,
    // since a blade only has 3-5 pixels to bend across.
    float swayAmplitude = 1.4f;
    float swaySpeed = 1.5f;
    float swayPhasePerTexel = 0.11f;

    // The spring carrying each blade to that target and back upright after a
    // shove. stiffness sets recovery speed (natural frequency ~ sqrt(stiffness)),
    // damping how much it overshoots; 90/9 gives one small visible wobble.
    float bendStiffness = 90.0f;
    float bendDamping = 9.0f;
    float maxBend = 3.0f; // texels; hard clamp so a fast pass can't fold a blade across the map

    // Character interaction. Any body TerrainSystem passes in pushes the blades
    // its AABB overlaps, padded so grass reacts just before contact. The push
    // follows the body's movement direction when moving and points away from its
    // centre when still -- so walking sweeps the grass along and standing splays
    // it to both sides.
    float disturbStrength = 110.0f;
    float disturbPadding = 2.0f;
    float disturbSpeedScale = 0.010f; // extra push per unit of |velocity.x|

    // --- Lifecycle ----------------------------------------------------------

    // Builds the heightmap, paints the dirt into `sprite`, and seeds the blades.
    // Sizes itself entirely from the sprite, so the owning body's `size` and the
    // generated content can never disagree. Safe to call again: it rewrites every
    // pixel, including clearing the ones above the surface back to transparent.
    void Generate(PixelSprite& sprite);

    // One frame of grass: apply each disturber's push, integrate the springs,
    // erase last frame's blade pixels and draw this frame's. Only touches pixels
    // ABOVE the surface, so it can never damage the dirt or a hole punched in it.
    //
    // Call this BEFORE LightingSystem::Update. SetPixel writes the authored base
    // into both of PixelSprite's buffers, so grass moved after the lighting pass
    // would draw unlit for one frame.
    void Update(PixelSprite& sprite, const RigidBody2D& terrainBody,
                const std::vector<const RigidBody2D*>& disturbers, float deltaTime);

    // World Y of the surface under `worldX` -- the top of the dirt, not the top
    // of the grass, since nothing stands on grass. Clamps to the nearest in-range
    // column so callers placing props don't have to range-check.
    //
    // Deliberately not interpolated between columns: the heightmap is snapped to
    // whole texels at generation, and this is the same number ResolveBody stands
    // bodies on, so a prop sits exactly on the pixel the player will walk on.
    float SurfaceWorldY(float worldX, const RigidBody2D& terrainBody) const;

    // Pushes `body` out of the terrain if it is in it; true if it corrected.
    // Vertical resolution wins while the deepest column under the body is within
    // maxStepHeight of its feet, which is what makes slopes walkable. Past that
    // it resolves HORIZONTALLY, so a steep rise acts as a wall instead of
    // teleporting the body up a cliff face.
    bool ResolveBody(RigidBody2D& body, const RigidBody2D& terrainBody) const;

    // Largest upward step (texels) climbable in one resolution -- roughly knee
    // height, 6 against a 32-tall player.
    float maxStepHeight = 6.0f;

    bool IsGenerated() const { return m_Width > 0 && m_Height > 0; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    int GetBladeCount() const { return static_cast<int>(m_Blades.size()); }

private:
    // One tuft. column/rootY are LOCAL sprite pixel coordinates, (0,0) top-left.
    struct GrassBlade {
        int column = 0;
        int rootY = 0;   // first AIR pixel above the surface; the blade grows up from here
        int height = 3;
        float phase = 0.0f;
        float tint = 0.0f;    // 0..1 lerp between grassDark and grassLight
        float bend = 0.0f;    // current tip displacement in texels, +x = right
        float bendVel = 0.0f;

        // Lateral offset actually written per segment last frame, so the erase
        // can un-write exactly those pixels. Safer than clearing a fixed band: a
        // band wide enough for every possible bend would also have to cover the
        // surface's unevenness, and would scrub a neighbouring column's dirt.
        int8_t lastOffset[kMaxBladeHeight] = {};
        bool drawn = false;
    };

    void ApplyDisturber(const RigidBody2D& disturber, const RigidBody2D& terrainBody, float deltaTime);
    void EraseBlades(PixelSprite& sprite);
    void DrawBlades(PixelSprite& sprite);

    // Continuous local pixel X <-> world X. `u` is measured from the sprite's
    // LEFT EDGE: column i spans [i, i+1), so u = i is its left edge and i + 0.5
    // its centre. Rotation is ignored (a rotated heightfield stops being one);
    // scale is honoured, so collision stays glued to where the sprite draws.
    float ColumnToWorldX(float u, const RigidBody2D& terrainBody) const;
    float WorldToColumn(float worldX, const RigidBody2D& terrainBody) const;

    int m_Width = 0;
    int m_Height = 0;

    // Local Y of the topmost solid texel per column, snapped to a whole texel at
    // generation. Generated data, not config -- and the piece that goes stale
    // the moment destruction opens a hole in a column, hence private, no setter.
    std::vector<float> m_SurfaceY;

    std::vector<GrassBlade> m_Blades;

    float m_Time = 0.0f; // per chunk, so two chunks don't sway in lockstep
};
