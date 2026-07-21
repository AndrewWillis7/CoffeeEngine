#include "IGraphicsContext.h"

#ifdef _WIN32
    // Integrate in windows later (WGL)
#elif defined(__linux__)
    #include "GLXGraphicsContext.h"
#endif

std::unique_ptr<IGraphicsContext> IGraphicsContext::Create(void* nativeDisplay, void* nativeWindow) {
#ifdef _WIN32
    return nullptr;
#elif defined(__linux__)
    return std::make_unique<GLXGraphicsContext>(nativeDisplay, nativeWindow);
#endif
}