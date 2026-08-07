#include "PixelSprite.h"
#include "../OS_/stb_image.h"
#include <algorithm>
#include <cmath>
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

    m_Texture = std::make_unique<Texture>(m_Pixels.data(), m_Width, m_Height, Texture::Format::RGBA, filter);
}

void PixelSprite::SetPixel(int x, int y, const Color& color) {
    if (x < 0 || y < 0 || x >= m_Width || y >= m_Height) return;

    size_t i = (static_cast<size_t>(y) * m_Width + x) * 4;
    m_Pixels[i + 0] = static_cast<unsigned char>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
    m_Pixels[i + 1] = static_cast<unsigned char>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
    m_Pixels[i + 2] = static_cast<unsigned char>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
    m_Pixels[i + 3] = static_cast<unsigned char>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f);
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
                m_Pixels[(static_cast<size_t>(y) * m_Width + x) * 4 + 3] = 0;
            }
        }
    }
    MarkDirty(minX, minY, maxX - minX + 1, maxY - minY + 1);
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

    const unsigned char* regionStart = m_Pixels.data() + (static_cast<size_t>(y) * m_Width + x) * 4;
    m_Texture->UpdateRegion(x, y, w, h, regionStart);

    m_Dirty = false;
}