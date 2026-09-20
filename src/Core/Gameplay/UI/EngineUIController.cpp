#include "EngineUIController.h"
#include "DebugLog.h"
#include "UIPanel.h"
#include "UIStyle.h"
#include "Core/ActorRegistry.h"
#include "Core/Input/UserInputService.h"
#include "Core/Input/KeyMap.h"
#include "Core/ScriptEngine.h"
#include "Core/Gameplay/LightingSystem.h"
#include "Core/Gameplay/Terrain/TerrainSystem.h"
#include "Renderer/Renderer2D.h"
#include "IWindow.h"
#include <algorithm>
#include <cstdio>
#include <iostream>

namespace {

// The frame budget every timing readout is graded against. 60 Hz is what the
// scene targets; the graph draws a second line at twice it.
constexpr float kTargetMs = 1000.0f / 60.0f;

std::string Num(float value, int decimals = 1) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    return buffer;
}

std::string Ms(float milliseconds) { return Num(milliseconds, 2) + " ms"; }

std::string Count(size_t value) { return std::to_string(value); }

// Vsync parks the frame time right on the budget, so a bare > kTargetMs test
// would strobe between green and amber every frame while nothing is wrong.
constexpr float kWarnMs = kTargetMs * 1.15f;
constexpr float kBadMs = kTargetMs * 2.0f;

Color GradeMs(float milliseconds) {
    if (milliseconds > kBadMs) return UITheme::Bad;
    if (milliseconds > kWarnMs) return UITheme::Warn;
    return UITheme::Good;
}

std::string Clock(float seconds) {
    const int total = static_cast<int>(seconds);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d:%02d", total / 60, total % 60);
    return buffer;
}

// Breaks on the last space that fits rather than mid-word, and indents the
// continuation so a wrapped entry still reads as one entry.
void EmitWrapped(UIPanel& panel, const std::string& text, int width, const Color& color) {
    const size_t limit = static_cast<size_t>(std::max(width, 12));
    size_t start = 0;
    bool first = true;

    while (start < text.size()) {
        const std::string indent = first ? "" : "  ";
        const size_t room = limit - indent.size();
        size_t take = std::min(room, text.size() - start);

        if (start + take < text.size()) {
            const size_t space = text.rfind(' ', start + take);
            if (space != std::string::npos && space > start) take = space - start;
        }
        if (take == 0) take = std::min(room, text.size() - start);

        panel.Label(indent + text.substr(start, take), color);
        start += take;
        while (start < text.size() && text[start] == ' ') ++start;
        first = false;
    }
}

Color SeverityColor(DebugLog::Severity severity) {
    switch (severity) {
        case DebugLog::Severity::Error:   return UITheme::Bad;
        case DebugLog::Severity::Warning: return UITheme::Warn;
        default:                          return UITheme::TextDim;
    }
}

} // namespace

EngineUIController::EngineUIController(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input,
                                       ScriptEngine& scripts, IWindow& window,
                                       const LightingSystem& lighting, const TerrainSystem& terrain)
    : m_Actors(actors), m_Renderer(renderer), m_Input(input), m_Scripts(scripts), m_Window(window),
      m_Lighting(lighting), m_Terrain(terrain),
      m_Panel(std::make_unique<UIPanel>(actors, renderer, input)) {

    m_Keys.toggle = KeyMap::Get("Backtick");
    if (m_Keys.toggle == 0) {
        std::cerr << "Engine Warning: KeyMap has no 'Backtick' entry -- falling back to keycode 49\n";
        m_Keys.toggle = 49;
    }
    m_Keys.reload   = KeyMap::Get("F5");
    m_Keys.pause    = KeyMap::Get("F6");
    m_Keys.step     = KeyMap::Get("F7");
    m_Keys.overlays = KeyMap::Get("F8");
    m_Keys.uiScale  = KeyMap::Get("F9");

    // Big windows get the 2x glyph grid, so the panel is legible on a 4K display
    // without anyone having to find the toggle first.
    if (window.GetHeight() >= 1100) m_Panel->SetUIScale(2.0f);
}

EngineUIController::~EngineUIController() = default;

bool EngineUIController::Pressed(int keycode) const {
    return keycode != 0 && m_Input.IsKeyPressed(keycode);
}

bool EngineUIController::WantsMouse() const {
    return m_Open && m_Panel->WantsMouse();
}

float EngineUIController::BeginFrame(float realDeltaTime) {
    m_Stats.Push(realDeltaTime);
    m_RealTime += realDeltaTime;

    if (Pressed(m_Keys.toggle)) m_Open = !m_Open;
    if (Pressed(m_Keys.reload)) m_ReloadRequested = true;
    if (Pressed(m_Keys.pause)) m_Paused = !m_Paused;
    if (Pressed(m_Keys.step)) {
        m_Paused = true;
        m_StepRequested = true;
    }
    if (Pressed(m_Keys.uiScale)) {
        m_Panel->SetUIScale(m_Panel->GetUIScale() >= 2.0f ? 1.0f : 2.0f);
    }
    if (Pressed(m_Keys.overlays)) {
        // One key cycles the whole set on and off together: the common want is
        // "show me everything" or "get out of the way", not one layer at a time.
        const bool anyOn = m_Overlay.options.bodyBounds || m_Overlay.options.colliders ||
                           m_Overlay.options.lightRanges || m_Overlay.options.velocities ||
                           m_Overlay.options.terrainSurface;
        m_Overlay.options = anyOn ? DebugOverlay::Options{}
                                  : DebugOverlay::Options{true, true, true, true, true};
    }

    float simDelta = realDeltaTime * m_TimeScale;
    if (m_Paused) {
        simDelta = m_StepRequested ? realDeltaTime * m_TimeScale : 0.0f;
        m_StepRequested = false;
    }

    m_SimTime += simDelta;
    return simDelta;
}

void EngineUIController::Update(int windowWidth, int windowHeight) {
    if (!m_Open) {
        // A reload asked for by hotkey still has to happen with the menu shut.
        if (m_ReloadRequested) {
            m_ReloadRequested = false;
            m_Scripts.Reload();
        }
        return;
    }

    UIPanel& panel = *m_Panel;
    panel.NewFrame(windowWidth, windowHeight);

    const float fps = m_Stats.Fps();
    panel.TitleBar("CoffeeEngine", Num(fps, 0) + " fps", GradeMs(m_Stats.AverageMs()));

    SectionPerformance(panel);
    SectionExplorer(panel);
    SectionInspector(panel);
    SectionTime(panel);
    SectionRender(panel);
    SectionRegistry(panel);
    SectionScripts(panel);
    SectionLog(panel);
    SectionHelp(panel);

    if (m_ReloadRequested) {
        m_ReloadRequested = false;
        m_Scripts.Reload();
    }
}

void EngineUIController::Draw() {
    if (!m_Open) return;

    // World gizmos first so the panel covers them rather than the other way
    // round. Selection is revalidated here, not cached, because the reload at
    // the end of Update() can have freed the body since it was picked.
    m_Overlay.Draw(m_Renderer, m_Actors, m_Explorer.Selected(m_Actors), m_RealTime);
    m_Panel->Draw();
}

// --- Sections ---------------------------------------------------------------

void EngineUIController::SectionPerformance(UIPanel& panel) {
    const float average = m_Stats.AverageMs();
    if (!panel.Header("Performance", &m_ShowPerformance, Num(m_Stats.Fps(), 0) + " fps")) return;

    panel.Graph(m_Stats.Ring(), m_Stats.Count(), m_Stats.Oldest(),
                kTargetMs * 3.0f, 42.0f, kWarnMs, kBadMs);

    panel.KeyValue("frame", Ms(m_Stats.LastMs()), GradeMs(m_Stats.LastMs()));
    panel.KeyValue("average", Ms(average), GradeMs(average));
    panel.KeyValue("best / worst", Num(m_Stats.MinMs(), 2) + " / " + Num(m_Stats.MaxMs(), 2),
                   GradeMs(m_Stats.MaxMs()));
    panel.KeyValue("worst fps", Num(m_Stats.WorstFps(), 0), GradeMs(m_Stats.MaxMs()));
    panel.Bar("frame budget " + Num(100.0f * average / kTargetMs, 0) + "%",
              average / kTargetMs, GradeMs(average));

    const LightingSystem::Stats& lighting = m_Lighting.GetStats();
    const TerrainSystem::Stats& terrain = m_Terrain.GetStats();

    panel.Separator();
    panel.KeyValue("lighting", Ms(lighting.milliseconds), GradeMs(lighting.milliseconds));
    panel.KeyValue("  lights / blockers", Count(lighting.lights) + " / " + Count(lighting.blockers));
    panel.KeyValue("  lit pixels", Count(lighting.litPixels));
    panel.KeyValue("terrain", Ms(terrain.milliseconds), GradeMs(terrain.milliseconds));
    panel.KeyValue("  chunks / blades", Count(terrain.chunks) + " / " + Count(terrain.blades));
    panel.KeyValue("  disturbers", Count(terrain.disturbers));

    const float accounted = lighting.milliseconds + terrain.milliseconds;
    panel.KeyValue("rest of frame", Ms(std::max(m_Stats.LastMs() - accounted, 0.0f)));
    panel.Spacing();
}

void EngineUIController::SectionExplorer(UIPanel& panel) {
    const size_t bodies = m_Actors.GetBodyCount();
    if (!panel.Header("Scene Explorer", &m_ShowExplorer, Count(bodies) + " bodies")) return;
    m_Explorer.DrawTree(panel, m_Actors);
    panel.Spacing();
}

void EngineUIController::SectionInspector(UIPanel& panel) {
    const RigidBody2D* selected = m_Explorer.Selected(m_Actors);
    if (!panel.Header("Inspector", &m_ShowInspector, selected ? "1 selected" : "none")) return;
    m_Explorer.DrawInspector(panel, m_Actors);
    panel.Spacing();
}

void EngineUIController::SectionTime(UIPanel& panel) {
    if (!panel.Header("Time", &m_ShowTime, m_Paused ? "paused" : Num(m_TimeScale, 2) + "x")) return;

    panel.Toggle("Paused", &m_Paused);
    if (panel.Button("Step", panel.GetWidth() * 0.3f)) {
        m_Paused = true;
        m_StepRequested = true;
    }
    panel.SameLine();
    if (panel.Button("Normal", panel.GetWidth() * 0.3f)) {
        m_TimeScale = 1.0f;
        m_Paused = false;
    }

    panel.Slider("Time scale", &m_TimeScale, 0.0f, 3.0f, 2);
    if (panel.Button("0.1x", panel.GetWidth() * 0.2f)) m_TimeScale = 0.1f;
    panel.SameLine();
    if (panel.Button("0.5x", panel.GetWidth() * 0.2f)) m_TimeScale = 0.5f;
    panel.SameLine();
    if (panel.Button("1x", panel.GetWidth() * 0.2f)) m_TimeScale = 1.0f;
    panel.SameLine();
    if (panel.Button("2x", panel.GetWidth() * 0.2f)) m_TimeScale = 2.0f;

    panel.KeyValue("real time", Clock(m_RealTime));
    panel.KeyValue("sim time", Clock(m_SimTime), m_Paused ? UITheme::Warn : UITheme::Value);
    panel.KeyValue("frames", std::to_string(m_Stats.FrameIndex()));
    panel.Spacing();
}

void EngineUIController::SectionRender(UIPanel& panel) {
    if (!panel.Header("Render", &m_ShowRender)) return;

    float pixelScale = m_Renderer.GetPixelScale();
    if (panel.Slider("Pixel scale", &pixelScale, 1.0f, 8.0f, 2)) m_Renderer.SetPixelScale(pixelScale);

    panel.KeyValue("window", std::to_string(m_Window.GetWidth()) + " x " + std::to_string(m_Window.GetHeight()));

    bool fullscreen = m_Window.IsFullscreen();
    if (panel.Toggle("Fullscreen", &fullscreen)) m_Window.SetFullscreen(fullscreen);

    float uiScale = m_Panel->GetUIScale();
    if (panel.Button(uiScale >= 2.0f ? "UI scale: 2x" : "UI scale: 1x", panel.GetWidth() * 0.45f)) {
        m_Panel->SetUIScale(uiScale >= 2.0f ? 1.0f : 2.0f);
    }

    panel.Separator();
    panel.Label("Gizmos", UITheme::TextDim);
    panel.Toggle("Body bounds", &m_Overlay.options.bodyBounds);
    panel.Toggle("Colliders", &m_Overlay.options.colliders);
    panel.Toggle("Light ranges", &m_Overlay.options.lightRanges);
    panel.Toggle("Velocities", &m_Overlay.options.velocities);
    panel.Toggle("Terrain surface", &m_Overlay.options.terrainSurface);
    panel.Spacing();
}

void EngineUIController::SectionRegistry(UIPanel& panel) {
    if (!panel.Header("Registry", &m_ShowRegistry)) return;

    panel.KeyValue("bodies", Count(m_Actors.GetBodyCount()));
    panel.KeyValue("collision shapes", Count(m_Actors.GetCollisionShapeCount()));
    panel.KeyValue("cameras", Count(m_Actors.GetCameraCount()));
    panel.KeyValue("light emitters", Count(m_Actors.GetLightEmitterCount()));
    panel.KeyValue("terrain chunks", Count(m_Actors.GetTerrainChunkCount()));
    panel.KeyValue("shaders", Count(m_Actors.GetShaderCount()));
    panel.KeyValue("sprites", Count(m_Actors.GetSpriteCount()));

    // Four bytes a texel in the base buffer, and PixelSprite keeps a lit copy
    // plus four floats of accumulation, so a texel actually costs 4 + 4 + 16.
    const size_t texels = m_Actors.GetSpritePixelCount();
    panel.KeyValue("sprite texels", Count(texels));
    panel.KeyValue("sprite memory", Num(static_cast<float>(texels * 24) / (1024.0f * 1024.0f), 2) + " MB");
    panel.Spacing();
}

void EngineUIController::SectionScripts(UIPanel& panel) {
    if (!panel.Header("Scripts", &m_ShowScripts)) return;

    panel.KeyValue("entry", m_Scripts.GetScriptPath());
    panel.KeyValue("reloads", std::to_string(m_Scripts.GetReloadCount()));
    if (panel.Button("Reload scripts (F5)")) m_ReloadRequested = true;
    panel.Spacing();
}

void EngineUIController::SectionLog(UIPanel& panel) {
    const unsigned int warnings = DebugLog::WarningCount();
    const unsigned int errors = DebugLog::ErrorCount();

    std::string summary = std::to_string(DebugLog::Entries().size()) + " lines";
    if (errors > 0) summary = std::to_string(errors) + " err";
    else if (warnings > 0) summary = std::to_string(warnings) + " warn";

    if (!panel.Header("Engine Log", &m_ShowLog, summary)) return;

    if (panel.Button("Clear", panel.GetWidth() * 0.3f)) DebugLog::Clear();
    panel.SameLine();
    panel.Label(std::to_string(warnings) + " warnings, " + std::to_string(errors) + " errors",
                errors > 0 ? UITheme::Bad : warnings > 0 ? UITheme::Warn : UITheme::TextFaint);

    const auto& entries = DebugLog::Entries();
    // Newest last, so the tail reads like a console. Only the most recent slice
    // is emitted: the rest is still in the buffer, just not worth the rows.
    constexpr size_t kVisible = 120;
    const size_t first = entries.size() > kVisible ? entries.size() - kVisible : 0;
    const int wrapAt = std::max(panel.CharsThatFit(panel.GetWidth()) - 6, 16);

    for (size_t i = first; i < entries.size(); ++i) {
        const DebugLog::Entry& entry = entries[i];
        std::string text = entry.text;
        if (entry.repeats > 1) text += " (x" + std::to_string(entry.repeats) + ")";

        EmitWrapped(panel, text, wrapAt, SeverityColor(entry.severity));
    }
    panel.Spacing();
}

void EngineUIController::SectionHelp(UIPanel& panel) {
    if (!panel.Header("Shortcuts", &m_ShowHelp)) return;

    panel.KeyValue("`", "open / close this menu");
    panel.KeyValue("F5", "reload Lua scripts");
    panel.KeyValue("F6", "pause / resume");
    panel.KeyValue("F7", "step one frame");
    panel.KeyValue("F8", "all gizmos on / off");
    panel.KeyValue("F9", "UI scale 1x / 2x");
    panel.KeyValue("F11", "fullscreen");
    panel.KeyValue("wheel", "scroll the panel");
    panel.KeyValue("drag right edge", "resize the panel");
    panel.KeyValue("click a row", "select, and gizmo it");
    panel.Spacing();
}
