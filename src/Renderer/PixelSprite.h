#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Texture.h"
#include "Core/Math/Color.h"

class PixelSprite {
public:
    // Loads a PNG off disk. Filter defaults to Nearest (not Texture's usual
    // Linear default) -- same reasoning as Font's atlas: torn/punched edges
    // should stay crisp, not bleed into neighboring pixels.
    explicit PixelSprite(const std::string& filepath, Texture::Filter filter = Texture::Filter::Nearest);

    // Builds a blank, in-memory sprite filled solid with `fill` -- no PNG
    // involved. This is the "split a basic flat-color square into
    // individual pixels" primitive: ActorRegistry::CreateSolidSprite (see
    // its header comment) calls this once per body so every plain
    // RigidBody2D quad becomes something SetPixel/PunchCircle/lighting can
    // actually address a pixel at a time, the same way a loaded PNG
    // already could.
    PixelSprite(int width, int height, const Color& fill, Texture::Filter filter = Texture::Filter::Nearest);

    PixelSprite(const PixelSprite&) = delete;
    PixelSprite& operator=(const PixelSprite&) = delete;

    bool IsValid() const { return m_Texture && m_Texture->IsValid(); }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    // (0, 0) is top-left, matching the loading convention documented in
    // Texture.cpp. Out-of-bounds calls are silently ignored/return
    // transparent-black rather than asserting -- gameplay code (a punch
    // radius near an edge, a collision probe just off the sprite) hits
    // this constantly and shouldn't have to bounds-check first.
    void SetPixel(int x, int y, const Color& color);
    Color GetPixel(int x, int y) const;
    bool IsSolid(int x, int y) const; // alpha > 0

        // =====================================================================
    // Raster primitives -- the "author a sprite from code, every frame"
    // path. SetPixel above is the general primitive, but a procedural rig
    // (see scripts/objects/leg_rig.lua) redraws a few hundred pixels EVERY
    // frame, and paying a Lua->C++ call boundary per pixel for that is
    // both slow and unreadable on the script side. These three do the
    // whole shape in one call.
    //
    // All three write the base buffer AND mirror into the lit buffer, same
    // as SetPixel, so a redraw is visible immediately even on a pixel no
    // light happens to touch this frame. All three clamp to bounds rather
    // than asserting -- a limb solved slightly outside its canvas gets
    // cropped, not crashed.
    // =====================================================================

    // Wipes the whole sprite back to transparent (RGBA all zero) and
    // zeroes the lighting accumulation with it, so the next frame's mix
    // starts clean instead of carrying weight from a pixel that used to be
    // solid here. This is the "start a fresh frame of procedural art"
    // call -- pair it with the DrawLimb/FillRect calls that rebuild the
    // pose, then let DrawBody's automatic Flush() upload the result.
    void Clear();

    // Axis-aligned filled rectangle, (x, y) = top-left, fully overwriting
    // (not blending) whatever was there. This is what draws anything that
    // must NEVER rotate to stay on the pixel grid -- a knee block, a boot,
    // a foot.
    void FillRect(int x, int y, int w, int h, const Color& color);

    // A straight limb segment from (x0, y0) to (x1, y1), `thickness`
    // texels wide, rasterized as one run per step along the segment's
    // MAJOR axis: a near-vertical limb gets one horizontal run of
    // `thickness` texels per row, a near-horizontal one gets one vertical
    // run per column.
    //
    // That major-axis scan is the whole point, and it's why this exists
    // instead of just rotating a quad: the result is gapless by
    // construction (exactly one run per row/column, no diagonal pinholes),
    // its thickness is measured along a grid axis rather than
    // perpendicular to an arbitrary angle, and every edge lands on a texel
    // boundary. A rotated quad has none of those properties -- its edges
    // fall wherever the angle puts them, so the same limb shimmers between
    // subtly different silhouettes frame to frame as the angle changes.
    //
    // Runs are centered by flooring (a run of width w at x covers
    // x - w/2 .. x - w/2 + w - 1), consistently for every thickness, so
    // segments of different widths chained end to end (thigh -> shin)
    // share a center line instead of stepping sideways at the joint.
    void DrawLimb(int x0, int y0, int x1, int y1, int thickness, const Color& color);

    // Sets alpha to 0 for every pixel within radius of (cx, cy) -- the
    // Noita-style "blow a hole in it" primitive. Leaves RGB untouched so a
    // later SetPixel/inspection still sees the original color if alpha
    // were ever restored.
    void PunchCircle(int cx, int cy, float radius);

    // =====================================================================
    // Lighting overlay -- LightingSystem-only (not exposed to Lua). Every
    // pixel this class holds actually lives in TWO color buffers: m_Pixels
    // is the authored "base" truth that SetPixel/GetPixel/IsSolid/
    // PunchCircle above all read and write, exactly as before lighting
    // existed at all. m_LitPixels mirrors it but with each frame's
    // lighting mixed in on top -- it's what Flush() actually uploads to
    // the GPU. Splitting these apart means lighting can recolor what's ON
    // SCREEN every single frame (never baked, never destructive) without
    // corrupting the pixel data any gameplay/destruction code actually
    // queries.
    //
    // A third pair of buffers -- m_LightAccumColor/m_LightAccumWeight,
    // float per pixel, NOT the GPU-facing RGBA8 the other two use -- holds
    // this frame's raw, not-yet-resolved light contributions:
    // m_LightAccumColor is a running SUM of `tint * strength` from every
    // light that's touched this pixel so far this frame, and
    // m_LightAccumWeight is the running sum of `strength` alone. Together
    // they're everything needed to turn N overlapping lights' colors into
    // one MIX rather than a wash: see AccumulateLightTint's comment below
    // for the actual blend math.
    //
    // All four are seeded/cleared equal (lit == base, accum == 0) by both
    // constructors and ResetLightingRect, so a sprite nothing ever lights
    // still displays its authored colors exactly as before -- lighting is
    // purely an overlay, never a prerequisite for correct rendering.
    // =====================================================================

    // Copies base -> lit AND zeroes the accumulation buffers for every
    // pixel in [minX,minY]..[maxX,maxY] (inclusive, clamped to bounds),
    // discarding whatever this rect mixed in last frame. LightingSystem
    // calls this once per frame, on whatever rect ITS OWN bookkeeping says
    // was lit last frame, before re-accumulating this frame's lights -- so
    // a torch that moved (or was removed) cleanly erases its own glow
    // instead of leaving a stale tinted patch (or a stale weight that
    // would bias the next mix) behind.
    void ResetLightingRect(int minX, int minY, int maxX, int maxY);

    // Folds one light's contribution into pixel (x, y)'s running mix and
    // immediately re-resolves m_LitPixels there, so the lit buffer is
    // always valid to display/upload even mid-accumulation. No-op on a
    // non-solid (fully transparent -- punched out, or simply never drawn)
    // pixel; alpha itself is never touched, so lighting can never change
    // what's solid.
    //
    // This is a genuine MIX, not an add-and-clamp: `tint * strength` is
    // folded into a running weighted-average light color (accumColor /
    // accumWeight), and the pixel's own base color is blended toward that
    // average by `saturate(accumWeight)` -- so a pixel barely grazed by
    // one weak light stays close to its base color, a pixel sitting in
    // one light's hotspot (weight >= 1) reads as that light's color
    // outright rather than blowing out toward white, and a pixel touched
    // by TWO differently-colored lights (say, a warm campfire and a cool
    // spotlight) shows a genuine blend of both -- weighted by however much
    // each one actually reached it -- instead of the two just summing
    // into a wash. Multiple lights (or multiple rays from the SAME light
    // re-touching a pixel already hit this frame) still stack naturally:
    // each call adds into the same running sum, it just no longer sums
    // past 1.0 in a way that erases hue. Call ResetLightingRect on this
    // pixel's rect first if you want a clean "starting from base" mix.
    void AccumulateLightTint(int x, int y, const Color& tint, float strength);

    // Uploads whatever's changed since the last Flush() as one
    // glTexSubImage2D over the accumulated dirty rect. Called automatically
    // by DrawBody() right before it draws a sprite-backed body, so scripts
    // don't need to remember to call this themselves -- exposed to Lua
    // mainly as an escape hatch (e.g. forcing an upload before a manual
    // Renderer2D draw call of your own).
    void Flush();

    Texture* GetTexture() { return m_Texture.get(); }

private:
    void MarkDirty(int x, int y, int w, int h);

    int m_Width = 0, m_Height = 0;
    std::vector<unsigned char> m_Pixels;    // RGBA8, row-major -- authored BASE truth
    std::vector<unsigned char> m_LitPixels; // RGBA8, row-major -- base mixed with this-frame's lighting; what Flush() uploads

    // Per-pixel, float, row-major, NOT RGBA8 -- this frame's raw light
    // accumulation, resolved into m_LitPixels by AccumulateLightTint on
    // every write (see its comment) rather than held until some separate
    // end-of-frame pass. RGB only (no alpha channel; lighting never
    // touches solidity), so these are W*H entries each, not W*H*4.
    std::vector<float> m_LightAccumColor;  // running sum of tint.rgb * strength, per pixel (3 floats/pixel: r,g,b)
    std::vector<float> m_LightAccumWeight; // running sum of strength alone, per pixel (1 float/pixel)

    std::unique_ptr<Texture> m_Texture; // GPU mirror; null if PNG failed to load

    bool m_Dirty = false;
    int m_DirtyMinX = 0, m_DirtyMinY = 0, m_DirtyMaxX = 0, m_DirtyMaxY = 0;
};