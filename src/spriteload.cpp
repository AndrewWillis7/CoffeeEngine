#include "spriteload.h"

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

Sprite::Sprite(const wchar_t* filePath, int frameWidth, int frameHeight, int columns, int rows) :
    m_frameWidth(frameWidth), m_frameHeight(frameHeight), m_columns(columns), m_rows(rows) {

    m_image = Image::FromFile(filePath);

    m_tintR = m_tintG = m_tintB = m_tintA = 1.0f;

    if (!m_image || m_image->GetLastStatus() != Ok) {
        m_image = nullptr;
        MessageBoxW(NULL, L"Failed to Load PNG!", L"Error", MB_OK);
    }

    if (m_image) {
        m_frameCount = m_columns * m_rows;
    }
    UpdateColorMatrix();
}

Sprite::~Sprite() {
    delete m_image;
}

void Sprite::SetTint(float r, float g, float b, float a) {
    m_tintR = r;
    m_tintG = g;
    m_tintB = b;
    m_tintA = a;

    UpdateColorMatrix();
}

void Sprite::ClearTint() {
    SetTint(1, 1, 1, 1);
}

void Sprite::Draw(HDC hdc, int x, int y, int frame, bool flip, float scale) {
    if (!m_image) return;

    Graphics graphics(hdc);
    int col = frame % m_columns;
    int row = frame / m_columns;

    int srcX = col * m_frameWidth;
    int srcY = row * m_frameHeight;

    graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);

    int drawWidth = (int)(m_frameWidth * scale);
    int drawHeight = (int)(m_frameHeight * scale);

    if (flip) {
        graphics.TranslateTransform((REAL)(x + drawWidth), (REAL)y);
        graphics.ScaleTransform(-1.0f, 1.0f);

        Rect destRect(0, 0, drawWidth, drawHeight);

        graphics.DrawImage(
            m_image,
            destRect,
            srcX, srcY,
            m_frameWidth,
            m_frameHeight,
            UnitPixel,
            &m_imageAttributes
        );

        graphics.ResetTransform();
    } else {
        Rect destRect(x, y, drawWidth, drawHeight);

        graphics.DrawImage(
            m_image,
            destRect,
            srcX, srcY,
            m_frameWidth,
            m_frameHeight,
            UnitPixel,
            &m_imageAttributes
        );
    }
}

void Sprite::UpdateColorMatrix() {
    ColorMatrix colorMatrix = {
        m_tintR, 0,       0,       0, 0,
        0,       m_tintG, 0,       0, 0,
        0,       0,       m_tintB, 0, 0,
        0,       0,       0,       m_tintA, 0,
        0,       0,       0,       0, 1
    };
    
    m_imageAttributes.SetColorMatrix(
        &colorMatrix,
        ColorMatrixFlagsDefault,
        ColorAdjustTypeBitmap
    );
}