#pragma once

#include "IWindow.h"

#include <string>
#include <functional>
#include <cstdint>

// Forward declarations instead of including windows.h
struct HWND__;
struct HINSTANCE__;

class WindowsWindow : public IWindow
{
public:
    WindowsWindow(
        const std::string& title,
        int width,
        int height);

    ~WindowsWindow() override;

    void PollEvents() override;

    bool ShouldClose() const override
    {
        return m_ShouldClose;
    }

    int GetWidth() const override
    {
        return m_Width;
    }

    int GetHeight() const override
    {
        return m_Height;
    }

    void SetIcon(const std::string& filepath) override;

    void SetFullscreen(bool fullscreen) override;
    bool IsFullscreen() const override { return m_Fullscreen; }

    void SetEventCallback(
        std::function<void(const WindowEvent&)> callback) override
    {
        m_EventCallback = callback;
    }

    void* GetNativeWindow() const override;
    void* GetNativeDisplay() const override;

    // Called by WindowProc in cpp
    long long HandleMessage(
        unsigned int msg,
        std::uintptr_t wParam,
        std::intptr_t lParam);

    // Used only by WindowProc during WM_NCCREATE
    void SetNativeWindow(HWND__* hwnd)
    {
        m_Hwnd = hwnd;
    }

private:
    HWND__* m_Hwnd = nullptr;
    HINSTANCE__* m_Instance = nullptr;

    int m_Width;
    int m_Height;

    bool m_ShouldClose = false;

    // Plain ints rather than RECT/LONG, so this header doesn't need
    // <windows.h> -- the same discipline as m_Hwnd above. SetFullscreen() saves
    // the windowed placement here so leaving fullscreen restores it exactly.
    bool m_Fullscreen = false;
    long m_WindowedStyle = 0;
    int m_WindowedX = 0, m_WindowedY = 0, m_WindowedW = 0, m_WindowedH = 0;

    std::function<void(const WindowEvent&)> m_EventCallback;
};