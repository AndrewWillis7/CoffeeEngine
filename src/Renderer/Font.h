#pragma once
#include <memory>
#include "Core/Math/Vector2.h"

class Texture;

// Fixed-width 8x8 bitmap font baked into one texture atlas at construction.
// Glyph data is public domain (see Font.cpp) -- no FreeType dependency.
class Font {
public:
    Font();
    ~Font();

    bool IsValid() const;
    Texture* GetAtlas() const { return m_Atlas.get(); }

    struct GlyphUV { Vector2 offset; Vector2 scale; };
    // UV rect within the atlas for one ASCII character. Out-of-range
    // characters fall back to index 0 (blank) rather than garbage.
    GlyphUV GetGlyphUV(unsigned char c) const;

private:
    std::unique_ptr<Texture> m_Atlas;
};