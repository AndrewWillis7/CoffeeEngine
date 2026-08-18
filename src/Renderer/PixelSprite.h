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

    // Sets alpha to 0 for every pixel within radius of (cx, cy) -- the
    // Noita-style "blow a hole in it" primitive. Leaves RGB untouched so a
    // later SetPixel/inspection still sees the original color if alpha
    // were ever restored.
    void PunchCircle(int cx, int cy, float radius);

    // =====================================================================
    // Lighting overlay -- LightingSystem-only (not exposed to Lua). Every
    // pixel this class holds actually lives in TWO buffers: m_Pixels is
    // the authored "base" truth that SetPixel/GetPixel/IsSolid/PunchCircle
    // above all read and write, exactly as before lighting existed at all.
    // m_LitPixels mirrors it but with each frame's lighting tint mixed in
    // on top -- it's what Flush() actually uploads to the GPU. Splitting
    // these apart means lighting can recolor what's ON SCREEN every single
    // frame (never baked, never destructive) without corrupting the
    // pixel data any gameplay/destruction code actually queries.
    //
    // Both are seeded equal (lit == base) by both constructors and kept in
    // sync by SetPixel/PunchCircle, so a sprite nothing ever lights still
    // displays its authored colors exactly as before -- lighting is purely
    // additive on top, never a prerequisite for correct rendering.
    // =====================================================================

    // Copies base -> lit for every pixel in [minX,minY]..[maxX,maxY]
    // (inclusive, clamped to bounds), discarding whatever tint was mixed
    // in there before. LightingSystem calls this once per frame, on
    // whatever rect ITS OWN bookkeeping says was lit last frame, before
    // re-accumulating this frame's lights -- so a torch that moved (or
    // was removed) cleanly erases its own glow instead of leaving a
    // stale tinted patch behind.
    void ResetLightingRect(int minX, int minY, int maxX, int maxY);

    // Adds `tint * strength` on top of whatever's currently in the lit
    // buffer at (x, y), clamped to 1.0 per channel (blown-out/white-hot
    // at brightest, never wraps). No-op on a non-solid (fully transparent
    // -- punched out, or simply never drawn) pixel; alpha itself is never
    // touched, so lighting can never change what's solid. Call
    // ResetLightingRect on this pixel's rect first if you want a clean
    // "starting from base" accumulation -- this call always ADDS to
    // whatever's already there, which is what lets several overlapping
    // lights stack naturally within one frame.
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
    std::vector<unsigned char> m_LitPixels; // RGBA8, row-major -- base + this-frame's lighting tint; what Flush() uploads
    std::unique_ptr<Texture> m_Texture; // GPU mirror; null if PNG failed to load

    bool m_Dirty = false;
    int m_DirtyMinX = 0, m_DirtyMinY = 0, m_DirtyMaxX = 0, m_DirtyMaxY = 0;
};