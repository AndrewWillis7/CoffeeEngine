#pragma once
#include "Vector2.h"

// Position + Rotation + scale
// Conversion to LUA should handle Degrees/Radians Conversion at the Boundary, not here honestly
struct Transform2D {
    Vector2 position;
    float rotation = 0.0f;
    Vector2 scale = Vector2::One();
};