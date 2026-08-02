#include "GLLoader.h"
#include <iostream>

#if defined(__linux__)
    #include <GL/glx.h>
namespace {
void* GetPlatformProcAddress(const char* name) {
    return reinterpret_cast<void*>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>(name)));
}
}// End Of Namespace
#elif defined (_WIN32)
    #include <windows.h>
namespace {
void* GetPlatformProcAddress(const char* name) {
    // WGL Wiring Required...
    return reinterpret_cast<void*>(wglGetProcAddress(name));
}
} // End of Namespace
#else
namespace {
void* GetPlatformProcAddress(const char*) {return nullptr;}
} // End of Namespace
#endif

namespace GL {

    PFNGLCREATESHADERPROC CreateShader = nullptr;
    PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC CompileShader = nullptr;
    PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC DeleteShader = nullptr;

    PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
    PFNGLATTACHSHADERPROC AttachShader = nullptr;
    PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
    PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
    PFNGLUSEPROGRAMPROC UseProgram = nullptr;

    PFNGLGENBUFFERSPROC GenBuffers = nullptr;
    PFNGLBINDBUFFERPROC BindBuffer = nullptr;
    PFNGLBUFFERDATAPROC BufferData = nullptr;
    PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;

    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray = nullptr;

    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
    PFNGLGETATTRIBLOCATIONPROC GetAttribLocation = nullptr;
    PFNGLUNIFORM1FPROC Uniform1f = nullptr;
    PFNGLUNIFORM2FPROC Uniform2f = nullptr;
    PFNGLUNIFORM3FPROC Uniform3f = nullptr;
    PFNGLUNIFORM4FPROC Uniform4f = nullptr;

namespace {

template <typename FnPtr>
bool LoadOne(FnPtr& outFn, const char* name) {
    outFn = reinterpret_cast<FnPtr>(GetPlatformProcAddress(name));
    if (!outFn) {
        std::cerr << "Engine Warning: Failed to load GL function: " << name << "\n";
        return false;
    }
    return true;
}
} // End of Namespace

bool Load() {
    bool ok = true;

    ok &= LoadOne(CreateShader, "glCreateShader");
    ok &= LoadOne(ShaderSource, "glShaderSource");
    ok &= LoadOne(CompileShader, "glCompileShader");
    ok &= LoadOne(GetShaderiv, "glGetShaderiv");
    ok &= LoadOne(GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= LoadOne(DeleteShader, "glDeleteShader");

    ok &= LoadOne(CreateProgram, "glCreateProgram");
    ok &= LoadOne(AttachShader, "glAttachShader");
    ok &= LoadOne(LinkProgram, "glLinkProgram");
    ok &= LoadOne(GetProgramiv, "glGetProgramiv");
    ok &= LoadOne(GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= LoadOne(DeleteProgram, "glDeleteProgram");
    ok &= LoadOne(UseProgram, "glUseProgram");

    ok &= LoadOne(GenBuffers, "glGenBuffers");
    ok &= LoadOne(BindBuffer, "glBindBuffer");
    ok &= LoadOne(BufferData, "glBufferData");
    ok &= LoadOne(DeleteBuffers, "glDeleteBuffers");

    ok &= LoadOne(VertexAttribPointer, "glVertexAttribPointer");
    ok &= LoadOne(EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= LoadOne(DisableVertexAttribArray, "glDisableVertexAttribArray");

    ok &= LoadOne(GetUniformLocation, "glGetUniformLocation");
    ok &= LoadOne(GetAttribLocation, "glGetAttribLocation");
    ok &= LoadOne(Uniform1f, "glUniform1f");
    ok &= LoadOne(Uniform2f, "glUniform2f");
    ok &= LoadOne(Uniform3f, "glUniform3f");
    ok &= LoadOne(Uniform4f, "glUniform4f");
    ok &= LoadOne(CreateShader, "glCreateShader");
    ok &= LoadOne(ShaderSource, "glShaderSource");
    ok &= LoadOne(CompileShader, "glCompileShader");
    ok &= LoadOne(GetShaderiv, "glGetShaderiv");
    ok &= LoadOne(GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= LoadOne(DeleteShader, "glDeleteShader");

    ok &= LoadOne(CreateProgram, "glCreateProgram");
    ok &= LoadOne(AttachShader, "glAttachShader");
    ok &= LoadOne(LinkProgram, "glLinkProgram");
    ok &= LoadOne(GetProgramiv, "glGetProgramiv");
    ok &= LoadOne(GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= LoadOne(DeleteProgram, "glDeleteProgram");
    ok &= LoadOne(UseProgram, "glUseProgram");

    ok &= LoadOne(GenBuffers, "glGenBuffers");
    ok &= LoadOne(BindBuffer, "glBindBuffer");
    ok &= LoadOne(BufferData, "glBufferData");
    ok &= LoadOne(DeleteBuffers, "glDeleteBuffers");

    ok &= LoadOne(VertexAttribPointer, "glVertexAttribPointer");
    ok &= LoadOne(EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= LoadOne(DisableVertexAttribArray, "glDisableVertexAttribArray");

    ok &= LoadOne(GetUniformLocation, "glGetUniformLocation");
    ok &= LoadOne(GetAttribLocation, "glGetAttribLocation");
    ok &= LoadOne(Uniform1f, "glUniform1f");
    ok &= LoadOne(Uniform2f, "glUniform2f");
    ok &= LoadOne(Uniform3f, "glUniform3f");
    ok &= LoadOne(Uniform4f, "glUniform4f");

    return ok;
    return ok;
}

} // End of Namespace GL