// File: src/Core/Gameplay/UI/UIPanel.h
#pragma once
#include "Core/Math/Vector2.h"
#include "Core/Math/Color.h"
#include <string>
#include <vector>
#include <memory>

class ActorRegistry;
class Renderer2D;
class UserInputService;
class Shader;
class Texture;
class Font;

// Immediate-mode, left-docked, scrollable UI panel. Usage per frame:
//
//   panel.NewFrame(window->GetHeight());
//   panel.Label("Actors: 4");
//   panel.SameLine();
//   if (panel.Button("Refresh")) { ... }
//   static bool showColliders = false;
//   panel.Toggle("Show Colliders", &showColliders);
//   panel.Image(iconTexture, 32, 32);
//   panel.Draw();
//
// No retained widget tree -- state you care about lives in your own code
// (the bool* for a toggle), not inside this class. Scroll with the mouse
// wheel while hovering the panel.
class UIPanel {
public:
    UIPanel(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input);
    ~UIPanel();

    void NewFrame(int windowHeight);

    bool Button(const std::string& label);
    bool Toggle(const std::string& label, bool* value);
    void Label(const std::string& text);
    void Image(Texture* texture, float width, float height);

    // Keeps the NEXT widget beside the previous one instead of below it.
    void SameLine();

    // Issues the actual draw calls -- call once, after all widgets for the frame.
    void Draw();

    void TextBlock(const std::vector<std::string>& lines);

private:
    struct Item {
        enum class Kind { Rect, Image } kind;
        Vector2 pos, size;
        Color color;
        Texture* texture = nullptr;
        std::string text; 
    };

    Vector2 PlaceWidget(float width, float height);
    bool MouseOver(Vector2 pos, Vector2 size) const;

    ActorRegistry& m_Actors;
    Renderer2D& m_Renderer;
    UserInputService& m_Input;

    static constexpr float kX = 10.0f;
    static constexpr float kMargin = 10.0f;
    static constexpr float kWidth = 260.0f;
    static constexpr float kRowHeight = 28.0f;
    static constexpr float kPadding = 10.0f;
    static constexpr float kScrollSpeed = 40.0f;

    int m_WindowHeight = 600;
    float m_PanelHeight = 400.0f;

    float m_CursorY = 0.0f;
    float m_LineBottom = 0.0f;
    float m_PrevRight = 0.0f;
    bool m_SameLineQueued = false;

    float m_ScrollOffset = 0.0f;

    Vector2 m_MousePos;
    bool m_MousePressedThisFrame = false;

    std::vector<Item> m_Items;

    void DrawGlyphString(const std::string& text, Vector2 pos, const Color& color);
    std::unique_ptr<Font> m_Font;
    static constexpr float kGlyphDrawSize = 12.0f;
    static constexpr float kGlyphSpacing = 1.0f;
};