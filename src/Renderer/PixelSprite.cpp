#include "PixelSprite.h"
#include "../OS_/stb_image.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

PixelSprite::PixelSprite(const std::string& filepath, Texture::Filter filter) {
    int channels = 0;
    // Same not-flipped convention as Texture's own file constructor, but decoded
    // here because we keep the pixels around for SetPixel/PunchCircle/IsSolid;
    // Texture frees its decoded buffer right after the initial upload.
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
    // Mirror into the lit buffer too: an authored edit must show immediately,
    // even on a pixel no light happens to touch this frame.
    m_LitPixels[i + 0] = r; m_LitPixels[i + 1] = g; m_LitPixels[i + 2] = b; m_LitPixels[i + 3] = a;
    MarkDirty(x, y, 1, 1);
}

Color PixelSprite::GetPixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_Width || y >= m_Height) return Color::Transparent();

    size_t i = (static_cast<size_t>(y) * m_Width + x) * 4;
    return Color(m_Pixels[i + 0] / 255.0f, m_Pixels[i + 1] / 255.0f, m_Pixels[i + 2] / 255.0f, m_Pixels[i + 3] / 255.0f);
}

void PixelSprite::Clear() {
    if (m_Pixels.empty()) return;

    std::fill(m_Pixels.begin(), m_Pixels.end(), static_cast<unsigned char>(0));
    std::fill(m_LitPixels.begin(), m_LitPixels.end(), static_cast<unsigned char>(0));
    // Accumulation goes with it: a texel that was solid last frame and is empty
    // now must not hand its leftover weight to whatever is drawn there next.
    std::fill(m_LightAccumColor.begin(), m_LightAccumColor.end(), 0.0f);
    std::fill(m_LightAccumWeight.begin(), m_LightAccumWeight.end(), 0.0f);

    MarkDirty(0, 0, m_Width, m_Height);
}

void PixelSprite::FillRect(int x, int y, int w, int h, const Color& color) {
    if (w <= 0 || h <= 0) return;

    int minX = std::max(0, x);
    int minY = std::max(0, y);
    int maxX = std::min(m_Width - 1, x + w - 1);
    int maxY = std::min(m_Height - 1, y + h - 1);
    if (minX > maxX || minY > maxY) return; // fully off-sprite

    unsigned char rgba[4] = {
        static_cast<unsigned char>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f),
    };

    for (int py = minY; py <= maxY; ++py) {
        size_t i = (static_cast<size_t>(py) * m_Width + minX) * 4;
        for (int px = minX; px <= maxX; ++px, i += 4) {
            std::memcpy(m_Pixels.data() + i, rgba, 4);
            std::memcpy(m_LitPixels.data() + i, rgba, 4);
        }
    }
    MarkDirty(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

void PixelSprite::DrawLimb(int x0, int y0, int x1, int y1, int thickness, const Color& color) {
    if (thickness < 1) thickness = 1;

    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = std::max(std::abs(dx), std::abs(dy));

    // Degenerate: still draw one run so a zero-length module doesn't vanish.
    if (steps == 0) {
        FillRect(x0 - thickness / 2, y0, thickness, 1, color);
        return;
    }

    // Integer-rational interpolation rather than a float accumulator: the minor
    // coordinate is recomputed from `i` each step so it cannot drift, and the
    // same endpoints always produce bit-identical runs. That is what stops a
    // held pose from shimmering.
    if (std::abs(dy) >= std::abs(dx)) {
        int stepY = (dy > 0) ? 1 : -1;
        for (int i = 0; i <= steps; ++i) {
            int py = y0 + stepY * i;
            int px = x0 + static_cast<int>(std::lround(static_cast<double>(dx) * i / steps));
            FillRect(px - thickness / 2, py, thickness, 1, color);
        }
    } else {
        int stepX = (dx > 0) ? 1 : -1;
        for (int i = 0; i <= steps; ++i) {
            int px = x0 + stepX * i;
            int py = y0 + static_cast<int>(std::lround(static_cast<double>(dy) * i / steps));
            FillRect(px, py - thickness / 2, 1, thickness, color);
        }
    }
}

void PixelSprite::DrawTaperedLimb(int x0, int y0, int x1, int y1,
                                  int t0, int t1, float bulge, float bulgeAt,
                                  const Color& color) {
    if (t0 < 1) t0 = 1;
    if (t1 < 1) t1 = 1;
    bulgeAt = std::min(std::max(bulgeAt, 0.05f), 0.95f);

    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int steps = std::max(std::abs(dx), std::abs(dy));

    // Thickness at u along the bone. The hump is a half-sine remapped so its peak
    // lands on bulgeAt, keeping the muscle belly a curve rather than a kink.
    auto widthAt = [&](float u) -> int {
        float w = static_cast<float>(t0) + (static_cast<float>(t1) - static_cast<float>(t0)) * u;
        if (bulge != 0.0f) {
            const float v = (u < bulgeAt)
                ? (u / bulgeAt) * 0.5f
                : 0.5f + ((u - bulgeAt) / (1.0f - bulgeAt)) * 0.5f;
            w += bulge * std::sin(3.14159265358979323846f * v);
        }
        int iw = static_cast<int>(std::lround(w));
        return iw < 1 ? 1 : iw;
    };

    if (steps == 0) {
        const int t = widthAt(0.0f);
        FillRect(x0 - t / 2, y0, t, 1, color);
        return;
    }

    // Same drift-free integer-rational minor axis as DrawLimb.
    if (std::abs(dy) >= std::abs(dx)) {
        const int stepY = (dy > 0) ? 1 : -1;
        for (int i = 0; i <= steps; ++i) {
            const int py = y0 + stepY * i;
            const int px = x0 + static_cast<int>(std::lround(static_cast<double>(dx) * i / steps));
            const int t = widthAt(static_cast<float>(i) / static_cast<float>(steps));
            FillRect(px - t / 2, py, t, 1, color);
        }
    } else {
        const int stepX = (dx > 0) ? 1 : -1;
        for (int i = 0; i <= steps; ++i) {
            const int px = x0 + stepX * i;
            const int py = y0 + static_cast<int>(std::lround(static_cast<double>(dy) * i / steps));
            const int t = widthAt(static_cast<float>(i) / static_cast<float>(steps));
            FillRect(px, py - t / 2, 1, t, color);
        }
    }
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

        // Zero the accumulation as well as the visible color, or next frame's
        // first AccumulateLightTint call inherits last frame's weight.
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

    // Mix the base toward the lights' weighted-average color by however much
    // light actually reached here. mixAmount saturates at 1, so extra weight
    // never blows the pixel out past the light's own color.
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