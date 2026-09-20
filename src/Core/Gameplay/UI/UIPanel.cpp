#include "UIPanel.h"
#include "Core/ActorRegistry.h"
#include "Core/Input/UserInputService.h"
#include "Renderer/Texture.h"
#include "Renderer/Font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <GL/gl.h> // glScissor/GL_SCISSOR_TEST -- GL1.0, no GLLoader entry needed

namespace {

std::string FormatFloat(float value, int decimals) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    return buffer;
}

// Faint wash behind every other row. Alpha only, so it tints whatever the
// section background happens to be instead of fighting it.
constexpr Color kZebra{1.0f, 1.0f, 1.0f, 0.022f};

} // namespace

UIPanel::UIPanel(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input)
    : m_Actors(actors), m_Renderer(renderer), m_Input(input), m_Font(std::make_unique<Font>()) {}

UIPanel::~UIPanel() = default;

void UIPanel::SetUIScale(float scale) {
    float next = scale >= 2.0f ? 2.0f : 1.0f;
    if (next == m_Scale) return;
    m_Width *= next / m_Scale;
    m_Scroll *= next / m_Scale;
    m_Scale = next;
}

int UIPanel::CharsThatFit(float width) const {
    return width > 0.0f ? static_cast<int>(width / Advance()) : 0;
}

std::string UIPanel::Fit(const std::string& text, int maxChars) {
    if (maxChars <= 0) return {};
    if (static_cast<int>(text.size()) <= maxChars) return text;
    if (maxChars <= 2) return text.substr(0, static_cast<size_t>(maxChars));
    return text.substr(0, static_cast<size_t>(maxChars) - 2) + "..";
}

void UIPanel::NewFrame(int windowWidth, int windowHeight) {
    m_Flats.clear();
    m_Rounded.clear();
    m_Glyphs.clear();
    m_Images.clear();
    m_TitleFlats = 0;
    m_TitleGlyphs = 0;

    m_WindowWidth = windowWidth;
    m_WindowHeight = windowHeight;

    m_X = S(kMargin);
    m_Y = S(kMargin);
    m_Height = std::max(static_cast<float>(windowHeight) - 2.0f * S(kMargin), S(kTitleH) + RowHeight());

    float widthCap = std::min(S(kMaxWidth), static_cast<float>(windowWidth) - 2.0f * S(kMargin));
    float widthFloor = std::min(S(kMinWidth), widthCap);

    m_ContentTop = m_Y + S(kTitleH);
    m_ContentH = std::max(m_Height - S(kTitleH) - S(kPad), RowHeight());

    m_Mouse = m_Input.GetMousePosition();
    m_MouseDown = m_Input.IsMouseButtonDown(MouseButton::Left);
    m_MousePressed = m_Input.IsMouseButtonPressed(MouseButton::Left);

    // Releasing anywhere ends the drag, including outside the panel.
    if (!m_MouseDown) m_ActiveId = 0;

    // Resize grip: a narrow strip straddling the right edge. Handled before the
    // width is used for anything else, so the rest of the frame lays out at the
    // size the cursor is asking for rather than lagging it by a frame.
    float gripLeft = m_X + m_Width - S(kGripW);
    bool overGrip = m_Mouse.x >= gripLeft && m_Mouse.x <= m_X + m_Width + S(kGripW) &&
                    m_Mouse.y >= m_Y && m_Mouse.y <= m_Y + m_Height;
    if (overGrip && m_MousePressed) m_ActiveId = kResizeId;
    if (m_ActiveId == kResizeId) m_Width = m_Mouse.x - m_X;
    m_Width = std::clamp(m_Width, widthFloor, widthCap);

    m_MouseInPanel = m_Mouse.x >= m_X && m_Mouse.x <= m_X + m_Width + S(kGripW) &&
                     m_Mouse.y >= m_Y && m_Mouse.y <= m_Y + m_Height;

    float maxScroll = std::max(0.0f, m_LastContentHeight - m_ContentH);

    float barX = ContentRight() + S(2.0f);
    bool overBar = m_Mouse.x >= barX && m_Mouse.x <= barX + S(kScrollBarW) &&
                   m_Mouse.y >= m_ContentTop && m_Mouse.y <= m_ContentTop + m_ContentH;
    if (overBar && m_MousePressed && maxScroll > 0.0f) m_ActiveId = kScrollBarId;

    if (m_ActiveId == kScrollBarId && maxScroll > 0.0f) {
        float handle = std::max(m_ContentH * (m_ContentH / m_LastContentHeight), S(20.0f));
        float travel = m_ContentH - handle;
        if (travel > 0.0f) {
            float t = (m_Mouse.y - m_ContentTop - handle * 0.5f) / travel;
            m_Scroll = std::clamp(t, 0.0f, 1.0f) * maxScroll;
        }
    } else if (m_MouseInPanel) {
        m_Scroll -= m_Input.GetScrollDelta() * S(kScrollStep);
    }

    // Final for the whole frame, so widgets can cull and hit-test against it as
    // they are emitted. It costs one frame of lag when the content shrinks.
    m_Scroll = std::clamp(m_Scroll, 0.0f, maxScroll);

    m_CursorY = S(kPad);
    m_LineBottom = m_CursorY;
    m_PrevRight = ContentLeft();
    m_SameLine = false;
    m_IndentLevel = 0;
    m_ZebraRow = 0;
    m_WidgetCounter = 0;
}

// --- Layout -----------------------------------------------------------------

void UIPanel::SameLine() { m_SameLine = true; }
void UIPanel::Indent(int levels) { m_IndentLevel += levels; }
void UIPanel::Unindent(int levels) { m_IndentLevel = std::max(0, m_IndentLevel - levels); }

void UIPanel::Spacing(float pixels) {
    m_CursorY = m_LineBottom;
    m_LineBottom += pixels > 0.0f ? S(pixels) : S(4.0f);
}

void UIPanel::Separator() {
    Vector2 size;
    Vector2 pos = Place(0.0f, S(5.0f), size);
    if (!Visible(pos.y, size.y)) return;
    PushRect({pos.x, pos.y + S(2.0f)}, {size.x, S(1.0f)}, UITheme::Guide);
}

Vector2 UIPanel::Place(float width, float height, Vector2& outSize) {
    float x;
    if (m_SameLine) {
        x = m_PrevRight + S(4.0f);
        m_SameLine = false;
    } else {
        m_CursorY = m_LineBottom;
        x = ContentLeft() + static_cast<float>(m_IndentLevel) * S(kIndent);
    }

    float available = std::max(ContentRight() - x, 0.0f);
    if (width <= 0.0f || width > available) width = available;

    m_PrevRight = x + width;
    m_LineBottom = std::max(m_LineBottom, m_CursorY + height + S(3.0f));
    outSize = {width, height};
    return {x, ScreenY(m_CursorY)};
}

bool UIPanel::Visible(float screenY, float height) const {
    return screenY + height >= m_ContentTop && screenY <= m_ContentTop + m_ContentH;
}

bool UIPanel::Hover(const Vector2& pos, const Vector2& size) const {
    // A drag in progress owns the cursor; nothing else may claim it.
    if (m_ActiveId == kScrollBarId || m_ActiveId == kResizeId) return false;
    // Clipped away by the content scissor, so it is not really under the cursor.
    if (m_Mouse.y < m_ContentTop || m_Mouse.y > m_ContentTop + m_ContentH) return false;
    return m_Mouse.x >= pos.x && m_Mouse.x <= pos.x + size.x &&
           m_Mouse.y >= pos.y && m_Mouse.y <= pos.y + size.y;
}

// --- Primitive emitters -----------------------------------------------------

void UIPanel::PushRect(const Vector2& pos, const Vector2& size, const Color& color) {
    if (color.a <= 0.0f || size.x <= 0.0f || size.y <= 0.0f) return;
    m_Flats.push_back({{pos.x + size.x * 0.5f, pos.y + size.y * 0.5f}, size, color, {0.0f, 0.0f}, {1.0f, 1.0f}});
}

void UIPanel::PushRounded(const Vector2& pos, const Vector2& size, const Color& fill,
                          const Color& border, float borderWidth, float corner) {
    if (size.x <= 0.0f || size.y <= 0.0f) return;
    m_Rounded.push_back({pos, size, fill, border, borderWidth, corner});
}

void UIPanel::PushText(const std::string& text, Vector2 pos, const Color& color, float clipRight) {
    if (color.a <= 0.0f || !m_Font || !m_Font->IsValid()) return;

    const float glyph = Glyph();
    const float advance = Advance();
    float x = pos.x;
    for (unsigned char c : text) {
        if (x + glyph > clipRight) break;
        // Blanks still advance but cost no draw -- most of a dense panel is space.
        if (c != ' ') {
            Font::GlyphUV uv = m_Font->GetGlyphUV(c);
            m_Glyphs.push_back({{x + glyph * 0.5f, pos.y + glyph * 0.5f}, {glyph, glyph}, color, uv.offset, uv.scale});
        }
        x += advance;
    }
}

void UIPanel::PushTextRight(const std::string& text, float right, float y, const Color& color, float clipLeft) {
    int maxChars = CharsThatFit(right - clipLeft);
    if (maxChars <= 0) return;
    std::string fitted = Fit(text, maxChars);
    PushText(fitted, {right - static_cast<float>(fitted.size()) * Advance(), y}, color, right + 1.0f);
}

// --- Chrome -----------------------------------------------------------------

void UIPanel::TitleBar(const std::string& title, const std::string& right, const Color& rightColor) {
    const Vector2 pos{m_X, m_Y};
    const Vector2 size{m_Width, S(kTitleH)};

    PushRect(pos, size, UITheme::TitleBg);
    PushRect({m_X, m_Y + size.y - S(1.0f)}, {m_Width, S(1.0f)}, UITheme::PanelBorder);
    PushRect({m_X, m_Y + S(4.0f)}, {S(3.0f), size.y - S(8.0f)}, UITheme::Accent);

    const float textY = m_Y + (size.y - Glyph()) * 0.5f;
    const float leftX = m_X + S(kPad) + S(3.0f);
    PushText(title, {leftX, textY}, UITheme::Text, m_X + m_Width - S(kPad));
    PushTextRight(right, m_X + m_Width - S(kPad), textY, rightColor,
                  leftX + static_cast<float>(title.size() + 1) * Advance());

    // Everything pushed so far belongs to the strip, which draws unclipped and
    // never scrolls; the counts split the batches at Draw() time.
    m_TitleFlats = m_Flats.size();
    m_TitleGlyphs = m_Glyphs.size();
}

// --- Widgets ----------------------------------------------------------------

void UIPanel::Label(const std::string& text, const Color& color) {
    Vector2 size;
    Vector2 pos = Place(0.0f, RowHeight(), size);
    if (!Visible(pos.y, size.y)) return;
    PushText(text, {pos.x, pos.y + (size.y - Glyph()) * 0.5f}, color, pos.x + size.x);
}

void UIPanel::KeyValue(const std::string& key, const std::string& value, const Color& valueColor) {
    Vector2 size;
    Vector2 pos = Place(0.0f, RowHeight(), size);
    const bool zebra = (m_ZebraRow++ & 1) != 0;
    if (!Visible(pos.y, size.y)) return;

    if (zebra) PushRect(pos, size, kZebra);
    const float textY = pos.y + (size.y - Glyph()) * 0.5f;

    // The column splits after the key rather than at a fixed fraction, so a
    // two-character key leaves the whole rest of the row for its value instead
    // of truncating it against an arbitrary midpoint.
    const float keyWidth = static_cast<float>(key.size()) * Advance() + S(8.0f);
    const float split = pos.x + std::clamp(keyWidth, size.x * 0.25f, size.x * 0.62f);

    PushText(key, {pos.x + S(3.0f), textY}, UITheme::TextDim, split);
    PushTextRight(value, pos.x + size.x - S(3.0f), textY, valueColor, split);
}

bool UIPanel::Button(const std::string& label, float width) {
    const int id = ++m_WidgetCounter;
    Vector2 size;
    Vector2 pos = Place(width, RowHeight() + S(4.0f), size);

    const bool hovered = Hover(pos, size);
    const bool clicked = hovered && m_MousePressed;
    if (clicked) m_ActiveId = id;
    const bool held = m_ActiveId == id && m_MouseDown;

    if (Visible(pos.y, size.y)) {
        const Color fill = held ? UITheme::Accent : hovered ? UITheme::RowHover : UITheme::SectionBg;
        PushRounded(pos, size, fill, hovered ? UITheme::Accent : UITheme::PanelBorder, S(1.0f), S(3.0f));

        const float textWidth = static_cast<float>(label.size()) * Advance();
        const float textX = pos.x + std::max(S(6.0f), (size.x - textWidth) * 0.5f);
        PushText(label, {textX, pos.y + (size.y - Glyph()) * 0.5f},
                 held ? Color::White() : UITheme::Text, pos.x + size.x - S(4.0f));
    }
    return clicked;
}

bool UIPanel::Toggle(const std::string& label, bool* value, float width, const Color& accent) {
    Vector2 size;
    Vector2 pos = Place(width, RowHeight() + S(2.0f), size);

    const bool hovered = Hover(pos, size);
    const bool changed = hovered && m_MousePressed;
    if (changed) *value = !*value;

    if (Visible(pos.y, size.y)) {
        if (hovered) PushRect(pos, size, UITheme::RowHover);

        const float box = Glyph();
        const float boxY = pos.y + (size.y - box) * 0.5f;
        PushRounded({pos.x + S(2.0f), boxY}, {box, box},
                    *value ? accent : UITheme::TrackBg,
                    *value ? accent : UITheme::PanelBorder, S(1.0f), S(2.0f));
        if (*value) {
            PushRect({pos.x + S(2.0f) + box * 0.28f, boxY + box * 0.28f},
                     {box * 0.44f, box * 0.44f}, UITheme::PanelBg);
        }
        PushText(label, {pos.x + S(2.0f) + box + S(6.0f), pos.y + (size.y - Glyph()) * 0.5f},
                 *value ? UITheme::Text : UITheme::TextDim, pos.x + size.x);
    }
    return changed;
}

float UIPanel::SliderTrack(const std::string& label, const std::string& valueText,
                           float fraction, int widgetId) {
    Vector2 size;
    Vector2 pos = Place(0.0f, RowHeight() + S(5.0f), size);

    const bool hovered = Hover(pos, size);
    if (hovered && m_MousePressed) m_ActiveId = widgetId;
    const bool active = m_ActiveId == widgetId && m_MouseDown;

    const float trackH = S(4.0f);
    const float trackY = pos.y + size.y - trackH - S(1.0f);
    const float clamped = std::clamp(fraction, 0.0f, 1.0f);

    if (Visible(pos.y, size.y)) {
        const float textY = pos.y + S(1.0f);
        PushText(label, {pos.x + S(2.0f), textY}, (hovered || active) ? UITheme::Text : UITheme::TextDim,
                 pos.x + size.x * 0.62f);
        PushTextRight(valueText, pos.x + size.x - S(2.0f), textY,
                      active ? UITheme::Accent : UITheme::Value, pos.x + size.x * 0.58f);

        PushRect({pos.x, trackY}, {size.x, trackH}, UITheme::TrackBg);
        PushRect({pos.x, trackY}, {size.x * clamped, trackH}, active ? UITheme::Accent : UITheme::AccentDim);

        const float knob = S(5.0f);
        PushRect({pos.x + size.x * clamped - knob * 0.5f, trackY - S(2.0f)}, {knob, trackH + S(4.0f)},
                 active ? Color::White() : UITheme::Accent);
    }

    if (active && size.x > 0.0f) return std::clamp((m_Mouse.x - pos.x) / size.x, 0.0f, 1.0f);
    return -1.0f;
}

bool UIPanel::Slider(const std::string& label, float* value, float minValue, float maxValue, int decimals) {
    const int id = ++m_WidgetCounter;
    const float span = maxValue - minValue;
    const float fraction = span > 0.0f ? (*value - minValue) / span : 0.0f;

    const float requested = SliderTrack(label, FormatFloat(*value, decimals), fraction, id);
    if (requested < 0.0f) return false;

    const float next = minValue + requested * span;
    if (next == *value) return false;
    *value = next;
    return true;
}

bool UIPanel::SliderInt(const std::string& label, int* value, int minValue, int maxValue) {
    const int id = ++m_WidgetCounter;
    const float span = static_cast<float>(maxValue - minValue);
    const float fraction = span > 0.0f ? static_cast<float>(*value - minValue) / span : 0.0f;

    const float requested = SliderTrack(label, std::to_string(*value), fraction, id);
    if (requested < 0.0f) return false;

    const int next = minValue + static_cast<int>(std::lround(requested * span));
    if (next == *value) return false;
    *value = next;
    return true;
}

bool UIPanel::Header(const std::string& label, bool* open, const std::string& right) {
    Vector2 size;
    Vector2 pos = Place(0.0f, RowHeight() + S(5.0f), size);

    const bool hovered = Hover(pos, size);
    if (hovered && m_MousePressed) *open = !*open;

    // Zebra striping restarts per section, so the first row under a header is
    // always the unshaded one however many rows the section above emitted.
    m_ZebraRow = 0;

    if (Visible(pos.y, size.y)) {
        PushRect(pos, size, hovered ? UITheme::RowHover : UITheme::SectionBg);
        PushRect(pos, {S(2.0f), size.y}, *open ? UITheme::Accent : UITheme::AccentDim);

        const float textY = pos.y + (size.y - Glyph()) * 0.5f;
        PushText(*open ? "-" : "+", {pos.x + S(7.0f), textY}, UITheme::Accent, pos.x + size.x);
        PushText(label, {pos.x + S(7.0f) + Advance() * 2.0f, textY}, UITheme::Text, pos.x + size.x - S(6.0f));
        if (!right.empty()) {
            PushTextRight(right, pos.x + size.x - S(6.0f), textY, UITheme::TextDim, pos.x + size.x * 0.5f);
        }
    }
    return *open;
}

UIPanel::RowHit UIPanel::Row(const RowSpec& spec) {
    // Rows span the full content width and take their indent from spec.depth,
    // so a tree lines up no matter what the surrounding section indented to.
    const int savedIndent = m_IndentLevel;
    m_IndentLevel = 0;
    Vector2 size;
    Vector2 pos = Place(0.0f, RowHeight(), size);
    m_IndentLevel = savedIndent;

    const float indent = static_cast<float>(spec.depth) * S(kIndent);
    const float arrowX = pos.x + S(5.0f) + indent;
    const float labelX = arrowX + (spec.hasChildren ? Advance() + S(3.0f) : S(3.0f));

    const bool hovered = Hover(pos, size);
    RowHit hit = RowHit::None;
    if (hovered && m_MousePressed) {
        hit = (spec.hasChildren && m_Mouse.x < labelX) ? RowHit::Expander : RowHit::Body;
    }

    const bool zebra = (m_ZebraRow++ & 1) != 0;

    if (Visible(pos.y, size.y)) {
        if (spec.selected)   PushRect(pos, size, UITheme::RowSelected);
        else if (hovered)    PushRect(pos, size, UITheme::RowHover);
        else if (zebra)      PushRect(pos, size, kZebra);

        // One rail per ancestor level, so a deep row stays traceable upward.
        for (int d = 0; d < spec.depth; ++d) {
            PushRect({pos.x + S(6.0f) + static_cast<float>(d) * S(kIndent), pos.y}, {S(1.0f), size.y}, UITheme::Guide);
        }

        if (spec.accent.a > 0.0f) PushRect(pos, {S(2.0f), size.y}, spec.accent);

        const float textY = pos.y + (size.y - Glyph()) * 0.5f;
        if (spec.hasChildren) {
            PushText(spec.expanded ? "v" : ">", {arrowX, textY},
                     hovered ? UITheme::Text : UITheme::TextDim, arrowX + Advance() + 1.0f);
        }

        const float valueWidth = spec.value.empty()
            ? 0.0f
            : static_cast<float>(spec.value.size() + 1) * Advance();
        const float labelClip = pos.x + size.x - S(4.0f) - valueWidth;

        PushText(spec.label, {labelX, textY}, spec.labelColor, labelClip);
        if (!spec.value.empty()) {
            PushTextRight(spec.value, pos.x + size.x - S(4.0f), textY, spec.valueColor, labelClip);
        }
    }
    return hit;
}

void UIPanel::Bar(const std::string& label, float fraction, const Color& color) {
    Vector2 size;
    Vector2 pos = Place(0.0f, RowHeight(), size);
    if (!Visible(pos.y, size.y)) return;

    PushRect(pos, size, UITheme::TrackBg);
    // Dimmed fill under full-strength text: a solid grade color behind white
    // glyphs is unreadable the moment the grade goes amber.
    PushRect(pos, {size.x * std::clamp(fraction, 0.0f, 1.0f), size.y},
             {color.r, color.g, color.b, color.a * 0.40f});
    PushText(label, {pos.x + S(4.0f), pos.y + (size.y - Glyph()) * 0.5f}, color, pos.x + size.x - S(2.0f));
}

void UIPanel::Graph(const float* ring, int count, int oldest, float maxValue,
                    float height, float warn, float bad) {
    Vector2 size;
    Vector2 pos = Place(0.0f, S(height), size);
    if (!Visible(pos.y, size.y) || !ring || count <= 0 || maxValue <= 0.0f) return;

    PushRect(pos, size, UITheme::TrackBg);

    auto plotY = [&](float value) {
        return pos.y + size.y - (std::min(value, maxValue) / maxValue) * size.y;
    };
    PushRect({pos.x, plotY(warn)}, {size.x, S(1.0f)}, {1.0f, 1.0f, 1.0f, 0.10f});
    PushRect({pos.x, plotY(bad)}, {size.x, S(1.0f)}, {1.0f, 0.4f, 0.4f, 0.18f});

    const float barWidth = size.x / static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
        const float value = ring[(oldest + i) % count];
        if (value <= 0.0f) continue;
        const float barHeight = std::min(value / maxValue, 1.0f) * size.y;
        const Color color = value >= bad ? UITheme::Bad : value >= warn ? UITheme::Warn : UITheme::Good;
        PushRect({pos.x + static_cast<float>(i) * barWidth, pos.y + size.y - barHeight},
                 {std::max(barWidth, 1.0f), barHeight}, color);
    }
}

void UIPanel::Image(Texture* texture, float width, float height) {
    Vector2 size;
    Vector2 pos = Place(width, height, size);
    if (!texture || !Visible(pos.y, size.y)) return;
    m_Images.push_back({pos, {width, height}, Color::White(), texture});
}

void UIPanel::TextBlock(const std::vector<std::string>& lines, const Color& color) {
    for (const std::string& line : lines) Label(line, color);
}

// --- Draw -------------------------------------------------------------------

void UIPanel::Draw() {
    m_LastContentHeight = m_LineBottom + S(kPad);

    Shader* roundedShader = m_Actors.GetOrCreateNamedShader("RoundedPanel");
    Shader* textShader = m_Actors.GetOrCreateNamedShader("Text");
    Shader* texturedShader = m_Actors.GetOrCreateNamedShader("Textured");
    Texture* atlas = (m_Font && m_Font->IsValid()) ? m_Font->GetAtlas() : nullptr;

    // Body first and unclipped, so its shadow and border fall outside the
    // scissor the scrolling content runs under.
    UIStyle panelStyle = UIStyles::Panel();
    panelStyle.border = UITheme::PanelBorder;
    panelStyle.cornerRadius = S(8.0f);
    panelStyle.ApplyTo(roundedShader);
    m_Renderer.DrawScreenQuad({{m_X + m_Width * 0.5f, m_Y + m_Height * 0.5f}, 0.0f},
                              {m_Width, m_Height}, UITheme::PanelBg, roundedShader);

    m_Renderer.DrawScreenQuadBatch(m_Flats.data(), m_TitleFlats, nullptr);
    if (atlas) m_Renderer.DrawScreenQuadBatch(m_Glyphs.data(), m_TitleGlyphs, textShader, atlas);

    // glScissor is bottom-left origin; the content rect is top-left.
    const int scissorW = static_cast<int>(m_Width);
    const int scissorH = static_cast<int>(m_ContentH);
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<int>(m_X), m_WindowHeight - static_cast<int>(m_ContentTop) - scissorH,
              std::max(scissorW, 0), std::max(scissorH, 0));

    m_Renderer.DrawScreenQuadBatch(m_Flats.data() + m_TitleFlats, m_Flats.size() - m_TitleFlats, nullptr);

    // Rounded widgets carry per-item corner and border, so they cannot batch.
    // There are only ever a handful: buttons, checkboxes and the panel itself.
    UIStyle widgetStyle = UIStyles::Button();
    widgetStyle.shadowColor = Color::Transparent();
    for (const RoundedItem& item : m_Rounded) {
        widgetStyle.cornerRadius = item.corner;
        widgetStyle.border = item.border;
        widgetStyle.borderWidth = item.borderWidth;
        widgetStyle.ApplyTo(roundedShader);
        m_Renderer.DrawScreenQuad(
            {{item.pos.x + item.size.x * 0.5f, item.pos.y + item.size.y * 0.5f}, 0.0f},
            item.size, item.fill, roundedShader);
    }

    if (atlas) {
        m_Renderer.DrawScreenQuadBatch(m_Glyphs.data() + m_TitleGlyphs, m_Glyphs.size() - m_TitleGlyphs,
                                       textShader, atlas);
    }

    for (const ImageItem& image : m_Images) {
        m_Renderer.DrawScreenTexturedQuad(
            {{image.pos.x + image.size.x * 0.5f, image.pos.y + image.size.y * 0.5f}, 0.0f},
            image.size, image.tint, texturedShader, image.texture);
    }

    glDisable(GL_SCISSOR_TEST);

    const float maxScroll = std::max(0.0f, m_LastContentHeight - m_ContentH);
    if (maxScroll <= 0.0f) return;

    const float barX = ContentRight() + S(2.0f);
    const float handle = std::max(m_ContentH * (m_ContentH / m_LastContentHeight), S(20.0f));
    const float handleY = m_ContentTop + (m_Scroll / maxScroll) * (m_ContentH - handle);

    Renderer2D::ScreenQuad bar[2] = {
        {{barX + S(kScrollBarW) * 0.5f, m_ContentTop + m_ContentH * 0.5f}, {S(kScrollBarW), m_ContentH},
         UITheme::TrackBg, {0.0f, 0.0f}, {1.0f, 1.0f}},
        {{barX + S(kScrollBarW) * 0.5f, handleY + handle * 0.5f}, {S(kScrollBarW), handle},
         m_ActiveId == kScrollBarId ? UITheme::Accent : UITheme::AccentDim, {0.0f, 0.0f}, {1.0f, 1.0f}},
    };
    m_Renderer.DrawScreenQuadBatch(bar, 2, nullptr);
}
