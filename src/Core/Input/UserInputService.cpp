#include "UserInputService.h"

void UserInputService::OnWindowEvent(const WindowEvent& event) {
    switch (event.type) {
        case WindowEvent::Type::KeyPressed:
            //insert().second is only true the first time a key goes down --
            // guards against auto-repeat and re-firing the "Pressed" edge every
            // tick while a key is held
            if (m_KeysDown.insert(event.keycode).second)
                m_KeysPressedThisFrame.insert(event.keycode);
            break;

        case WindowEvent::Type::KeyReleased:
            m_KeysDown.erase(event.keycode);
            m_KeysReleasedThisFrame.insert(event.keycode);
            break;

        case WindowEvent::Type::MouseButtonPressed:
            if (m_MouseButtonsDown.insert(event.button).second)
                m_MouseButtonsPressedThisFrame.insert(event.button);
            m_MousePosition = Vector2(static_cast<float>(event.mouseX), static_cast<float>(event.mouseY));
            break;

        case WindowEvent::Type::MouseButtonReleased:
            m_MouseButtonsDown.erase(event.button);
            m_MouseButtonsReleasedThisFrame.insert(event.button);
            m_MousePosition = Vector2(static_cast<float>(event.mouseX), static_cast<float>(event.mouseY));
            break;

        case WindowEvent::Type::MouseMoved:
            m_MousePosition = Vector2(static_cast<float>(event.mouseX), static_cast<float>(event.mouseY));
            break;

        case WindowEvent::Type::MouseScrolled:
            m_ScrollDelta += event.scrollDelta;
            break;

        default:
            break; // Close/Resize -- not relevant to UIS
    }
}

void UserInputService::NewFrame() {
    m_KeysPressedThisFrame.clear();
    m_KeysReleasedThisFrame.clear();
    m_MouseButtonsPressedThisFrame.clear();
    m_MouseButtonsReleasedThisFrame.clear();
    m_ScrollDelta = 0.0f;
}

bool UserInputService::IsKeyDown(int keycode) const {
    return m_KeysDown.count(keycode) != 0;
}

bool UserInputService::IsKeyPressed(int keycode) const {
    return m_KeysPressedThisFrame.count(keycode) != 0;
}

bool UserInputService::IsKeyReleased(int keycode) const {
    return m_KeysReleasedThisFrame.count(keycode) != 0;
}

bool UserInputService::IsMouseButtonDown(MouseButton button) const {
    return m_MouseButtonsDown.count(static_cast<int>(button)) != 0;
}

bool UserInputService::IsMouseButtonPressed(MouseButton button) const {
    return m_MouseButtonsPressedThisFrame.count(static_cast<int>(button)) != 0;
}

bool UserInputService::IsMouseButtonReleased(MouseButton button) const {
    return m_MouseButtonsReleasedThisFrame.count(static_cast<int>(button)) != 0;
}