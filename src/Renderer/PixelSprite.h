#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Texture.h"
#include "Core/Math/Color.h"

class PixelSprite {
public:
    // Filter defaults to Nearest rather than Texture's Linear: torn and punched
    // edges should stay crisp, not bleed into neighbours.
    explicit PixelSprite(const std::string& filepath, Texture::Filter filter = Texture::Filter::Nearest);

    // A blank in-memory sprite filled with `fill` -- the "turn a flat quad into
    // addressable pixels" primitive, so SetPixel/PunchCircle/lighting work on a
    // plain body exactly as they would on a loaded PNG.
    PixelSprite(int width, int height, const Color& fill, Texture::Filter filter = Texture::Filter::Nearest);

    PixelSprite(const PixelSprite&) = delete;
    PixelSprite& operator=(const PixelSprite&) = delete;

    bool IsValid() const { return m_Texture && m_Texture->IsValid(); }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    // (0, 0) is top-left. Out-of-bounds is silently ignored / reads transparent
    // rather than asserting: a punch radius near an edge or a probe just off the
    // sprite hits this constantly and shouldn't have to bounds-check first.
    void SetPixel(int x, int y, const Color& color);
    Color GetPixel(int x, int y) const;

    // Inline deliberately -- this is the innermost test of the lighting pixel
    // walk, the occlusion march and Physics::RaycastDown's column scan.
    bool IsSolid(int x, int y) const {
        if (x < 0 || y < 0 || x >= m_Width || y >= m_Height) return false;
        return m_Pixels[(static_cast<size_t>(y) * m_Width + x) * 4 + 3] > 0;
    }

    // --- Raster primitives: authoring a sprite from code, every frame. -------
    // A procedural rig redraws a few hundred pixels per frame and can't pay a
    // Lua->C++ boundary per pixel, so these do a whole shape in one call. All of
    // them write the base buffer and mirror into the lit buffer, and all clamp
    // to bounds -- a limb solved off-canvas gets cropped, not crashed.

    // Back to fully transparent, lighting accumulation included, so next frame's
    // mix doesn't carry weight from a pixel that used to be solid here.
    void Clear();

    // Axis-aligned filled rect, (x, y) = top-left, overwriting rather than
    // blending. Draws anything that must stay on the pixel grid: knee, boot, foot.
    void FillRect(int x, int y, int w, int h, const Color& color);

    // A straight limb `thickness` texels wide, rasterized as one run per step
    // along the segment's MAJOR axis. That scan is why this exists instead of a
    // rotated quad: exactly one run per row/column means no diagonal pinholes,
    // thickness is measured along a grid axis, and every edge lands on a texel
    // boundary. A rotated quad's edges fall wherever the angle puts them, so the
    // same limb shimmers between silhouettes as it moves.
    //
    // Runs are floor-centered consistently at every thickness, so chained
    // segments of different widths share a center line instead of stepping
    // sideways at the joint.
    void DrawLimb(int x0, int y0, int x1, int y1, int thickness, const Color& color);

    // DrawLimb with thickness varying from `t0` to `t1`, plus an optional smooth
    // `bulge` peaking at `bulgeAt` (0..1 along the bone). This is what stops a
    // two-bone chain reading as two sticks: at 320x180 the silhouette is the
    // entire character. Same major-axis scan and floor-centered runs as DrawLimb,
    // so tapered and untapered segments still chain without a sideways step.
    void DrawTaperedLimb(int x0, int y0, int x1, int y1,
                         int t0, int t1, float bulge, float bulgeAt,
                         const Color& color);

    // Alpha to 0 within radius of (cx, cy) -- the "blow a hole in it" primitive.
    // RGB is left intact so the original color survives if alpha is restored.
    void PunchCircle(int cx, int cy, float radius);

    // --- Lighting overlay: LightingSystem-only, not exposed to Lua. ----------
    // Every pixel lives in two RGBA8 buffers. m_Pixels is the authored base that
    // SetPixel/GetPixel/IsSolid/PunchCircle read and write; m_LitPixels mirrors
    // it with this frame's lighting mixed in, and is what Flush() uploads.
    // Splitting them lets lighting recolor the screen every frame, never
    // destructively, without corrupting the data gameplay queries.
    //
    // A float pair, m_LightAccumColor/m_LightAccumWeight, holds this frame's raw
    // contributions -- the running sums of tint*strength and of strength alone --
    // which is what turns N overlapping lights into one mix rather than a wash.
    // All four start equal (lit == base, accum == 0), so a sprite nothing lights
    // still displays its authored colors.

    // Copies base -> lit and zeroes accumulation over the inclusive rect,
    // discarding what it mixed in last frame. LightingSystem calls this on last
    // frame's lit rect before re-accumulating, so a torch that moved erases its
    // own glow instead of leaving a stale patch (or a stale weight) behind.
    void ResetLightingRect(int minX, int minY, int maxX, int maxY);

    // Folds one light into pixel (x, y)'s running mix and re-resolves
    // m_LitPixels immediately, so the lit buffer is always valid mid-
    // accumulation. No-op on a transparent pixel; alpha is never touched, so
    // lighting can't change solidity.
    //
    // A genuine mix, not add-and-clamp: the base color blends toward the
    // accumulated lights' weighted-average color by saturate(accumWeight). A
    // pixel grazed by one weak light stays near its base; one in a hotspot reads
    // as that light's color rather than blowing out toward white; one touched by
    // a warm campfire and a cool spotlight shows a real blend of the two.
    void AccumulateLightTint(int x, int y, const Color& tint, float strength);

    // One glTexSubImage2D over the accumulated dirty rect. DrawBody() calls this
    // automatically; it is exposed to Lua only as an escape hatch.
    void Flush();

    Texture* GetTexture() { return m_Texture.get(); }

private:
    void MarkDirty(int x, int y, int w, int h);

    int m_Width = 0, m_Height = 0;
    std::vector<unsigned char> m_Pixels;    // RGBA8, row-major -- authored base truth
    std::vector<unsigned char> m_LitPixels; // RGBA8, row-major -- what Flush() uploads

    // Resolved into m_LitPixels on every write rather than in an end-of-frame
    // pass. RGB only, since lighting never touches solidity.
    std::vector<float> m_LightAccumColor;  // sum of tint.rgb * strength (3 floats/pixel)
    std::vector<float> m_LightAccumWeight; // sum of strength alone (1 float/pixel)

    std::unique_ptr<Texture> m_Texture; // GPU mirror; null if the PNG failed to load

    bool m_Dirty = false;
    int m_DirtyMinX = 0, m_DirtyMinY = 0, m_DirtyMaxX = 0, m_DirtyMaxY = 0;
};
