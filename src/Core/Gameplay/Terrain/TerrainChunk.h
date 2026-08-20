#pragma once
#include <vector>
#include <cstdint>
#include "../../Math/Color.h"
#include "../../Math/Vector2.h"

class PixelSprite;
class RigidBody2D;

// A procedurally generated patch of ground -- noise-driven uneven dirt
// with per-pixel grass growing out of its surface. Attached to a
// RigidBody2D via RigidBody2D::terrain, exactly the way
// LightEmitterConfig/PlayerActorConfig/Camera2D already are: a non-owning
// pointer on the body, the instance owned by ActorRegistry, created from
// Lua via TerrainChunk.new(). The owning body's transform.position is the
// chunk's world-space CENTER and its `size` the chunk's texel dimensions,
// same "the RigidBody2D is the actor" convention everything else here
// uses -- which is what lets terrain be placed anywhere, and lets several
// independent chunks coexist without any of them being "the floor".
//
// Ticked once a frame by TerrainSystem (see TerrainSystem.h), same
// engine-wide-subsystem shape LightingSystem already has.
//
// =====================================================================
// WHY THE PIXELS LIVE IN A PixelSprite
// =====================================================================
// Everything this class draws goes through PixelSprite::SetPixel on the
// sprite attached to the owning body -- it does NOT own a texture, a
// shader, or a draw call of its own. That's deliberate and it's what
// makes terrain a first-class citizen of the systems already here rather
// than a parallel special case:
//   - LightingSystem tints solid sprite pixels, so terrain and grass are
//     lit by the campfire/spotlight for free, per pixel, with zero
//     terrain-specific code in the lighting pass.
//   - PixelSprite::PunchCircle already exists, so the Noita-style
//     destruction goal applies to terrain the moment you want it (the
//     heightmap below is what would need re-deriving afterward -- see
//     the note on m_SurfaceY).
//   - The whole thing draws as ONE textured quad through the existing
//     DrawBody path. 300 blades of grass cost one draw call, not 300.
//
// The consequence to be aware of: the sprite must be tall enough to hold
// both the dirt AND the grass standing above it. See surfaceOffset.
//
// =====================================================================
// COLLISION: HEIGHTFIELD, NOT AABB
// =====================================================================
// Uneven ground can't be an AABB, and CollisionShape2D is axis-aligned
// boxes and circles by construction. Rather than bolt a third shape type
// onto CollisionShape2D that would need the terrain's pixel data anyway,
// terrain resolves collision itself: ResolveBody() samples the generated
// heightmap under the moving body and pushes it out, then hands the
// resulting correction to RigidBody2D::ApplyCollisionCorrection so the
// velocity-zeroing and grounded-flag conventions stay identical to the
// AABB path. A terrain body therefore has NO collisionShape attached --
// leaving one there would give it a flat box top that fights the real
// surface, and CollidesWith/ResolveCollisionWith against it correctly
// no-op (both bail when either side has no shape).
//
// This is a heightmap, not a per-pixel mask: one surface height per
// column, solid all the way down. Overhangs and caves are out of scope
// for it -- when destruction lands and holes can appear mid-column, this
// is the piece that has to become a real pixel-mask query.
class TerrainChunk {
public:
    // Hard cap on grassMaxHeight below -- each blade stores its
    // last-drawn lateral offsets inline (see GrassBlade::lastOffset) so
    // the per-frame erase touches exactly the pixels the last frame
    // actually wrote, instead of scrubbing a whole band of the sprite.
    static constexpr int kMaxBladeHeight = 8;

    // =================================================================
    // Generation config -- plain public fields, same convention as
    // LightEmitterConfig. Set these, THEN call Generate(); changing one
    // afterward does nothing until the next Generate() call.
    // =================================================================

    // Threaded into every Noise:: call below. Same seed + same sprite
    // size always rebuilds a byte-identical chunk, so a script hot-reload
    // brings the same world back rather than a new one.
    int seed = 1337;

    // Surface shape. `surfaceFrequency` is noise cycles per texel: at
    // 0.03 the broadest hump spans ~33 texels, roughly two player-widths,
    // which reads as gentle rolling ground at 320x180. Crank it toward
    // 0.1 for jagged, noisy ground.
    //
    // Note that FBM's normalized output clusters toward its mean (see
    // Noise::FBM1D01's range comment), so the surface only uses roughly
    // half of surfaceAmplitude in practice -- pick this by looking at
    // the result, not by assuming a full +/- swing.
    float surfaceFrequency = 0.022f;
    float surfaceAmplitude = 5.0f; // +/- texels around the mean surface
    int surfaceOctaves = 3;
    float surfaceLacunarity = 2.0f;
    float surfaceGain = 0.5f;

    // Texels from the TOP EDGE of the sprite down to the MEAN surface
    // height. This is the headroom the grass stands in, so it has to
    // clear the worst case or blades get clipped off by the sprite's own
    // top edge: surfaceAmplitude + grassMaxHeight + 1 is the minimum.
    // Generate() clamps it up to that minimum and warns rather than
    // silently producing decapitated grass.
    float surfaceOffset = 12.0f;

    // Dirt body. Two tones blended by 2D FBM and then posterized into
    // `dirtToneSteps` levels (see Noise::Quantize01's comment for why a
    // smooth gradient is wrong here). rockChance scatters uncorrelated
    // white-noise specks of rockColor through the fill.
    Color dirtDark = Color(0.24f, 0.15f, 0.10f, 1.0f);
    Color dirtLight = Color(0.42f, 0.29f, 0.18f, 1.0f);
    Color rockColor = Color(0.33f, 0.31f, 0.31f, 1.0f);
    float dirtFrequency = 0.16f;
    int dirtOctaves = 3;
    int dirtToneSteps = 5;
    float rockChance = 0.035f;

    // How much darker the dirt gets at the bottom of the chunk than at
    // the surface, 0 = flat lighting. Purely an authored base-color
    // gradient, completely independent of LightingSystem -- it's there so
    // an UNLIT chunk still reads as having depth.
    float depthDarkening = 0.45f;

    // A band of grass-colored soil immediately under the surface, so the
    // blades appear to grow out of the ground instead of balancing on top
    // of a hard dirt/green seam.
    Color topsoilColor = Color(0.16f, 0.30f, 0.13f, 1.0f);
    int topsoilDepth = 2; // texels; 0 disables

    // =================================================================
    // Grass config
    // =================================================================

    // Blades are drawn one texel wide, grassMinHeight..grassMaxHeight
    // texels tall, at most one per column, with `grassDensity` as the
    // per-column probability that a column gets one at all. Density below
    // 1 is what stops the surface reading as a solid green stripe.
    Color grassDark = Color(0.15f, 0.34f, 0.13f, 1.0f);
    Color grassLight = Color(0.40f, 0.66f, 0.24f, 1.0f);
    int grassMinHeight = 3;
    int grassMaxHeight = 5;
    float grassDensity = 0.82f;

    // Idle sway. Each blade's TIP is sprung toward a moving target of
    // swayAmplitude * sin(time * swaySpeed + column * swayPhasePerTexel);
    // the per-column phase term is what makes the wind visibly travel
    // along the ground as a wave instead of every blade leaning in
    // lockstep. Amplitude is in texels of tip displacement -- keep it
    // near 1-2 at this scale, since a blade only has 3-5 pixels to bend
    // across and anything larger just snaps between two columns.
    float swayAmplitude = 1.4f;
    float swaySpeed = 1.5f;
    float swayPhasePerTexel = 0.11f;

    // The spring that carries each blade toward that target, and back
    // upright after something shoves it. stiffness sets how fast it
    // recovers (natural frequency ~= sqrt(stiffness) rad/s), damping how
    // much it overshoots -- at 90/9 a blade snaps back with one small
    // visible wobble, which is the springy-grass read. Drop damping
    // toward 3 for a much bouncier, more cartoonish recovery.
    float bendStiffness = 90.0f;
    float bendDamping = 9.0f;
    float maxBend = 3.0f; // texels; hard clamp so a fast pass can't fold a blade across the map

    // Character interaction. Any body TerrainSystem hands to Update()
    // pushes the blades its own AABB overlaps (padded by disturbPadding
    // texels, so grass reacts just before contact rather than only once
    // it's visually inside the character). The push direction is the
    // body's movement direction when it's actually moving, and
    // away-from-its-center when it's standing still -- so walking through
    // sweeps the grass along with you, and standing in it splays it out
    // to both sides.
    float disturbStrength = 110.0f;
    float disturbPadding = 2.0f;
    float disturbSpeedScale = 0.010f; // extra push per unit of |velocity.x|

    // =================================================================
    // Lifecycle
    // =================================================================

    // Builds the heightmap, paints every dirt pixel into `sprite`, and
    // seeds the blade list. Sizes itself entirely from the sprite, so the
    // owning body's `size` (which SetSprite already derives from the
    // sprite) and the generated content can never disagree. Safe to call
    // again to regenerate with different config -- it rewrites every
    // pixel, including clearing the ones above the surface back to
    // transparent.
    void Generate(PixelSprite& sprite);

    // One frame of grass: apply each disturber's push, integrate every
    // blade's spring, erase last frame's blade pixels and draw this
    // frame's. Only touches pixels ABOVE the surface, so it can never
    // damage the dirt (or a hole punched in it).
    //
    // Call this BEFORE LightingSystem::Update for the frame -- SetPixel
    // writes the authored base color into both of PixelSprite's buffers,
    // so grass moved after the lighting pass would show up unlit for one
    // frame.
    void Update(PixelSprite& sprite, const RigidBody2D& terrainBody,
                const std::vector<const RigidBody2D*>& disturbers, float deltaTime);

    // World-space Y of the terrain surface directly under `worldX` (the
    // top of the dirt, not the top of the grass -- grass is decoration,
    // nothing stands on it). Clamps to the nearest in-range column for an
    // X outside the chunk, so a caller placing props doesn't have to
    // range-check first.
    //
    // Deliberately NOT interpolated between columns: the heightmap is
    // snapped to whole texels at generation (see m_SurfaceY), and this is
    // the same number ResolveBody stands bodies on. A prop placed with
    // this sits exactly on the pixel the player will walk on, which is
    // the whole point of asking.
    float SurfaceWorldY(float worldX, const RigidBody2D& terrainBody) const;

    // Pushes `body` out of the terrain, if it's in it. Returns true if a
    // correction was applied. See the class comment for why this lives
    // here instead of on CollisionShape2D.
    //
    // Vertical resolution wins whenever the deepest column under the body
    // is within `maxStepHeight` of the body's feet -- that's what makes
    // slopes walkable instead of something you collide with sideways.
    // Past that it resolves HORIZONTALLY instead, so a steep enough rise
    // acts as a wall rather than teleporting the body up a cliff face.
    bool ResolveBody(RigidBody2D& body, const RigidBody2D& terrainBody) const;

    // Largest upward step (texels) the body is allowed to climb in one
    // resolution. Roughly "knee height" -- 6 against a 32-tall player.
    float maxStepHeight = 6.0f;

    bool IsGenerated() const { return m_Width > 0 && m_Height > 0; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    int GetBladeCount() const { return static_cast<int>(m_Blades.size()); }

private:
    // One tuft. `column`/`rootY` are LOCAL sprite pixel coordinates
    // ((0,0) top-left, +y down -- same convention PixelSprite and
    // LightingSystem's WorldToPixel already use).
    struct GrassBlade {
        int column = 0;
        int rootY = 0;   // first AIR pixel above the surface; the blade grows upward from here
        int height = 3;
        float phase = 0.0f;
        float tint = 0.0f;    // 0..1 lerp between grassDark and grassLight
        float bend = 0.0f;    // current tip displacement, texels (signed, +x = right)
        float bendVel = 0.0f;

        // Lateral offset actually written for each segment last frame, so
        // the erase pass can un-write exactly those pixels. Cheaper and
        // (more importantly) SAFER than clearing a fixed band: a band wide
        // enough to cover every possible bend would also have to be tall
        // enough to cover the surface's own unevenness, and would then be
        // scrubbing pixels that belong to a NEIGHBORING column's dirt.
        int8_t lastOffset[kMaxBladeHeight] = {};
        bool drawn = false;
    };

    void ApplyDisturber(const RigidBody2D& disturber, const RigidBody2D& terrainBody, float deltaTime);
    void EraseBlades(PixelSprite& sprite);
    void DrawBlades(PixelSprite& sprite);

    // Continuous local pixel X <-> world X. `u` is measured from the
    // sprite's LEFT EDGE: column i spans u = [i, i+1), so u = i is that
    // column's left edge and u = i + 0.5 its center (see the .cpp).
    // Rotation is deliberately ignored (a rotated heightfield stops being
    // a heightfield); scale is honored, matching how the sprite actually
    // draws.
    float ColumnToWorldX(float u, const RigidBody2D& terrainBody) const;
    float WorldToColumn(float worldX, const RigidBody2D& terrainBody) const;

    int m_Width = 0;
    int m_Height = 0;

    // Local Y of the topmost SOLID (dirt) texel in each column, snapped
    // to a whole texel at generation. This is generated data, not config
    // -- and it's the piece that goes stale the moment destruction lets a
    // hole appear in a column, which is exactly why it's private with no
    // setter.
    std::vector<float> m_SurfaceY;

    std::vector<GrassBlade> m_Blades;

    float m_Time = 0.0f; // accumulated per chunk, so two chunks don't sway in lockstep
};