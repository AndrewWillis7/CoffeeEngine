#include "LinuxWindow.h"
#include <iostream>
#include <vector>

#ifdef __linux__

// You'll need the devkit for X11 if you're recompiling the engine btw...
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "stb_image.h"

LinuxWindow::LinuxWindow(const std::string& title, int width, int height)
    : m_width(width), m_height(height)
{
    m_Display = XOpenDisplay(nullptr);
    if (!m_Display) {
        std::cerr << "Engine Fatal: Failed to open X11 display\n";
        exit(1);
    }

    int screen = DefaultScreen(m_Display);
    Window root = RootWindow(m_Display, screen);

    // Request a standard 24bit TrueColor Visual for modern graphics APIs
    XVisualInfo vi;
    if (!XMatchVisualInfo(m_Display, screen, 24, TrueColor, &vi)) {
        std::cerr << "Engine Fatal: No 24-bit TrueColor visual found!\n";
        exit(1);
    }

    Colormap cmap = XCreateColormap(m_Display, root, vi.visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap = cmap;

    // Give all necessary event handles please!!
    swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

    m_Window = XCreateWindow(m_Display, root, 0, 0, m_width, m_height, 0,
                                vi.depth, InputOutput, vi.visual,
                                CWColormap | CWEventMask, &swa);

    XMapWindow(m_Display, m_Window);
    XStoreName(m_Display, m_Window, title.c_str());

    // Intercept the OS window Close button so we don't immediately cancel a process
    m_DeleteMessage = XInternAtom(m_Display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(m_Display, m_Window, &m_DeleteMessage, 1);
}

LinuxWindow::~LinuxWindow() {
    XDestroyWindow(m_Display, m_Window);
    XCloseDisplay(m_Display);
}

void LinuxWindow::PollEvents() {
    XEvent event;
    // XPending checks if there are events without blocking the game loop?
    while (XPending(m_Display)) {
        XNextEvent(m_Display, &event);

        switch (event.type) {
            case ClientMessage:
                // Handle the Window Close
                if (static_cast<unsigned long>(event.xclient.data.l[0]) == m_DeleteMessage) {
                    m_ShouldClose = true;
                    if (m_EventCallback)
                        m_EventCallback(WindowEvent{WindowEvent::Type::Close, 0,0,0});
                }
                break;

            case ConfigureNotify:
                // Handle Window Resize
                if (event.xconfigure.width != m_width || event.xconfigure.height != m_height) {
                    m_width = event.xconfigure.width;
                    m_height = event.xconfigure.height;
                    if (m_EventCallback)
                        m_EventCallback(WindowEvent{WindowEvent::Type::Resize, m_width, m_height, 0});
                }
                break;

            case KeyPress:
                // Handle Keyboard Input
                if (m_EventCallback)
                    m_EventCallback(WindowEvent{WindowEvent::Type::KeyPressed, 0, 0, static_cast<int>(event.xkey.keycode)});
                break;
        }
    }
}

void* LinuxWindow::GetNativeWindow() const {
    // uintpt_t cast prevents compiler warnings when converting a 64bit long to a pointer apparently
    return reinterpret_cast<void*>(static_cast<uintptr_t>(m_Window));
}

void* LinuxWindow::GetNativeDisplay() const {
    return m_Display;
}

// Big Icon stuff, probably wont work... but idk
void LinuxWindow::SetIcon(const std::string& filepath) {
    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

    if (!data) {
        std::cerr << "Engine Warning: Failed to load icon from " << filepath << "\n";
        return;
    }

    // Allocate buffer for X11 format
    std::vector<unsigned long> iconData(2 + width * height);
    iconData[0] = width;
    iconData[1] = height;

    for (int i = 0; i < width * height; ++i) {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        unsigned char b = data[i * 4 + 2];
        unsigned char a = data[i * 4 + 3];

        // Pack RGBA into ARGB
        iconData[2 + i] = (a << 24) | (r << 16) | (g << 8) | b;
    }

    Atom netWmIcon = XInternAtom(m_Display, "_NET_WM_ICON", False);
    Atom cardinal = XInternAtom(m_Display, "CARDINAL", False);

    XChangeProperty(m_Display, m_Window, netWmIcon, cardinal, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(iconData.data()), iconData.size());
    
    XFlush(m_Display);
    stbi_image_free(data);
}

#endif