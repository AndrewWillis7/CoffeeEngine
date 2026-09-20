#pragma once

// RGBA, channels in [0,1] to match glColor4f.
struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;

    Color() = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}

    static constexpr Color White() {return {1.0f, 1.0f, 1.0f, 1.0f};}
    static constexpr Color Black() {return {0.0f, 0.0f, 0.0f, 1.0f};}
    static constexpr Color Red() {return {1.0f, 0.0f, 0.0f, 1.0f};}
    static constexpr Color Green() {return {0.0f, 1.0f, 0.0f, 1.0f};}
    static constexpr Color Blue() {return {0.0f, 0.0f, 1.0f, 1.0f};}
    static constexpr Color Transparent() {return {0.0f, 0.0f, 0.0f, 0.0f};}
};