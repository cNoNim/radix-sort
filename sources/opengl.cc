#include "opengl.hh"

GL gl;
bool initialized = false;

GL const & GL::instance() {
  if (!initialized) GL::initialize(nullptr);
  return gl;
}

GL const & GL::initialize(GLDEBUGPROC debug_message_callback) {
  static char const * names[] = {
#define FUNCTION(name, NAME) "gl" # name,
    GL_FUNCTIONS(FUNCTION)
#ifndef NDEBUG
    GL_DEBUG_FUNCTIONS(FUNCTION)
#endif
#undef FUNCTION
#define FUNCTION(name, NAME) "wgl" # name,
    WGL_FUNCTIONS(FUNCTION)
#ifndef NDEBUG
    WGL_DEBUG_FUNCTIONS(FUNCTION)
#endif
#undef FUNCTION
    nullptr
  };

  auto gl_name = names;
  auto gl_function = reinterpret_cast<PROC *>(&gl);
  while (*gl_name) *gl_function++ = wglGetProcAddress(*gl_name++);

  if(debug_message_callback != nullptr)
    gl.DebugMessageCallback(debug_message_callback, nullptr);

  return gl;
}
