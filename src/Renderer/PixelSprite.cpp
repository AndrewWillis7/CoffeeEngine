#include "PixelSprite.h"
#include "../OS_/stb_image.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

PixelSprite::PixelSprite(const std::string& filepath, Texture::Filter filter) {
    int channels = 0;
    // Same not-flipped convention as Texture::Texture(filepath) -- see the
    // comment there. Decoded here (rather than reusing that constructor)
    // because we need to keep our own copy of the pixels around for
    // SetPixel/PunchCircle/IsSolid; Texture's file constructor frees its
    // decoded buffer right after the initial upload.
    unsigned char* data = stbi_load(filepath.c_str(), &m_Width, &m_Height, &channels, 4);
    if (!data) {
        std::cerr << "Engine Warning: PixelSprite failed to load '" << filepath << "'\n";
        return; // m_Texture stays null -- IsValid() reports false
    }

    m_Pixels.assign(data, data + (static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) * 4));
    stbi_image_free(data);

    m_LitPixels = m_Pixels; // unlit == base until LightingSystem says otherwise
    m_LightAccumColor.assign(static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) * 3, 0.0f);
    m_LightAccumWeight.assign(static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height), 0.0f);
    m_Texture = std::make_unique<Texture>(m_LitPixels.data(), m_Width, m_Height, Texture::Format::RGBA, filter);
}

PixelSprite::PixelSprite(int width, int height, const Color& fill, Texture::Filter filter)
    : m_Width(width), m_Height(height) {
    if (width <= 0 || height <= 0) {
        std::cerr << "Engine Warning: PixelSprite(width, height, fill) got a non-positive size ("
                   << width << "x" << height << ") -- leaving invalid.\n";
        m_Width = m_Height = 0;
        return;
    }

    unsigned char r = static_cast<unsigned char>(std::clamp(fill.r, 0.0f, 1.0f) * 255.0f);
    unsigned char g = static_cast<unsigned char>(std::clamp(fill.g, 0.0f, 1.0f) * 255.0f);
    unsigned char b = static_cast<unsigned char>(std::clamp(fill.b, 0.0f, 1.0f) * 255.0f);
    unsigned char a = static_cast<unsigned char>(std::clamp(fill.a, 0.0f, 1.0f) * 255.0f);

    m_Pixels.resize(static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) * 4);
    for (size_t i = 0; i < m_Pixels.size(); i += 4) {
        m_Pixels[i + 0] = r; m_Pixels[i + 1] = g; m_Pixels[i + 2] = b; m_Pixels[i + 3] = a;
    }

    m_LitPixels = m_Pixels;
    m_LightAccumColor.assign(static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) * 3, 0.0f);
    m_LightAccumWeight.assign(static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height), 0.0f);
    m_Texture = std::make_unique<Texture>(m_LitPixels.data(), m_Width, m_Height, Texture::Format::RGBA, filter);
}

void PixelSprite::SetPixel(int x, int y, const Color& color) {
    if (x < 0 || y < 0 || x >= m_Width || y >= m_Height) return;

    size_t i = (static_cast<size_t>(y) * m_Width + x) * 4;
    unsigned char r = static_cast<unsigned char>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
    unsigned char g = static_cast<unsigned char>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
    unsigned char b = static_cast<unsigned char>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
    unsigned char a = static_cast<unsigned char>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f);

    m_Pixels[i + 0] = r; m_Pixels[i + 1] = g; m_Pixels[i + 2] = b; m_Pixels[i + 3] = a;
    // Mirror straight into the lit buffer too -- an authored edit must be
    // visible immediately even on a pixel no light happens to be touching
    // this frame (the common case: most of a sprite, most of the time).
    m_LitPixels[i + 0] = r; m_LitPixels[i + 1] = g; m_LitPixels[i + 2] = b; m_LitPixels[i + 3] = a;
    MarkDirty(x, y, 1, 1);
}

Color PixelSprite::GetPixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_Width || y >= m_Height) return Color::Transparent();

    size_t i = (static_cast<size_t>(y) * m_Width + x) * 4;
    return Color(m_Pixels[i + 0] / 255.0f, m_Pixels[i + 1] / 255.0f, m_Pixels[i + 2] / 255.0f, m_Pixels[i + 3] / 255.0f);
}

bool PixelSprite::IsSolid(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_Width || y >= m_Height) return false;
    return m_Pixels[(static_cast<size_t>(y) * m_Width + x) * 4 + 3] > 0;
}

void PixelSprite::PunchCircle(int cx, int cy, float radius) {
    int minX = std::max(0, static_cast<int>(std::floor(cx - radius)));
    int maxX = std::min(m_Width - 1, static_cast<int>(std::ceil(cx + radius)));
    int minY = std::max(0, static_cast<int>(std::floor(cy - radius)));
    int maxY = std::min(m_Height - 1, static_cast<int>(std::ceil(cy + radius)));
    if (minX > maxX || minY > maxY) return; // fully off-sprite, nothing to do

    float r2 = radius * radius;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            float dx = static_cast<float>(x - cx);
            float dy = static_cast<float>(y - cy);
            if (dx * dx + dy * dy <= r2) {
                size_t alphaIdx = (static_cast<size_t>(y) * m_Width + x) * 4 + 3;
                m_Pixels[alphaIdx] = 0;
                m_LitPixels[alphaIdx] = 0; // keep both buffers' solidity in lockstep
            }
        }
    }
    MarkDirty(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

void PixelSprite::ResetLightingRect(int minX, int minY, int maxX, int maxY) {
    minX = std::max(0, minX);
    minY = std::max(0, minY);
    maxX = std::min(m_Width - 1, maxX);
    maxY = std::min(m_Height - 1, maxY);
    if (minX > maxX || minY > maxY) return; // fully off-sprite, nothing to do

    size_t rowBytes = (static_cast<size_t>(maxX - minX) + 1) * 4;
    size_t accumRowFloats = (static_cast<size_t>(maxX - minX) + 1) * 3;
    size_t weightRowFloats = (static_cast<size_t>(maxX - minX) + 1);
    for (int y = minY; y <= maxY; ++y) {
        size_t rowStart = (static_cast<size_t>(y) * m_Width + minX) * 4;
        std::memcpy(m_LitPixels.data() + rowStart, m_Pixels.data() + rowStart, rowBytes);

        // Zero this rect's accumulation too -- not just the visible lit
        // color -- so next frame's mix starts from a clean (0 weight)
        // slate instead of quietly carrying over last frame's light
        // contributions and biasing the very first AccumulateLightTint
        // call of the new frame.
        size_t accumRowStart = (static_cast<size_t>(y) * m_Width + minX) * 3;
        size_t weightRowStart = static_cast<size_t>(y) * m_Width + minX;
        std::fill_n(m_LightAccumColor.data() + accumRowStart, accumRowFloats, 0.0f);
        std::fill_n(m_LightAccumWeight.data() + weightRowStart, weightRowFloats, 0.0f);
    }
    MarkDirty(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

void PixelSprite::AccumulateLightTint(int x, int y, const Color& tint, float strength) {
    if (x < 0 || y < 0 || x >= m_Width || y >= m_Height || strength <= 0.0f) return;

    size_t i = (static_cast<size_t>(y) * m_Width + x) * 4;
    if (m_Pixels[i + 3] == 0) return; // nothing solid/drawn here -- don't light empty space

    // Fold this light's contribution into the running weighted sum first.
    size_t accumI = (static_cast<size_t>(y) * m_Width + x) * 3;
    size_t weightI = static_cast<size_t>(y) * m_Width + x;
    m_LightAccumColor[accumI + 0] += tint.r * strength;
    m_LightAccumColor[accumI + 1] += tint.g * strength;
    m_LightAccumColor[accumI + 2] += tint.b * strength;
    m_LightAccumWeight[weightI] += strength;
    float totalWeight = m_LightAccumWeight[weightI];

    // Resolve: mix the base color toward the accumulated lights' own
    // weighted-average color, by however much light has actually reached
    // this pixel. `mixAmount` saturates at 1.0 (fully replaced by the
    // light color, never blown out past it) even if several lights (or
    // several rays off the same light) keep adding weight beyond that --
    // see AccumulateLightTint's header comment for why this reads as a
    // genuine mix rather than an additive wash.
    float baseR = m_Pixels[i + 0] / 255.0f;
    float baseG = m_Pixels[i + 1] / 255.0f;
    float baseB = m_Pixels[i + 2] / 255.0f;

    float mixAmount = std::clamp(totalWeight, 0.0f, 1.0f);
    float avgR = m_LightAccumColor[accumI + 0] / totalWeight;
    float avgG = m_LightAccumColor[accumI + 1] / totalWeight;
    float avgB = m_LightAccumColor[accumI + 2] / totalWeight;

    float r = baseR * (1.0f - mixAmount) + avgR * mixAmount;
    float g = baseG * (1.0f - mixAmount) + avgG * mixAmount;
    float b = baseB * (1.0f - mixAmount) + avgB * mixAmount;

    m_LitPixels[i + 0] = static_cast<unsigned char>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
    m_LitPixels[i + 1] = static_cast<unsigned char>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
    m_LitPixels[i + 2] = static_cast<unsigned char>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
    // Alpha untouched -- lighting only ever recolors, never changes solidity.
    MarkDirty(x, y, 1, 1);
}

void PixelSprite::MarkDirty(int x, int y, int w, int h) {
    int x2 = x + w - 1;
    int y2 = y + h - 1;
    if (!m_Dirty) {
        m_DirtyMinX = x; m_DirtyMinY = y; m_DirtyMaxX = x2; m_DirtyMaxY = y2;
        m_Dirty = true;
    } else {
        m_DirtyMinX = std::min(m_DirtyMinX, x);
        m_DirtyMinY = std::min(m_DirtyMinY, y);
        m_DirtyMaxX = std::max(m_DirtyMaxX, x2);
        m_DirtyMaxY = std::max(m_DirtyMaxY, y2);
    }
}

void PixelSprite::Flush() {
    if (!m_Dirty || !m_Texture) return;

    int x = m_DirtyMinX, y = m_DirtyMinY;
    int w = m_DirtyMaxX - m_DirtyMinX + 1;
    int h = m_DirtyMaxY - m_DirtyMinY + 1;

    const unsigned char* regionStart = m_LitPixels.data() + (static_cast<size_t>(y) * m_Width + x) * 4;
    m_Texture->UpdateRegion(x, y, w, h, regionStart);

    m_Dirty = false;
}