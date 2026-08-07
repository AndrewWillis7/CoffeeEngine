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
    std::vector<unsigned char> m_Pixels; // RGBA8, row-major, m_Width texels wide
    std::unique_ptr<Texture> m_Texture; // GPU mirror; null if PNG failed to load

    bool m_Dirty = false;
    int m_DirtyMinX = 0, m_DirtyMinY = 0, m_DirtyMaxX = 0, m_DirtyMaxY = 0;
};