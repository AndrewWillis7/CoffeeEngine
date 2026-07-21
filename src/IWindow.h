#pragma once
#include <string>
#include <memory>
#include <functional>

// Struct for passing events to the engine
struct WindowEvent {
    enum class Type { Close, Resize, KeyPressed } type;
    int width, height, keycode;
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