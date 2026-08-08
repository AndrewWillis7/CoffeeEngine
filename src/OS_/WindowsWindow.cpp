#include "WindowsWindow.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM / GET_Y_LPARAM
#include <iostream>

static LRESULT CALLBACK WIndowProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowsWindow* window = nullptr;

        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);

            window = static_cast<WindowsWindow*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        } else {
            window = reinterpret_cast<WindowsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        if (window)
            return window->HandleMessage(msg, wParam, lParam);

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

WindowsWindow::WindowsWindow(
    const std::string& title,
    int width,
    int height) : m_Width(width), m_Height(height)
{
    m_Instance = GetModuleHandle(nullptr);

    static bool classRegistered = false;

    if (!classRegistered) {
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = WIndowProc;
        wc.hInstance = m_Instance;
        wc.lpszClassName = L"EngineWindowClass";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        if (!RegisterClassEx(&wc)) {
            std::cerr << "Engine Fatal: Failed to register window class\n";
            std::exit(1);
        }

        classRegistered = true;
    }

    RECT rect = {0, 0, width, height};

    AdjustWindowRect(
        &rect, WS_OVERLAPPEDWINDOW, FALSE
    );

    std::wstring wTitle(title.begin(), title.end());

    m_Hwnd = CreateWindowEx(
        0,
        L"EngineWindowClass",
        wTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        m_Instance,
        this);

    if (!m_Hwnd) {
        std::cerr << "Engine Fatal: Failed to create window\n";
        std::exit(1);
    }

    ShowWindow(m_Hwnd, SW_SHOW);
    UpdateWindow(m_Hwnd);
}

WindowsWindow::~WindowsWindow() {
    if (m_Hwnd)
        DestroyWindow(m_Hwnd);
}

void WindowsWindow::PollEvents() {
    MSG msg;

    while (PeekMessage(
        &msg,
        nullptr,
        0,
        0,
        PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


LRESULT WindowsWindow::HandleMessage(
    UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE: {
            m_ShouldClose = true;
            if(m_EventCallback) {
                m_EventCallback({WindowEvent::Type::Close, 0, 0, 0});
            }
            return 0;
        }
        case WM_SIZE: {
            m_Width = LOWORD(lParam);
            m_Height = HIWORD(lParam);
            if (m_EventCallback) {
                m_EventCallback({WindowEvent::Type::Resize, m_Width, m_Height, 0});
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (m_EventCallback) {
                m_EventCallback({WindowEvent::Type::KeyPressed, 0, 0, static_cast<int>(wParam)});
            }
            return 0;
        }
        case WM_KEYUP: {
            if (m_EventCallback) {
                WindowEvent e{WindowEvent::Type::KeyReleased};
                e.keycode = static_cast<int>(wParam);
                m_EventCallback(e);
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            if (m_EventCallback) {
                WindowEvent e{WindowEvent::Type::MouseButtonPressed};
                e.button = static_cast<int>(msg == WM_LBUTTONDOWN ? MouseButton::Left
                                            : msg == WM_RBUTTONDOWN ? MouseButton::Right
                                            : MouseButton::Middle);
                e.mouseX = GET_X_LPARAM(lParam);
                e.mouseY = GET_Y_LPARAM(lParam);
                m_EventCallback(e);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            if (m_EventCallback) {
                WindowEvent e{WindowEvent::Type::MouseButtonReleased};
                e.button = static_cast<int>(msg == WM_LBUTTONUP ? MouseButton::Left
                                            : msg == WM_RBUTTONUP ? MouseButton::Right
                                            : MouseButton::Middle);
                e.mouseX = GET_X_LPARAM(lParam);
                e.mouseY = GET_Y_LPARAM(lParam);
                m_EventCallback(e);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (m_EventCallback) {
                WindowEvent e{WindowEvent::Type::MouseMoved};
                e.mouseX = GET_X_LPARAM(lParam);
                e.mouseY = GET_Y_LPARAM(lParam);
                m_EventCallback(e);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            if (m_EventCallback) {
                WindowEvent e{WindowEvent::Type::MouseScrolled};
                e.scrollDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
                // WM_MOUSEWHEEL gives SCREEN coordinates, unlike every other mouse
                // message here which is client-space -- convert or this will be wrong.
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(m_Hwnd, &pt);
                e.mouseX = pt.x;
                e.mouseY = pt.y;
                m_EventCallback(e);
            }
            return 0;
        }
    }

    return DefWindowProc(
        m_Hwnd,
        msg,
        wParam,
        lParam);
}

void* WindowsWindow::GetNativeWindow() const {
    return m_Hwnd;
}

void* WindowsWindow::GetNativeDisplay() const {
    return m_Instance;
}

void WindowsWindow::SetIcon(const std::string& filepath) {
    HICON icon = static_cast<HICON>(
        LoadImageA(
            nullptr,
            filepath.c_str(),
            IMAGE_ICON,
            0,
            0,
            LR_LOADFROMFILE | LR_DEFAULTSIZE));

    if (!icon) {
        std::cerr << "Engine Warning: Failed to load Icon: " << filepath << '\n';
        return;
    }

    SendMessage(m_Hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    SendMessage(m_Hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
}

// Borderless-fullscreen toggle (WS_POPUP sized to the monitor), not true
// exclusive fullscreen -- simpler, no display-mode switch, and plays
// nicer with alt-tab/multi-monitor setups, same trade-off most modern
// engines make by default. SetWindowPos's SWP_FRAMECHANGED synchronously
// pumps a WM_SIZE through the message loop, which already fires
// WindowEvent::Type::Resize via HandleMessage() above -- so, same as the
// Linux EWMH path, the camera/letterbox system picks up the new size with
// no extra plumbing. UNVERIFIED -- no Windows box to test against, same
// caveat already on WM_MOUSEWHEEL elsewhere in this file.
void WindowsWindow::SetFullscreen(bool fullscreen) {
    if (fullscreen == m_Fullscreen) return;

    if (fullscreen) {
        m_WindowedStyle = GetWindowLong(m_Hwnd, GWL_STYLE);

        RECT windowedRect;
        GetWindowRect(m_Hwnd, &windowedRect);
        m_WindowedX = windowedRect.left;
        m_WindowedY = windowedRect.top;
        m_WindowedW = windowedRect.right - windowedRect.left;
        m_WindowedH = windowedRect.bottom - windowedRect.top;

        HMONITOR monitor = MonitorFromWindow(m_Hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(MONITORINFO) };
        GetMonitorInfo(monitor, &mi);

        SetWindowLong(m_Hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(m_Hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_FRAMECHANGED);
    } else {
        SetWindowLong(m_Hwnd, GWL_STYLE, m_WindowedStyle);
        SetWindowPos(m_Hwnd, HWND_TOP,
            m_WindowedX, m_WindowedY, m_WindowedW, m_WindowedH,
            SWP_FRAMECHANGED);
    }

    m_Fullscreen = fullscreen;
}

#endif