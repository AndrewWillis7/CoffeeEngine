#pragma once
#include <string>

class Texture {
public:
    enum class Format { RGBA, Alpha };
    enum class Filter {Linear, Nearest};

    explicit Texture(const std::string& filepath, Filter filter = Filter::Linear);                             // load from disk
    Texture(const unsigned char* pixels, int width, int height, Format format, Filter filter = Filter::Linear); // build from memory (e.g. a font atlas)
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void Bind() const;
    bool IsValid() const { return m_Handle != 0; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    // Re-uploads a sub-rect. `pixels` must point at the (x, y) texel of a buffer
    // GetWidth() texels wide, not a tightly packed w*h copy.
    void UpdateRegion(int x, int y, int w, int h, const unsigned char* pixels);

private:
    void Upload(const unsigned char* pixels, unsigned int glFormat, Filter filter);

    unsigned int m_Handle = 0;
    int m_Width = 0, m_Height = 0;
    unsigned int m_GLFormat = 0; // Set by Upload(), reused by UpdateRegion()
};