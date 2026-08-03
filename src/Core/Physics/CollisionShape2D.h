// src/Core/Physics/CollisionShape2D.h
#pragma once
#include "../Math/Vector2.h"
#include "../Math/Transform2D.h"
#include "../Math/AABB.h"

// A collider attached to a RigidBody2D. Owned by ActorRegistry, same
// lifetime convention as Shader -- RigidBody2D only holds a non-owning
// pointer to it.
//
// Offset/size are defined in the OWNING BODY'S LOCAL space; pass the body's
// Transform2D into GetWorldAABB/Intersects to get world-space results.
// Rotation is ignored for collision purposes, same simplification AABB.h
// already makes -- these are axis-aligned shapes, not oriented ones.
class CollisionShape2D {
public:
    enum class Type { Box, Circle };

    static CollisionShape2D MakeBox(const Vector2& halfExtents, const Vector2& offset = Vector2::Zero());
    static CollisionShape2D MakeCircle(float radius, const Vector2& offset = Vector2::Zero());

    Type GetType() const { return m_Type; }
    Vector2 GetOffset() const { return m_Offset; }
    Vector2 GetHalfExtents() const { return m_HalfExtents; } // Box only
    float GetRadius() const { return m_Radius; }             // Circle only

    // Conservative world-space AABB. Useful for broad-phase checks or debug
    // drawing regardless of the shape's real type (a circle's AABB is just
    // its bounding square).
    AABB GetWorldAABB(const Transform2D& ownerTransform) const;

    // Narrow-phase overlap test between two shapes, given both owners'
    // transforms. Handles Box-Box, Circle-Circle, and Box-Circle.
    static bool Intersects(
        const CollisionShape2D& a, const Transform2D& transformA,
        const CollisionShape2D& b, const Transform2D& transformB);

    // Minimum-translation-vector separation for two BOX shapes only (Circle
    // resolution isn't implemented yet). Returns false and leaves
    // outCorrection at zero if they aren't overlapping. Otherwise
    // outCorrection is how far (and which way) to move `a` so the boxes no
    // longer overlap -- RigidBody2D::ResolveCollisionWith splits this
    // between the two bodies based on mass.
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