#pragma once
#include "IWindow.h"
#include <string>

// Forward Declaration of X11 runtime to keep Xlib out of header
struct _XDisplay;

class LinuxWindow : public IWindow {
public:
    LinuxWindow(const std::string& title, int width, int height);
    ~LinuxWindow() override;

    void PollEvents() override;
    bool ShouldClose() const override { return m_ShouldClose; }

    int GetWidth() const override { return m_width; }
    int GetHeight() const override { return m_height; }

    void SetIcon(const std::string& filepath) override;
    void SetEventCallback(std::function<void(const WindowEvent&)> callback) override { m_EventCallback = callback; }

    void* GetNativeWindow() const override;
    void* GetNativeDisplay() const override;

private:
    _XDisplay* m_Display;
    unsigned long m_Window;
    unsigned long m_DeleteMessage;

    int m_width, m_height;
    bool m_ShouldClose = false;

    std::function<void(const WindowEvent&)> m_EventCallback;
};