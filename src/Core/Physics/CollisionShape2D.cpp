#include "CollisionShape2D.h"
#include <algorithm>
#include <cmath>

CollisionShape2D CollisionShape2D::MakeBox(const Vector2& halfExtents, const Vector2& offset) {
    CollisionShape2D shape;
    shape.m_Type = Type::Box;
    shape.m_HalfExtents = halfExtents;
    shape.m_Offset = offset;
    return shape;
}

CollisionShape2D CollisionShape2D::MakeCircle(float radius, const Vector2& offset) {
    CollisionShape2D shape;
    shape.m_Type = Type::Circle;
    shape.m_Radius = radius;
    shape.m_Offset = offset;
    return shape;
}

AABB CollisionShape2D::GetWorldAABB(const Transform2D& ownerTransform) const {
    Vector2 worldCenter = ownerTransform.position + m_Offset;
    Vector2 halfExtents = (m_Type == Type::Box) ? m_HalfExtents : Vector2(m_Radius, m_Radius);
    return AABB{worldCenter, halfExtents};
}

namespace {

bool CircleCircle(const Vector2& centerA, float radiusA, const Vector2& centerB, float radiusB) {
    float radiusSum = radiusA + radiusB;
    return (centerA - centerB).LengthSquared() <= radiusSum * radiusSum;
}

bool BoxCircle(const Vector2& boxCenter, const Vector2& boxHalfExtents, const Vector2& circleCenter, float radius) {
    Vector2 min = boxCenter - boxHalfExtents;
    Vector2 max = boxCenter + boxHalfExtents;

    Vector2 closest(
        std::clamp(circleCenter.x, min.x, max.x),
        std::clamp(circleCenter.y, min.y, max.y)
    );

    return (closest - circleCenter).LengthSquared() <= radius * radius;
}

} // namespace

bool CollisionShape2D::Intersects(
    const CollisionShape2D& a, const Transform2D& transformA,
    const CollisionShape2D& b, const Transform2D& transformB)
{
    Vector2 centerA = transformA.position + a.m_Offset;
    Vector2 centerB = transformB.position + b.m_Offset;

    if (a.m_Type == Type::Box && b.m_Type == Type::Box) {
        return AABB{centerA, a.m_HalfExtents}.Intersects(AABB{centerB, b.m_HalfExtents});
    }
    if (a.m_Type == Type::Circle && b.m_Type == Type::Circle) {
        return CircleCircle(centerA, a.m_Radius, centerB, b.m_Radius);
    }
    if (a.m_Type == Type::Box && b.m_Type == Type::Circle) {
        return BoxCircle(centerA, a.m_HalfExtents, centerB, b.m_Radius);
    }
    // Remaining case: a is Circle, b is Box.
    return BoxCircle(centerB, b.m_HalfExtents, centerA, a.m_Radius);
}

bool CollisionShape2D::ComputeBoxSeparation(
    const CollisionShape2D& a, const Transform2D& transformA,
    const CollisionShape2D& b, const Transform2D& transformB,
    Vector2& outCorrection)
{
    Vector2 centerA = transformA.position + a.m_Offset;
    Vector2 centerB = transformB.position + b.m_Offset;

    Vector2 delta = centerA - centerB;
    Vector2 totalExtents = a.m_HalfExtents + b.m_HalfExtents;

    float overlapX = totalExtents.x - std::abs(delta.x);
    float overlapY = totalExtents.y - std::abs(delta.y);

    if (overlapX <= 0.0f || overlapY <= 0.0f) {
        outCorrection = Vector2::Zero();
        return false;
    }

    // Push out along whichever axis has the smaller overlap -- that's the
    // shortest path that separates the two boxes.
    if (overlapX < overlapY) {
        outCorrection = Vector2(delta.x < 0.0f ? -overlapX : overlapX, 0.0f);
    } else {
        outCorrection = Vector2(0.0f, delta.y < 0.0f ? -overlapY : overlapY);
    }
    return true;
}