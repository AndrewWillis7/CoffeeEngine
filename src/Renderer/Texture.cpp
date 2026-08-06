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