#include <iostream>

#include <opengl.hh>
#include <primitives/radix-sort.hh>

#include "debug.hh"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#define EACH(i, size) for (int i = 0; i < size; i++)

void APIENTRY debug_message(
  GLenum source, GLenum type, GLuint id, GLenum severity,
  GLsizei, const GLchar* message, void*) {
  DEBUGF("%1!#08x!:%2!#08x!:%3!#08x!:%4!#08x!:%5", source, type, id, severity, message);
  if (type == GL_DEBUG_TYPE_ERROR) {
    std::cout << message << std::endl;
    exit(1);
  }
}

int main() {
  auto window = CreateWindowExA(WS_EX_APPWINDOW, "static", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  auto device = GetDC(window);
  static PIXELFORMATDESCRIPTOR pfd = { 0 };
  pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  SetPixelFormat(device, ChoosePixelFormat(device, &pfd), &pfd);
  wglMakeCurrent(device, wglCreateContext(device));
  auto major = 0, minor = 0;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major < 4 || minor < 3)
    debug_message(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_ERROR,
      GL_VERSION, GL_DEBUG_SEVERITY_HIGH, -1, "OpenGL 4.3 Required", nullptr);
  GL const & gl = GL::initialize((GLDEBUGPROC) &debug_message);
#ifndef NDEBUG
  int attribs[] = { WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB, 0 };
  wglMakeCurrent(device, gl.CreateContextAttribsARB(device, nullptr, attribs));
  GL::initialize((GLDEBUGPROC) &debug_message);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

  GLuint const count = 256 * 256 * 256;
  struct { GLuint objects[2]; } buffers = { 0 };
  gl.GenBuffers(sizeof(buffers) / sizeof(GLuint), reinterpret_cast<GLuint *>(&buffers));
  gl.BindBuffer(GL_COPY_WRITE_BUFFER, buffers.objects[0]);
  gl.BufferData(GL_COPY_WRITE_BUFFER, sizeof(GLint) * count, nullptr, GL_DYNAMIC_COPY);
  gl.BindBuffer(GL_COPY_WRITE_BUFFER, buffers.objects[1]);
  gl.BufferData(GL_COPY_WRITE_BUFFER, sizeof(GLint) * count, nullptr, GL_DYNAMIC_COPY);
  gl.BindBuffer(GL_COPY_WRITE_BUFFER, buffers.objects[0]);
  auto keys = static_cast<GLint *>(gl.MapBuffer(GL_COPY_WRITE_BUFFER, GL_WRITE_ONLY));
  gl.BindBuffer(GL_COPY_READ_BUFFER, buffers.objects[1]);
  auto indexes = static_cast<GLuint *>(gl.MapBuffer(GL_COPY_READ_BUFFER, GL_WRITE_ONLY));
  EACH(i, count) {
    int j = i == 0 ? 0 : rand() % i;
    keys[i] = keys[j];
    indexes[i] = indexes[j];
    keys[j] = i - count / 2;
    indexes[j] = i;
  }
  gl.UnmapBuffer(GL_COPY_READ_BUFFER);
  gl.UnmapBuffer(GL_COPY_WRITE_BUFFER);
  radix_sort(gl, buffers.objects[0], count, buffers.objects[1], true, true);
  auto passed = true;
  {
    gl.BindBuffer(GL_COPY_READ_BUFFER, buffers.objects[1]);
    auto ptr = static_cast<GLuint *>(gl.MapBuffer(GL_COPY_READ_BUFFER, GL_WRITE_ONLY));
    for (size_t i = 0; i < count && passed; i++) {
      passed &= ptr[i] == count - i - 1;
    }
    gl.UnmapBuffer(GL_COPY_READ_BUFFER);
  }
  if (passed) std::cout << "TEST PASSED" << std::endl;
  else std::cout << "TEST FAILED" << std::endl;
  return 0;
}
