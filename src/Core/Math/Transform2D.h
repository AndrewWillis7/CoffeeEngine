#pragma once
#include "Vector2.h"

// Rotation is radians; the Lua bindings convert at the boundary.
struct Transform2D {
    Vector2 position;
    float rotation = 0.0f;
    Vector2 scale = Vector2::One();
};