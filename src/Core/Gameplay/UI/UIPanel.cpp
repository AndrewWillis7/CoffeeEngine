#include "UIPanel.h"
#include "UIStyle.h"
#include "Core/ActorRegistry.h"
#include "Core/Input/UserInputService.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Texture.h"
#include "Renderer/Font.h"
#include <algorithm>
#include <GL/gl.h> // glScissor/GL_SCISSOR_TEST -- GL1.0, no GLLoader entry needed

namespace {
    UIStyle g_PanelStyle  = UIStyles::Panel();
    UIStyle g_WidgetStyle = UIStyles::Button();
}

UIPanel::UIPanel(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input)
    : m_Actors(actors), m_Renderer(renderer), m_Input(input), m_Font(std::make_unique<Font>()) {}

UIPanel::~UIPanel() = default;
    
void UIPanel::NewFrame(int windowHeight) {
    m_Items.clear();
    m_WindowHeight = windowHeight;
    m_PanelHeight = static_cast<float>(windowHeight) - 2.0f * kMargin;

    m_CursorY = kMargin + kPadding;
    m_LineBottom = m_CursorY;
    m_PrevRight = kX + kPadding;
    m_SameLineQueued = false;

    m_MousePos = m_Input.GetMousePosition();
    m_MousePressedThisFrame = m_Input.IsMouseButtonPressed(MouseButton::Left);

    if (MouseOver({kX, kMargin}, {kWidth, m_PanelHeight})) {
        m_ScrollOffset -= m_Input.GetScrollDelta() * kScrollSpeed;
    }
    m_ScrollOffset = std::max(m_ScrollOffset, 0.0f);
}

bool UIPanel::MouseOver(Vector2 pos, Vector2 size) const {
    return m_MousePos.x >= pos.x && m_MousePos.x <= pos.x + size.x &&
           m_MousePos.y >= pos.y && m_MousePos.y <= pos.y + size.y;
}

void UIPanel::SameLine() {
    m_SameLineQueued = true;
}

Vector2 UIPanel::PlaceWidget(float width, float height) {
    float x, y;
    if (m_SameLineQueued) {
        x = m_PrevRight + kPadding * 0.5f;
        y = m_CursorY;
        m_SameLineQueued = false;
    } else {
        m_CursorY = m_LineBottom;
        x = kX + kPadding;
        y = m_CursorY;
    }
    m_PrevRight = x + width;
    m_LineBottom = std::max(m_LineBottom, y + height + kPadding * 0.5f);
    return {x, y};
}

bool UIPanel::Button(const std::string& label) {
    float x = m_SameLineQueued ? (m_PrevRight + kPadding * 0.5f) : (kX + kPadding);
    float width = (kX + kWidth) - kPadding - x;
    Vector2 pos = PlaceWidget(width, kRowHeight - 6.0f);
    Vector2 size{width, kRowHeight - 6.0f};

    bool hovered = MouseOver(pos, size);
    bool clicked = hovered && m_MousePressedThisFrame;
    Color color = clicked ? g_WidgetStyle.fillActive : hovered ? g_WidgetStyle.fillHover : g_WidgetStyle.fill;

    m_Items.push_back({Item::Kind::Rect, pos, size, color, nullptr, label});
    return clicked;
}

bool UIPanel::Toggle(const std::string& label, bool* value) {
    float x = m_SameLineQueued ? (m_PrevRight + kPadding * 0.5f) : (kX + kPadding);
    float width = (kX + kWidth) - kPadding - x;
    Vector2 pos = PlaceWidget(width, kRowHeight - 6.0f);
    Vector2 size{width, kRowHeight - 6.0f};

    bool hovered = MouseOver(pos, size);
    bool changed = false;
    if (hovered && m_MousePressedThisFrame) {
        *value = !*value;
        changed = true;
    }

    Color color = *value ? g_WidgetStyle.fillActive : g_WidgetStyle.fill;
    m_Items.push_back({Item::Kind::Rect, pos, size, color, nullptr, label + (*value ? " [on]" : " [off]")});
    return changed;
}

void UIPanel::Label(const std::string& text) {
    float width = std::clamp(text.size() * (kGlyphDrawSize + kGlyphSpacing) + 4.0f, 20.0f, kWidth - 2.0f * kPadding);
    Vector2 pos = PlaceWidget(width, kRowHeight - 6.0f);
    m_Items.push_back({Item::Kind::Rect, pos, {width, kRowHeight - 6.0f}, Color::Transparent(), nullptr, text});
}

void UIPanel::Image(Texture* texture, float width, float height) {
    Vector2 pos = PlaceWidget(width, height);
    m_Items.push_back({Item::Kind::Image, pos, {width, height}, Color::White(), texture, ""});
}

void UIPanel::TextBlock(const std::vector<std::string>& lines) {
    const float lineHeight = kGlyphDrawSize + 4.0f;
    const float blockPadding = 6.0f;
    float width = kWidth - 2.0f * kPadding;
    float height = std::max<size_t>(lines.size(), 1) * lineHeight + 2.0f * blockPadding;

    Vector2 pos = PlaceWidget(width, height);

    // Recessed section background so this reads as a distinct block under
    // whatever's above it (the reload button).
    m_Items.push_back({Item::Kind::Rect, pos, {width, height}, Color{0.10f, 0.10f, 0.13f, 1.0f}, nullptr, ""});

    for (size_t i = 0; i < lines.size(); ++i) {
        Vector2 linePos{pos.x + blockPadding, pos.y + blockPadding + i * lineHeight};
        m_Items.push_back({Item::Kind::Rect, linePos, {width - 2.0f * blockPadding, lineHeight}, Color::Transparent(), nullptr, lines[i]});
    }
}

void UIPanel::DrawGlyphString(const std::string& text, Vector2 pos, const Color& color) {
    if (!m_Font || !m_Font->IsValid()) return;
    Shader* textShader = m_Actors.GetOrCreateNamedShader("Text");
    if (!textShader) return;

    float x = pos.x;
    for (char c : text) {
        Font::GlyphUV uv = m_Font->GetGlyphUV(static_cast<unsigned char>(c));
        Vector2 glyphCenter{x + kGlyphDrawSize * 0.5f, pos.y + kGlyphDrawSize * 0.5f};
        m_Renderer.DrawTexturedQuad({glyphCenter, 0.0f}, {kGlyphDrawSize, kGlyphDrawSize}, color,
                                     textShader, m_Font->GetAtlas(), uv.offset, uv.scale);
        x += kGlyphDrawSize + kGlyphSpacing;
    }
}

void UIPanel::Draw() {
    float contentHeight = m_LineBottom;
    float maxScroll = std::max(0.0f, contentHeight - m_PanelHeight);
    m_ScrollOffset = std::min(m_ScrollOffset, maxScroll);

    Shader* panelShader = m_Actors.GetOrCreateNamedShader("RoundedPanel");
    g_PanelStyle.ApplyTo(panelShader);
    m_Renderer.DrawQuad(
        {{kX + kWidth * 0.5f, kMargin + m_PanelHeight * 0.5f}, 0.0f},
        {kWidth, m_PanelHeight}, g_PanelStyle.fill, panelShader);

    glEnable(GL_SCISSOR_TEST);
    int scissorY = m_WindowHeight - static_cast<int>(kMargin + m_PanelHeight);
    glScissor(static_cast<int>(kX), scissorY, static_cast<int>(kWidth), static_cast<int>(m_PanelHeight));

    Shader* rectShader = m_Actors.GetOrCreateNamedShader("RoundedPanel");
    Shader* texShader = m_Actors.GetOrCreateNamedShader("Textured");
    g_WidgetStyle.ApplyTo(rectShader);

    for (auto& item : m_Items) {
        float y = item.pos.y - m_ScrollOffset;
        Vector2 drawCenter{item.pos.x + item.size.x * 0.5f, y + item.size.y * 0.5f};

        if (y + item.size.y < kMargin || y > kMargin + m_PanelHeight)
            continue;

        if (item.kind == Item::Kind::Image) {
            m_Renderer.DrawTexturedQuad({drawCenter, 0.0f}, item.size, item.color, texShader, item.texture);
            continue;
        }

        if (item.color.a > 0.0f) {
            m_Renderer.DrawQuad({drawCenter, 0.0f}, item.size, item.color, rectShader);
        }

        if (!item.text.empty()) {
            Vector2 textPos{item.pos.x + 6.0f, y + (item.size.y - kGlyphDrawSize) * 0.5f};
            DrawGlyphString(item.text, textPos, Color::White());
        }
    }

    glDisable(GL_SCISSOR_TEST);
}