#pragma once
#include <cmath>

// 2D Vector --> position, size, direction, velocity, pretty much anything with x,y
// This one math type is a helper for transform and Rigidbody, AABB, Actor, Sprite, what have you
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    Vector2() = default;
    Vector2(float x_, float y_) : x(x_), y(y_) {}

    // Arithmetic
    Vector2 operator+(const Vector2& other) const {return {x + other.x, y + other.y};}
    Vector2 operator-(const Vector2& other) const {return {x - other.x, y - other.y};}
    Vector2 operator*(float scalar) const {return {x * scalar, y * scalar};}
    Vector2 operator*(const Vector2& other) const {return {x * other.x, y * other.y};}
    Vector2 operator/(float scalar) const {return {x / scalar, y / scalar};}
    Vector2 operator-() const {return {-x, -y};}

    Vector2& operator+=(const Vector2& other) { x += other.x; y += other.y; return *this; }
    Vector2& operator-=(const Vector2& other) { x -= other.x; y -= other.y; return *this; }
    Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }

    bool operator==(const Vector2& other) const {return x == other.x && y == other.y;}
    bool operator!=(const Vector2& other) const {return !(*this == other);}

    // Metrics
    float LengthSquared() const { return x * x + y * y;}
    float Length() const { return std::sqrt(LengthSquared());}

    Vector2 Normalized() const {
        float len = Length();
        return len > 0.0f ? Vector2{x / len, y / len} : Vector2{0.0f, 0.0f};
    }

    float Dot(const Vector2& other) const { return x * other.x + y * other.y;}

    // Rotates this vector by RADIANS (COUNTER-CLOCKWISE)
    Vector2 Rotated(float radians) const {
        float c = std::cos(radians);
        float s = std::sin(radians);
        return {x * c - y * s, x * s + y * c};
    }

    // Statistics
    static float Distance(const Vector2& a, const Vector2& b) {return (a - b).Length();}
    static Vector2 Lerp(const Vector2& a, const Vector2& b, float t) { return a + (b - a) * t;}

    static Vector2 Zero() {return {0.0f, 0.0f};}
    static Vector2 One() {return {1.0f, 1.0f};}

    // Screen Space Coordinates
    // "up" is -y
    static Vector2 Up() {return {0.0f, -1.0f};}
    static Vector2 Down() {return {0.0f, 1.0f};}
    static Vector2 Left() {return {-1.0f, 0.0f};}
    static Vector2 Right() {return {1.0f, 0.0f};}

};

inline Vector2 operator*(float scalar, const Vector2& v) {return v * scalar;}