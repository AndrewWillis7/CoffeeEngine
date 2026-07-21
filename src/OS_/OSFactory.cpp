#include "IWindow.h"

#ifdef _WIN32
    #include "WindowsWindow.h"
#elif defined(__linux__)
    #include "LinuxWindow.h"
#else
    #error "UNKNOWN PLATFORM!"
#endif

std::unique_ptr<IWindow> IWindow::Create(const std::string& title, int width, int height) {
#ifdef _WIN32
    return std::make_unique<WindowsWindow>(title, width, height);
#elif defined(__linux__)
    return std::make_unique<LinuxWindow>(title, width, height);
#endif
}
