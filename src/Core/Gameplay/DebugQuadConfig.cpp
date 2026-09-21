#include "DebugQuadConfig.h"
#include "Renderer/PixelSprite.h"
#include "Core/Math/Color.h"

namespace {

constexpr Color kBackground(0.13f, 0.13f, 0.16f, 1.0f);
constexpr Color kGridLine(0.42f, 0.42f, 0.48f, 1.0f);
constexpr Color kCrosshair(0.85f, 0.85f, 0.92f, 1.0f);
constexpr Color kOutline(0.55f, 0.55f, 0.62f, 1.0f);
constexpr int kCrosshairArm = 2; // texels either side of an intersection

} // namespace

void GenerateDebugQuadTexture(PixelSprite& sprite, int cellsX, int cellsY) {
    const int cell = DebugQuadConfig::kCellSize;
    const int width = sprite.GetWidth();
    const int height = sprite.GetHeight();
    if (width <= 0 || height <= 0) return;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) sprite.SetPixel(x, y, kBackground);
    }

    // Internal cell boundaries, one line per shared edge between two cells.
    for (int cx = 1; cx < cellsX; ++cx) {
        const int x = cx * cell;
        for (int y = 0; y < height; ++y) sprite.SetPixel(x, y, kGridLine);
    }
    for (int cy = 1; cy < cellsY; ++cy) {
        const int y = cy * cell;
        for (int x = 0; x < width; ++x) sprite.SetPixel(x, y, kGridLine);
    }

    // A small "+" at every internal intersection, over the grid lines.
    for (int cx = 1; cx < cellsX; ++cx) {
        for (int cy = 1; cy < cellsY; ++cy) {
            const int x = cx * cell;
            const int y = cy * cell;
            for (int d = -kCrosshairArm; d <= kCrosshairArm; ++d) {
                sprite.SetPixel(x + d, y, kCrosshair);
                sprite.SetPixel(x, y + d, kCrosshair);
            }
        }
    }

    // One-texel outline so the quad's extent reads clearly against whatever
    // it's placed over.
    for (int x = 0; x < width; ++x) {
        sprite.SetPixel(x, 0, kOutline);
        sprite.SetPixel(x, height - 1, kOutline);
    }
    for (int y = 0; y < height; ++y) {
        sprite.SetPixel(0, y, kOutline);
        sprite.SetPixel(width - 1, y, kOutline);
    }
}
