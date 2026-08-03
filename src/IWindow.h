#pragma once
#include <string>
#include <memory>
#include <functional>

// Shared across OS_ Backends and UserInputService so both sides can agree on
// what a mouse button number means

enum class MouseButton {Left, Right, Middle};

// Struct for passing events to the engine
struct WindowEvent {
    enum class Type { Close, Resize, KeyPressed, KeyReleased, MouseButtonPressed, MouseButtonReleased, MouseMoved } type;
    int width = 0, height = 0, keycode = 0;

    // Mouse Specific Fields. mouseX/mouseY are window-client-space pixels
    // Button is a MouseButton cast to int
    int mouseX = 0, mouseY = 0, button = 0;
};

class IWindow {
public:
    virtual ~IWindow() = default;

    // Core Window Loop
    virtual void PollEvents() = 0;
    virtual bool ShouldClose() const = 0;

    // Properties
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;

    // Features
    virtual void SetIcon(const std::string& filepath) = 0;
    virtual void SetEventCallback(std::function<void(const WindowEvent&)> callback) = 0;

    // Native OS Handles
    virtual void* GetNativeWindow() const = 0;
    virtual void* GetNativeDisplay() const = 0;

    // Factory method (implement in main probably...)
    static std::unique_ptr<IWindow> Create(const std::string& title, int width, int height);
};