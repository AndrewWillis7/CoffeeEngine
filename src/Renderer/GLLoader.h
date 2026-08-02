#pragma once
#include <GL/gl.h>
#include <cstddef>

// On Linux the ABI only guarentees libGL exports up through OpenGL 1.2
// Everything Shader related (GL 2.0+) has to be resolved at runtime
// glXGetProcAddressARB can do this

// Deliberately not GLAD/GLEW, just entrypoints

namespace GL {

using PFNGLCREATESHADERPROC = GLuint (*)(GLenum type);
using PFNGLSHADERSOURCEPROC = void (*)(GLuint shader, GLsizei count, const char* const* string, const GLint* length);
using PFNGLCOMPILESHADERPROC = void (*)(GLuint shader);
using PFNGLGETSHADERIVPROC = void (*)(GLuint shader, GLenum pname, GLint* params);
using PFNGLGETSHADERINFOLOGPROC = void (*)(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog);
using PFNGLDELETESHADERPROC = void (*)(GLuint shader);

using PFNGLCREATEPROGRAMPROC = GLuint (*)();
using PFNGLATTACHSHADERPROC = void (*)(GLuint program, GLuint shader);
using PFNGLLINKPROGRAMPROC = void (*)(GLuint program);
using PFNGLGETPROGRAMIVPROC = void (*)(GLuint program, GLenum pname, GLint* params);
using PFNGLGETPROGRAMINFOLOGPROC = void (*)(GLuint program, GLsizei bufSize, GLsizei* length, char* infoLog);
using PFNGLDELETEPROGRAMPROC = void (*)(GLuint program);
using PFNGLUSEPROGRAMPROC = void (*)(GLuint program);

using PFNGLGENBUFFERSPROC = void (*)(GLsizei n, GLuint* buffers);
using PFNGLBINDBUFFERPROC = void (*)(GLenum target, GLuint buffer);
using PFNGLBUFFERDATAPROC = void (*)(GLenum target, ptrdiff_t size, const void* data, GLenum usage);
using PFNGLDELETEBUFFERSPROC = void (*)(GLsizei n, const GLuint* buffers);

using PFNGLVERTEXATTRIBPOINTERPROC = void (*)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (*)(GLuint index);
using PFNGLDISABLEVERTEXATTRIBARRAYPROC = void (*)(GLuint index);

using PFNGLGETUNIFORMLOCATIONPROC = GLint (*)(GLuint program, const char* name);
using PFNGLGETATTRIBLOCATIONPROC = GLint (*)(GLuint program, const char* name);
using PFNGLUNIFORM1FPROC = void (*)(GLint location, GLfloat v0);
using PFNGLUNIFORM2FPROC = void (*)(GLint location, GLfloat v0, GLfloat v1);
using PFNGLUNIFORM3FPROC = void (*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
using PFNGLUNIFORM4FPROC = void (*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

extern PFNGLCREATESHADERPROC CreateShader;
extern PFNGLSHADERSOURCEPROC ShaderSource;
extern PFNGLCOMPILESHADERPROC CompileShader;
extern PFNGLGETSHADERIVPROC GetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
extern PFNGLDELETESHADERPROC DeleteShader;

extern PFNGLCREATEPROGRAMPROC CreateProgram;
extern PFNGLATTACHSHADERPROC AttachShader;
extern PFNGLLINKPROGRAMPROC LinkProgram;
extern PFNGLGETPROGRAMIVPROC GetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
extern PFNGLDELETEPROGRAMPROC DeleteProgram;
extern PFNGLUSEPROGRAMPROC UseProgram;

extern PFNGLGENBUFFERSPROC GenBuffers;
extern PFNGLBINDBUFFERPROC BindBuffer;
extern PFNGLBUFFERDATAPROC BufferData;
extern PFNGLDELETEBUFFERSPROC DeleteBuffers;

extern PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC DisableVertexAttribArray;

extern PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
extern PFNGLGETATTRIBLOCATIONPROC GetAttribLocation;
extern PFNGLUNIFORM1FPROC Uniform1f;
extern PFNGLUNIFORM2FPROC Uniform2f;
extern PFNGLUNIFORM3FPROC Uniform3f;
extern PFNGLUNIFORM4FPROC Uniform4f;

// Resolves every entrypoint above against the GL Context
// Must be called after IGraphicsContext::Init()
bool Load();

}