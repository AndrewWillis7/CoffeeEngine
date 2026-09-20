#pragma once
#include "Core/Math/Color.h"
#include "Core/Math/Vector2.h"

class Shader;

// Decoration pushed onto the RoundedPanel shader before a widget draws.
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

    void ApplyTo(Shader* shader) const;
};

namespace UIStyles {
    inline UIStyle Panel()  { UIStyle s; s.cornerRadius = 10.0f; s.shadowBlur = 16.0f; s.shadowOffset = {0.0f, 4.0f}; return s; }
    inline UIStyle Button() { UIStyle s; s.cornerRadius = 4.0f;  s.shadowBlur = 4.0f;  s.shadowOffset = {0.0f, 2.0f}; return s; }
}

// Every color the debug UI draws with, in one place. Dark-IDE palette: near
// black chrome, one blue accent, and a distinct hue per actor capability so a
// long explorer tree can be skimmed by color instead of read line by line.
namespace UITheme {
    inline constexpr Color PanelBg     {0.055f, 0.060f, 0.075f, 0.97f};
    inline constexpr Color PanelBorder {0.22f,  0.24f,  0.30f,  1.0f};
    inline constexpr Color TitleBg     {0.10f,  0.11f,  0.14f,  1.0f};
    inline constexpr Color SectionBg   {0.085f, 0.092f, 0.115f, 1.0f};
    inline constexpr Color RowHover    {0.16f,  0.18f,  0.23f,  1.0f};
    inline constexpr Color RowSelected {0.14f,  0.28f,  0.46f,  1.0f};
    inline constexpr Color Accent      {0.28f,  0.62f,  0.98f,  1.0f};
    inline constexpr Color AccentDim   {0.18f,  0.38f,  0.60f,  1.0f};

    inline constexpr Color Text        {0.86f,  0.88f,  0.92f,  1.0f};
    inline constexpr Color TextDim     {0.52f,  0.56f,  0.64f,  1.0f};
    inline constexpr Color TextFaint   {0.34f,  0.37f,  0.44f,  1.0f};
    inline constexpr Color Value       {0.62f,  0.82f,  0.98f,  1.0f};

    inline constexpr Color Good        {0.42f,  0.84f,  0.48f,  1.0f};
    inline constexpr Color Warn        {0.96f,  0.74f,  0.28f,  1.0f};
    inline constexpr Color Bad         {0.95f,  0.38f,  0.38f,  1.0f};

    inline constexpr Color Guide       {0.20f,  0.22f,  0.28f,  1.0f};
    inline constexpr Color TrackBg     {0.13f,  0.14f,  0.18f,  1.0f};

    // One per SceneExplorer::Kind, in the order that enum declares them.
    inline constexpr Color KindPlayer   {0.45f, 0.86f, 0.52f, 1.0f};
    inline constexpr Color KindActor    {0.36f, 0.78f, 0.72f, 1.0f};
    inline constexpr Color KindCamera   {0.40f, 0.86f, 0.90f, 1.0f};
    inline constexpr Color KindLight    {0.99f, 0.76f, 0.34f, 1.0f};
    inline constexpr Color KindTerrain  {0.72f, 0.60f, 0.36f, 1.0f};
    inline constexpr Color KindCollider {0.80f, 0.56f, 0.90f, 1.0f};
    inline constexpr Color KindSprite   {0.58f, 0.78f, 0.98f, 1.0f};
    inline constexpr Color KindPlain    {0.45f, 0.48f, 0.56f, 1.0f};

    // World-space gizmos.
    inline constexpr Color GizmoBounds   {0.35f, 0.70f, 1.00f, 0.85f};
    inline constexpr Color GizmoCollider {0.35f, 0.95f, 0.45f, 0.85f};
    inline constexpr Color GizmoLight    {1.00f, 0.72f, 0.25f, 0.55f};
    inline constexpr Color GizmoVelocity {1.00f, 0.92f, 0.30f, 0.90f};
    inline constexpr Color GizmoSurface  {0.95f, 0.45f, 0.85f, 0.85f};
    inline constexpr Color GizmoSelected {1.00f, 1.00f, 1.00f, 0.95f};
}
