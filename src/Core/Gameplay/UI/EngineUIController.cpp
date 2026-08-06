#include "EngineUIController.h"
#include "UIPanel.h"
#include "Core/ActorRegistry.h"
#include "Core/Input/UserInputService.h"
#include "Core/Input/KeyMap.h"
#include "Core/ScriptEngine.h"
#include <iostream>

EngineUIController::EngineUIController(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input, ScriptEngine& scripts)
    : m_Actors(actors), m_Input(input), m_Scripts(scripts), m_Panel(std::make_unique<UIPanel>(actors, renderer, input)) {
        m_ToggleKeycode = KeyMap::Get("Backtick");
        if (m_ToggleKeycode == 0) {
            std::cerr << "Engine Warning: KeyMap has no 'Backtick' entry -- falling back to keycode 49\n";
            m_ToggleKeycode = 49;
        }
    }

EngineUIController::~EngineUIController() = default;

void EngineUIController::Update(int windowHeight) {
    if (m_Input.IsKeyPressed(m_ToggleKeycode)) {
        m_Open = !m_Open;
    }
    if (!m_Open) return;

    m_Panel->NewFrame(windowHeight);

    m_Panel->Label("Debug Menu");

    if (m_Panel->Button("Reload Lua Scripts")) {
        m_Scripts.Reload();
    }

    m_Panel->TextBlock(m_Actors.GetDebugLines());
}

void EngineUIController::Draw() {
    if (m_Open) m_Panel->Draw();
}