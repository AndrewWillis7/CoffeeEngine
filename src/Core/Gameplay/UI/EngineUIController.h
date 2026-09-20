#pragma once
#include "DebugOverlay.h"
#include "FrameStats.h"
#include "SceneExplorer.h"
#include <memory>

class ActorRegistry;
class Renderer2D;
class UserInputService;
class ScriptEngine;
class LightingSystem;
class TerrainSystem;
class IWindow;
class UIPanel;

// The debug menu: the backtick panel, the world-space gizmos that go with it,
// and the frame clock it can pause. Owns nothing the engine needs -- pull it
// out of main.cpp and everything still runs.
class EngineUIController {
public:
    EngineUIController(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input,
                       ScriptEngine& scripts, IWindow& window,
                       const LightingSystem& lighting, const TerrainSystem& terrain);
    ~EngineUIController();

    // Real frame time in, simulation frame time out: zero while paused, scaled
    // by the time dial otherwise, and exactly one real frame when a step has
    // been asked for. Also consumes the hotkeys, so call it at the top of the
    // frame, before anything else reads input.
    float BeginFrame(float realDeltaTime);

    // Lays out this frame's panel. Call after the scripts have run, so the
    // values it reads are this frame's rather than last.
    void Update(int windowWidth, int windowHeight);

    // Gizmos first, then the panel over them.
    void Draw();

    bool IsOpen() const { return m_Open; }
    bool IsPaused() const { return m_Paused; }

    // True when the menu has the cursor, so the caller can stop the same click
    // reaching the scene.
    bool WantsMouse() const;

private:
    void SectionPerformance(UIPanel& panel);
    void SectionExplorer(UIPanel& panel);
    void SectionInspector(UIPanel& panel);
    void SectionTime(UIPanel& panel);
    void SectionRender(UIPanel& panel);
    void SectionRegistry(UIPanel& panel);
    void SectionScripts(UIPanel& panel);
    void SectionLog(UIPanel& panel);
    void SectionHelp(UIPanel& panel);

    bool Pressed(int keycode) const;

    ActorRegistry& m_Actors;
    Renderer2D& m_Renderer;
    UserInputService& m_Input;
    ScriptEngine& m_Scripts;
    IWindow& m_Window;
    const LightingSystem& m_Lighting;
    const TerrainSystem& m_Terrain;

    std::unique_ptr<UIPanel> m_Panel;
    SceneExplorer m_Explorer;
    DebugOverlay m_Overlay;
    FrameStats m_Stats;

    // Resolved once from KeyMap; 0 means the table had no entry and the
    // shortcut is simply inert rather than firing on keycode 0.
    struct Hotkeys {
        int toggle = 0;
        int reload = 0;
        int pause = 0;
        int step = 0;
        int overlays = 0;
        int uiScale = 0;
    } m_Keys;

    bool m_Open = false;

    bool m_Paused = false;
    bool m_StepRequested = false;
    float m_TimeScale = 1.0f;
    float m_RealTime = 0.0f;
    float m_SimTime = 0.0f;

    // Deferred to the end of Update(): a reload frees every body mid-frame, and
    // the panel is still being built out of them at the point it is requested.
    bool m_ReloadRequested = false;

    bool m_ShowPerformance = true;
    bool m_ShowExplorer = true;
    bool m_ShowInspector = true;
    bool m_ShowTime = false;
    bool m_ShowRender = false;
    bool m_ShowRegistry = false;
    bool m_ShowScripts = false;
    bool m_ShowLog = false;
    bool m_ShowHelp = false;
};
