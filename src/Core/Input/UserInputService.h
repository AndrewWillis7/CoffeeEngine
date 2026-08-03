#pragma once
#include "../Math/Vector2.h"
#include "../../IWindow.h" // WindowEvent, MouseButton
#include <unordered_set>

// Polling-style input state, fed by IWindow's raw event callback. Wire it up
// in main.cpp:
//
//   window->SetEventCallback([&](const WindowEvent& e){
//       inputService.OnWindowEvent(e);
//       // ...whatever else main.cpp already does with Close/Resize...
//   });
//
// and call inputService.NewFrame() once per loop iteration, after
// window->PollEvents()/ScriptEngine::Update have both run, so *Pressed and
// *Released only report true for the single frame the transition happened.
class UserInputService {
public:
    void OnWindowEvent(const WindowEvent& event);
    void NewFrame();

    // Keycodes are the RAW platform code (X11 keycode on Linux, virtual-key
    // code on Windows) -- there's no cross-platform key-name table yet, same
    // as the pre-existing WindowEvent::KeyPressed. Worth revisiting once we
    // want Lua scripts to say Input.IsKeyDown("W") instead of a magic number.
    bool IsKeyDown(int keycode) const;
    bool IsKeyPressed(int keycode) const;
    bool IsKeyReleased(int keycode) const;

    bool IsMouseButtonDown(MouseButton button) const;
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonReleased(MouseButton button) const;

    float GetScrollDelta() const {return m_ScrollDelta;}

    Vector2 GetMousePosition() const { return m_MousePosition; }

    // Every keycode that went down on THIS frame. Mainly a debugging aid --
    // e.g. print these to find out what keycode "W" actually is on your
    // keyboard/platform, since there's no name->keycode table yet.
    const std::unordered_set<int>& GetKeysPressedThisFrame() const { return m_KeysPressedThisFrame; }

private:
    std::unordered_set<int> m_KeysDown;
    std::unordered_set<int> m_KeysPressedThisFrame;
    std::unordered_set<int> m_KeysReleasedThisFrame;

    std::unordered_set<int> m_MouseButtonsDown;
    std::unordered_set<int> m_MouseButtonsPressedThisFrame;
    std::unordered_set<int> m_MouseButtonsReleasedThisFrame;

    Vector2 m_MousePosition;
    float m_ScrollDelta = 0.0f;
};