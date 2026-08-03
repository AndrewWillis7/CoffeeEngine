#pragma once
#include "Core/Math/Color.h"
#include "Core/Math/Vector2.h"

class Shader;

struct UIStyle {
    Color fill         = {0.16f, 0.16f, 0.20f, 1.0f};
    Color fillHover    = {0.22f, 0.22f, 0.28f, 1.0f};
    Color fillActive   = {0.30f, 0.55f, 0.90f, 1.0f};
    Color border       = {0.35f, 0.35f, 0.42f, 1.0f};
    float borderWidth  = 1.0f;
    float cornerRadius = 6.0f;
    Color shadowColor  = {0.0f, 0.0f, 0.0f, 0.35f};
    Vector2 shadowOffset = {0.0f, 3.0f};
    float shadowBlur   = 8.0f;

    // Pushes the decorative uniforms onto whateer shader is anout to draw a widget
    void ApplyTo(Shader* shader) const;
};

namespace UIStyles {
    inline UIStyle Panel()  { UIStyle s; s.cornerRadius = 10.0f; s.shadowBlur = 16.0f; s.shadowOffset = {0.0f, 4.0f}; return s; }
    inline UIStyle Button() { UIStyle s; s.cornerRadius = 4.0f;  s.shadowBlur = 4.0f;  s.shadowOffset = {0.0f, 2.0f}; return s; }
}