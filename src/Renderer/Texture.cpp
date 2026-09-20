#include "Texture.h"
#include "GLLoader.h" // GL/gl.h + glext.h (GL_CLAMP_TO_EDGE is GL 1.2)
#include <iostream>
#include "../OS_/stb_image.h"

Texture::Texture(const std::string& filepath, Filter filter) {
    int channels = 0;
    // NOT flipped, unlike the usual advice: with +y down and uv = v_LocalPos +
    // 0.5, uv.y = 0 already lands at the top of the quad, which is where
    // stb_image's un-flipped row 0 is. If images ever load upside-down, add
    // stbi_set_flip_vertically_on_load(1) here.
    unsigned char* data = stbi_load(filepath.c_str(), &m_Width, &m_Height, &channels, 4);
    if (!data) {
        std::cerr << "Engine Warning: Texture failed to load '" << filepath << "'\n";
        return;
    }
    Upload(data, GL_RGBA, filter);
    stbi_image_free(data);
}

Texture::Texture(const unsigned char* pixels, int width, int height, Format format, Filter filter)
    : m_Width(width), m_Height(height) {
    Upload(pixels, format == Format::Alpha ? GL_ALPHA : GL_RGBA, filter);
}

void Texture::Upload(const unsigned char* pixels, unsigned int glFormat, Filter filter) {
    m_GLFormat = glFormat;
    GLint glFilter = (filter == Filter::Nearest) ? GL_NEAREST : GL_LINEAR;

    glGenTextures(1, &m_Handle);
    glBindTexture(GL_TEXTURE_2D, m_Handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // RGBA rows are always 4-byte aligned, but a single-channel GL_ALPHA upload
    // is 1 byte/texel, so any width not a multiple of 4 shears unless this is 1.
    glPixelStorei(GL_UNPACK_ALIGNMENT, (glFormat == GL_ALPHA) ? 1 : 4);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(glFormat), m_Width, m_Height, 0, glFormat, GL_UNSIGNED_BYTE, pixels);
    // Global state, not per-texture -- reset so a later upload isn't affected.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture() {
    if (m_Handle) glDeleteTextures(1, &m_Handle);
}

void Texture::Bind() const {
    glBindTexture(GL_TEXTURE_2D, m_Handle);
}

void Texture::UpdateRegion(int x, int y, int w, int h, const unsigned char* pixels) {
    if (!m_Handle || w <= 0 || h <= 0) return;

    glBindTexture(GL_TEXTURE_2D, m_Handle);
    // Tells GL the source buffer's full row width so it can stride through
    // `pixels` without the caller memcpy-ing a tightly packed sub-copy first.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, m_Width);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, static_cast<GLenum>(m_GLFormat), GL_UNSIGNED_BYTE, pixels);
    // Global state -- reset to the tightly-packed default for later uploads.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}