#pragma once

// RGBA, channels in [0,1] to match glColor4f.
struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;

    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}

    static Color White() {return {1.0f, 1.0f, 1.0f, 1.0f};}
    static Color Black() {return {0.0f, 0.0f, 0.0f, 1.0f};}
    static Color Red() {return {1.0f, 0.0f, 0.0f, 1.0f};}
    static Color Green() {return {0.0f, 1.0f, 0.0f, 1.0f};}
    static Color Blue() {return {0.0f, 0.0f, 1.0f, 1.0f};}
    static Color Transparent() {return {0.0f, 0.0f, 0.0f, 0.0f};}
};