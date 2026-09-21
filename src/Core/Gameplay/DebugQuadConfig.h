#pragma once

class PixelSprite;

// Marks a RigidBody2D as an editor-placed "debug quad": a procedurally
// generated grid-and-crosshair rectangle with no source image, sized in whole
// cells rather than continuously. Owned by ActorRegistry, like
// LightEmitterConfig. Placed and resized entirely from the scene editor's
// Asset Menu / inspector (see SceneEditor, SceneFields' Group::Asset fields).
class DebugQuadConfig {
public:
    // The "scale value" a resize steps by: fixed engine-wide so every quad
    // shares one grid, the way the editor's own move/rotate snaps do.
    static constexpr int kCellSize = 8;
    static constexpr int kMaxCells = 40;

    int cellsX = 1;
    int cellsY = 1;
};

// Redraws `sprite` (already sized cellsX*kCellSize x cellsY*kCellSize) as a
// dark grid: a lighter line along every internal cell boundary, a small "+"
// crosshair at every internal intersection, and a one-texel outline. Modeled
// on TerrainChunk::Generate's per-texel SetPixel authoring.
void GenerateDebugQuadTexture(PixelSprite& sprite, int cellsX, int cellsY);
