#pragma once
#include <memory>

class ActorRegistry;
class Renderer2D;
class UserInputService;
class ScriptEngine;
class UIPanel;

class EngineUIController {
public:
    EngineUIController(ActorRegistry& actors, Renderer2D& renderer, UserInputService& input, ScriptEngine& scripts);
    ~EngineUIController();

    void Update(int windowHeight);
    void Draw();
    bool IsOpen() const { return m_Open; }

private:
    int m_ToggleKeycode;

    ActorRegistry& m_Actors;
    UserInputService& m_Input;
    ScriptEngine& m_Scripts;
    bool m_Open = false;
    std::unique_ptr<UIPanel> m_Panel;
};