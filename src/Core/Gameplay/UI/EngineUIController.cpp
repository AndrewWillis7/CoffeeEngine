#include "EngineUIController.h"
#include "UIPanel.h"
#include "Core/ActorRegistry.h"
#include "Core/Input/UserInputService.h"
#include "Core/ScriptEngine.h"

EngineUIController::EngineUIController(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input, ScriptEngine& scripts)
    : m_Actors(actors), m_Input(input), m_Scripts(scripts), m_Panel(std::make_unique<UIPanel>(actors, renderer, input)) {}

EngineUIController::~EngineUIController() = default;

void EngineUIController::Update(int windowHeight) {
    if (m_Input.IsKeyPressed(kBacktickKeycode)) {
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