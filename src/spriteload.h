#pragma once

#include <windows.h>
#include <ole2.h>

#undef min
#undef max

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

class Sprite {
private:
    Gdiplus::Image* m_image;

    // Sizing Constraints
    int m_frameWidth;
    int m_frameHeight;
    int m_frameCount;

    int m_columns;
    int m_rows;

    // Tint States
    float m_tintR = 1.0f;
    float m_tintG = 1.0f;
    float m_tintB = 1.0f;
    float m_tintA = 1.0f;

    ImageAttributes m_imageAttributes;

    void UpdateColorMatrix();

public:
    Sprite(const wchar_t* filePath, int frameWidth, int frameHeightm, int columns, int rows);
    ~Sprite();
    void Draw(HDC hdc, int x, int y, int frame, bool flip, float scale);
    void SetTint(float r, float g, float b, float a = 1.0f);
    void ClearTint();
};