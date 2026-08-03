#pragma once
#include <string>

class Texture {
public:
    enum class Format { RGBA, Alpha };

    explicit Texture(const std::string& filepath);                             // load from disk
    Texture(const unsigned char* pixels, int width, int height, Format format); // build from memory (e.g. a font atlas)
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void Bind() const;
    bool IsValid() const { return m_Handle != 0; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

private:
    void Upload(const unsigned char* pixels, unsigned int glFormat);

    unsigned int m_Handle = 0;
    int m_Width = 0, m_Height = 0;
};