#include "IWindow.h"

#ifdef _WIN32
    // WINDOWS STUFF
#elif defined(__linux__)
    #include "LinuxWindow.h"
#else
    #error "UNKNOWN PLATFORM!"
#endif

std::unique_ptr<IWindow> IWindow::Create(const std::string& title, int width, int height) {
#ifdef _WIN32
    // WINDOWS CREATION
#elif defined(__linux__)
    return std::make_unique<LinuxWindow>(title, width, height);
#endif
}
