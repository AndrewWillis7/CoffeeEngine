#pragma once
#include "Vector2.h"
#include <cmath>

// Axis-Aligned Bounding Box, defined by center + half-extents.
// This covers simple collision which is cheap and can be calculated before any real collider instances
struct AABB {
    Vector2 center;
    Vector2 halfExtents;

    Vector2 Min() const {return center - halfExtents;}
    Vector2 Max() const {return center + halfExtents;}

    bool Intersects(const AABB& other) const {
        Vector2 delta = center - other.center;
        Vector2 totalExtents = halfExtents + other.halfExtents;
        return std::abs(delta.x) <= totalExtents.x && std::abs(delta.y) <= totalExtents.y;
    }

    bool Contains(const Vector2& point) const {
        Vector2 min = Min();
        Vector2 max = Max();
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
    }
};