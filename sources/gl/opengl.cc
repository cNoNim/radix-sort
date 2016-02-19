#include "parallel/gl/opengl.hh"

#include <GL/wglext.h>

namespace parallel {
namespace gl {

static GL gl;

GL & GL::instance() { return gl; }

static PIXELFORMATDESCRIPTOR pfd = { 0 };
GL & GL::initialize(HDC device, GLDEBUGPROC debug_message_callback, bool debug /*= false*/) {
  static char const * names[] = {
#define FUNCTION(name, NAME) "gl" # name,
    GL_FUNCTIONS(FUNCTION)
#undef FUNCTION
    nullptr
  };

  pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  SetPixelFormat(device, ChoosePixelFormat(device, &pfd), &pfd);
  wglMakeCurrent(device, wglCreateContext(device));

  auto major = 0, minor = 0;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major < 4 || minor < 3)
    debug_message_callback(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_ERROR,
      GL_VERSION, GL_DEBUG_SEVERITY_HIGH, -1, "OpenGL 4.3 Required", nullptr);

  if (debug) {
    auto wglCreateContextAttribsARB
      = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
    GLint attribs[] = { WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB, 0 };
    auto context = wglGetCurrentContext();
    wglMakeCurrent(device, wglCreateContextAttribsARB(device, nullptr, attribs));
    wglDeleteContext(context);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  }

  auto gl_name = names;
  auto gl_function = reinterpret_cast<PROC *>(this);
  while (*gl_name) *gl_function++ = wglGetProcAddress(*gl_name++);

  if(debug_message_callback != nullptr)
    this->DebugMessageCallback(debug_message_callback, nullptr);

  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &this->alignment);

  return *this;
}

void GL::deinitialize() {
  auto context = wglGetCurrentContext();
  auto device = wglGetCurrentDC();
  wglMakeCurrent(nullptr, nullptr);
  wglDeleteContext(context);
  DeleteDC(device);
}

}
}
