#pragma once
#include <string>
#include <memory>
#include <functional>

// Shared across OS_ Backends and UserInputService so both sides can agree on
// what a mouse button number means

enum class MouseButton {Left, Right, Middle};

// Struct for passing events to the engine
struct WindowEvent {
    enum class Type { Close, Resize, KeyPressed, KeyReleased, MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled } type;
    int width = 0, height = 0, keycode = 0;

    // Mouse Specific Fields. mouseX/mouseY are window-client-space pixels
    // Button is a MouseButton cast to int
    int mouseX = 0, mouseY = 0, button = 0;
    float scrollDelta = 0.0f;
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

    // Toggles OS-level fullscreen (borderless, monitor-filling on Windows;
    // EWMH _NET_WM_STATE_FULLSCREEN on Linux -- needs an EWMH-compliant
    // window manager, so it's a silent no-op under a bare Xvfb/no-WM
    // setup). Either path resizes the real window, which fires the usual
    // WindowEvent::Type::Resize -> Renderer2D::SetViewportSize callback
    // already wired in main.cpp -- no extra plumbing needed for the
    // camera/letterbox system to pick up the new size.
    virtual void SetFullscreen(bool fullscreen) = 0;
    virtual bool IsFullscreen() const = 0;

    // Native OS Handles
    virtual void* GetNativeWindow() const = 0;
    virtual void* GetNativeDisplay() const = 0;

    // Factory method (implement in main probably...)
    static std::unique_ptr<IWindow> Create(const std::string& title, int width, int height);
};