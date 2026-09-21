#pragma once
#include "UIStyle.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Color.h"
#include "Renderer/Renderer2D.h"
#include <string>
#include <vector>
#include <memory>

class ActorRegistry;
class UserInputService;
class Texture;
class Font;

// Immediate-mode, left-docked, scrollable debug panel. No retained widget tree:
// the state a widget edits lives in the caller (the bool* behind a Toggle), so
// sections can be added or removed without bookkeeping anywhere else.
//
//   panel.NewFrame(w, h);
//   panel.TitleBar("Debug", "60 fps", UITheme::Good);
//   if (panel.Header("Performance", &showPerf)) panel.KeyValue("Frame", "16.6 ms");
//   panel.Draw();
//
// Widgets record themselves into four batches -- flat rects, rounded rects,
// glyphs, images -- which Draw() issues in that order, so a row background is
// always under its own text. Items are culled and hit-tested in screen space at
// emit time, which is why the scroll offset is finalised in NewFrame(), against
// last frame's content height, rather than at the end of the frame.
class UIPanel {
public:
    // Which edge NewFrame() docks the panel against. Fixed for the panel's
    // lifetime -- set once at construction, not something a caller flips
    // frame to frame.
    enum class Anchor { Left, Right };

    UIPanel(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input, Anchor anchor = Anchor::Left);
    ~UIPanel();

    // One row of a tree or list: an optional color bar down the left edge, an
    // indent, an expander, a label, and a right-aligned value.
    struct RowSpec {
        std::string label;
        std::string value;
        Color accent      = UITheme::KindPlain; // left bar; alpha 0 draws none
        Color labelColor  = UITheme::Text;
        Color valueColor  = UITheme::TextDim;
        int depth         = 0;
        bool hasChildren  = false;
        bool expanded     = false;
        bool selected     = false;
    };
    enum class RowHit { None, Body, Expander };

    void NewFrame(int windowWidth, int windowHeight);
    void Draw();

    // Fixed strip across the top: never scrolls, never counts toward content
    // height. Call it first or not at all.
    void TitleBar(const std::string& title, const std::string& right, const Color& rightColor);

    // --- Layout -------------------------------------------------------------
    void SameLine();
    void Indent(int levels = 1);
    void Unindent(int levels = 1);
    void Separator();
    void Spacing(float pixels = 0.0f);

    // --- Widgets ------------------------------------------------------------
    void Label(const std::string& text, const Color& color = UITheme::Text);
    void KeyValue(const std::string& key, const std::string& value, const Color& valueColor = UITheme::Value);
    bool Button(const std::string& label, float width = 0.0f);
    // width <= 0 fills the rest of the line. accent colors the checkbox, which
    // is what lets a row of filters carry the color of what each one filters.
    bool Toggle(const std::string& label, bool* value, float width = 0.0f,
                const Color& accent = UITheme::Accent);
    bool Slider(const std::string& label, float* value, float minValue, float maxValue, int decimals = 2);
    bool SliderInt(const std::string& label, int* value, int minValue, int maxValue);

    // Unbounded numeric field: press and drag sideways, `speed` units per
    // screen pixel. For values with no natural range -- a world position --
    // where a Slider would need one invented. The result rounds to `decimals`,
    // so a position edited at 0 decimals stays on whole texels. minValue <
    // maxValue clamps; leave them equal for no bounds. labelColor lets a
    // caller flag the value (the scene editor marks saved edits amber).
    bool DragFloat(const std::string& label, float* value, float speed, int decimals = 1,
                   float width = 0.0f, float minValue = 0.0f, float maxValue = 0.0f,
                   const Color& labelColor = UITheme::TextDim);

    // A value that can be read here but not changed: drawn like a DragFloat
    // field, dimmed, behind a padlock, and inert. Hovering it shows `reason`,
    // so a locked row always says why rather than just refusing.
    void LockedField(const std::string& label, const std::string& value, const std::string& reason,
                     float width = 0.0f);

    // Text shown in a small box by the cursor, over everything, for this frame
    // only. Last call wins; widgets call it while hovered.
    void Tooltip(const std::string& text) { m_Tooltip = text; }

    // The padlock the font carries in an unused control-code slot.
    static constexpr char kLockGlyph = '\x01';

    // Collapsible section bar. Returns *open, so a section body reads as
    // if (panel.Header(...)) { ... }.
    bool Header(const std::string& label, bool* open, const std::string& right = {});

    RowHit Row(const RowSpec& spec);

    void Bar(const std::string& label, float fraction, const Color& color);

    // Ring-buffer plot: sample i, oldest first, is ring[(oldest + i) % count].
    // Bars past warn/bad recolor, and both get a reference line.
    void Graph(const float* ring, int count, int oldest, float maxValue,
               float height, float warn, float bad);

    void Image(Texture* texture, float width, float height);
    void TextBlock(const std::vector<std::string>& lines, const Color& color = UITheme::TextDim);

    // --- Queries and settings -----------------------------------------------
    // True while the cursor is anywhere over the panel, so the caller can stop
    // the scene reacting to the same click.
    bool WantsMouse() const { return m_MouseInPanel; }

    float GetUIScale() const { return m_Scale; }
    void SetUIScale(float scale);

    float GetWidth() const { return m_Width; }
    int CharsThatFit(float width) const;

    // Shortens to at most maxChars, marking the cut with two dots.
    static std::string Fit(const std::string& text, int maxChars);

private:
    struct RoundedItem {
        Vector2 pos, size;
        Color fill, border;
        float borderWidth, corner;
    };
    struct ImageItem {
        Vector2 pos, size;
        Color tint;
        Texture* texture;
    };

    // Base metrics, every one multiplied by m_Scale. The glyph atlas is 8x8, so
    // only whole-number scales keep text on the texel grid.
    static constexpr float kGlyph      = 8.0f;
    static constexpr float kAdvance    = 8.0f;
    static constexpr float kRowPad     = 6.0f;
    static constexpr float kPad        = 7.0f;
    static constexpr float kIndent     = 10.0f;
    static constexpr float kMargin     = 8.0f;
    static constexpr float kTitleH     = 22.0f;
    static constexpr float kScrollBarW = 6.0f;
    static constexpr float kGripW      = 5.0f;
    static constexpr float kScrollStep = 48.0f;
    static constexpr float kMinWidth   = 240.0f;
    static constexpr float kMaxWidth   = 900.0f;

    float S(float base) const { return base * m_Scale; }
    float Glyph() const { return kGlyph * m_Scale; }
    float Advance() const { return kAdvance * m_Scale; }
    float RowHeight() const { return (kGlyph + kRowPad) * m_Scale; }
    float ContentLeft() const { return m_X + S(kPad); }
    float ContentRight() const { return m_X + m_Width - S(kPad) - S(kScrollBarW); }
    float ContentWidth() const { return ContentRight() - ContentLeft(); }

    // Content-local y (unscrolled) to absolute screen y.
    float ScreenY(float localY) const { return m_ContentTop + localY - m_Scroll; }

    // Reserves a widget on the current line and returns its screen-space rect.
    // Width <= 0 means "the rest of the line".
    Vector2 Place(float width, float height, Vector2& outSize);

    bool Visible(float screenY, float height) const;
    bool Hover(const Vector2& pos, const Vector2& size) const;

    void PushRect(const Vector2& pos, const Vector2& size, const Color& color);
    void PushRounded(const Vector2& pos, const Vector2& size, const Color& fill,
                     const Color& border, float borderWidth, float corner);
    void PushText(const std::string& text, Vector2 pos, const Color& color, float clipRight);
    void PushTextRight(const std::string& text, float right, float y, const Color& color, float clipLeft);

    void DrawTooltip(Shader* textShader, Texture* atlas);

    // Shared by Slider and SliderInt: draws the track, and returns the 0..1
    // position the mouse is asking for, or a negative number when it is not
    // dragging this widget.
    float SliderTrack(const std::string& label, const std::string& valueText,
                      float fraction, int widgetId);

    ActorRegistry& m_Actors;
    Renderer2D& m_Renderer;
    UserInputService& m_Input;
    std::unique_ptr<Font> m_Font;
    Anchor m_Anchor;

    float m_Scale = 1.0f;
    float m_X = kMargin, m_Y = kMargin;
    float m_Width = 380.0f;
    float m_Height = 400.0f;
    float m_ContentTop = 0.0f;
    float m_ContentH = 0.0f;
    int m_WindowWidth = 800, m_WindowHeight = 600;

    float m_Scroll = 0.0f;
    float m_LastContentHeight = 0.0f;

    float m_CursorY = 0.0f; // content-local, top of the current line
    float m_LineBottom = 0.0f;
    float m_PrevRight = 0.0f;
    bool m_SameLine = false;
    int m_IndentLevel = 0;
    int m_ZebraRow = 0;

    Vector2 m_Mouse;
    bool m_MouseDown = false;
    bool m_MousePressed = false;
    bool m_MouseInPanel = false;

    // Which interactive widget owns the drag, so sliding off a slider does not
    // hand it to whatever is now under the cursor. Ids are the per-frame widget
    // ordinal, stable as long as the section list is.
    int m_WidgetCounter = 0;
    int m_ActiveId = 0;

    // DragFloat measures from where the press landed rather than frame to
    // frame, so a drag slower than the rounding step still gets somewhere.
    float m_DragAnchorX = 0.0f;
    float m_DragStartValue = 0.0f;

    std::string m_Tooltip;

    static constexpr int kScrollBarId = -1;
    static constexpr int kResizeId = -2;

    std::vector<Renderer2D::ScreenQuad> m_Flats;
    std::vector<RoundedItem> m_Rounded;
    std::vector<Renderer2D::ScreenQuad> m_Glyphs;
    std::vector<ImageItem> m_Images;

    // TitleBar() runs before any content, so its items sit at the front of the
    // flat and glyph batches. Draw() splits there to render the strip outside
    // the scissor the scrolling content needs.
    size_t m_TitleFlats = 0;
    size_t m_TitleGlyphs = 0;
};
