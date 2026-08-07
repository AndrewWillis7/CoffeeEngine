#include "Texture.h"
#include <GL/gl.h>
#include <iostream>
#include "../OS_/stb_image.h"

Texture::Texture(const std::string& filepath, Filter filter) {
    int channels = 0;
    // NOT flipped, unlike the usual OpenGL-tutorial advice: this engine's
    // vertex shader (BuiltInShaders::QuadVertexSrc) uses a top-left-origin,
    // +y-down screen convention, and working through its actual uv math
    // (uv = v_LocalPos + 0.5) shows uv.y=0 already lands at the TOP of the
    // drawn quad -- which is exactly where stb_image's un-flipped row 0
    // already is. If a loaded image ever comes out upside-down on your
    // build, that derivation was wrong for your setup -- add
    // stbi_set_flip_vertically_on_load(1) here and it's fixed.
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
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(glFormat), m_Width, m_Height, 0, glFormat, GL_UNSIGNED_BYTE, pixels);
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
    // GL_UNPACK_ROW_LENGTH tells GL the source buffer's full row width in
    // texels, so it can stride through `pixels` (which points at the (x,y)
    // texel of a GetWidth()-wide buffer, not a tightly-packed w*h buffer)
    // without the caller needing to memcpy a sub-copy first.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, m_Width);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, static_cast<GLenum>(m_GLFormat), GL_UNSIGNED_BYTE, pixels);
    // GL_UNPACK_ROW_LENGTH is global GL state, not per-texture -- reset it
    // to the default (0 = "tightly packed") so it doesn't silently corrupt
    // some other Texture/Font upload that runs later and assumes default
    // unpack state.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}