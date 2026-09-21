#pragma once
#include "../Math/Vector2.h"
#include "../Math/Transform2D.h"
#include "../Math/AABB.h"

// A collider attached to a RigidBody2D, owned by ActorRegistry.
// Offset/size are in the owning body's local space; pass the body's transform
// in to get world-space results. Rotation is ignored -- these are axis-aligned.
class CollisionShape2D {
public:
    enum class Type { Box, Circle };

    static CollisionShape2D MakeBox(const Vector2& halfExtents, const Vector2& offset = Vector2::Zero());
    static CollisionShape2D MakeCircle(float radius, const Vector2& offset = Vector2::Zero());

    Type GetType() const { return m_Type; }
    Vector2 GetOffset() const { return m_Offset; }
    Vector2 GetHalfExtents() const { return m_HalfExtents; } // Box only
    float GetRadius() const { return m_Radius; }             // Circle only

    // Reshaping in place, for the scene editor: nothing at runtime resizes a
    // collider, but an editor dragging one around must not have to swap the
    // pointer every body holds. Sizes clamp at zero; a negative extent would
    // make every overlap test pass the wrong way round.
    void SetType(Type type) { m_Type = type; }
    void SetOffset(const Vector2& offset) { m_Offset = offset; }
    void SetHalfExtents(const Vector2& halfExtents) {
        m_HalfExtents = Vector2(halfExtents.x > 0.0f ? halfExtents.x : 0.0f,
                                halfExtents.y > 0.0f ? halfExtents.y : 0.0f);
    }
    void SetRadius(float radius) { m_Radius = radius > 0.0f ? radius : 0.0f; }

    // Conservative world-space AABB (a circle's is its bounding square).
    AABB GetWorldAABB(const Transform2D& ownerTransform) const;

    // Narrow-phase overlap: Box-Box, Circle-Circle, Box-Circle.
    static bool Intersects(
        const CollisionShape2D& a, const Transform2D& transformA,
        const CollisionShape2D& b, const Transform2D& transformB);

    // Minimum-translation-vector separation, BOX shapes only. False (and a zero
    // correction) when they don't overlap; otherwise outCorrection is how far to
    // move `a` clear, which ResolveCollisionWith then splits by mass.
    static bool ComputeBoxSeparation(
        const CollisionShape2D& a, const Transform2D& transformA,
        const CollisionShape2D& b, const Transform2D& transformB,
        Vector2& outCorrection);

private:
    Type m_Type = Type::Box;
    Vector2 m_Offset;
    Vector2 m_HalfExtents = Vector2(25.0f, 25.0f); // Box
    float m_Radius = 25.0f;                        // Circle
};