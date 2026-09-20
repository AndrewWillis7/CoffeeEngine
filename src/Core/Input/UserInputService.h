#pragma once
#include "../Math/Vector2.h"
#include "../../IWindow.h" // WindowEvent, MouseButton
#include <unordered_set>

// Polling-style input state, fed by IWindow's raw event callback. main.cpp
// forwards every event into OnWindowEvent() and calls NewFrame() once per loop,
// after PollEvents() and ScriptEngine::Update() have both run, so *Pressed and
// *Released only report true on the frame the transition happened.
class UserInputService {
public:
    void OnWindowEvent(const WindowEvent& event);
    void NewFrame();

    // Raw platform codes: X11 keycodes on Linux, virtual-key codes on Windows.
    // There is no cross-platform name table yet.
    bool IsKeyDown(int keycode) const;
    bool IsKeyPressed(int keycode) const;
    bool IsKeyReleased(int keycode) const;

    bool IsMouseButtonDown(MouseButton button) const;
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonReleased(MouseButton button) const;

    float GetScrollDelta() const {return m_ScrollDelta;}

    Vector2 GetMousePosition() const { return m_MousePosition; }

    // Every keycode that went down this frame -- mainly a debugging aid for
    // finding what a key's raw code is on your platform.
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