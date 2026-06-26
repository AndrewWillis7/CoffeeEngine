#pragma once
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>
#include <functional>

#include "AssetManager.h"
#include "Actor.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001

class Window {
private:
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    int m_width, m_height;
    bool m_interactive = false;

    HDC m_memDC = nullptr;
    HBITMAP m_backBuffer = nullptr;

private:
    void AddTrayIcon();

    void InitBackBuffer(HDC hdc);
    void CleanupBackBuffer();

    static LRESULT CALLBACK RouteMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
public:
    Window(HINSTANCE hInstance, int width, int height);
    ~Window();

    bool Create(const wchar_t* title, int x, int y);
    void UpdatePosition(int x, int y);
    void ToggleInteraction();
    void Render(const std::vector<RenderData>& data, AssetManager& assets);
    void Run(std::function<void(float)>tick);
    HWND GetHandle() const { return m_hwnd; }
};