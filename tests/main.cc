#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cinttypes>

#include <opengl.hh>
#include <primitives/radix-sort.hh>

#define EACH(i, size) for (GLsizeiptr i = 0; i < size; i++)

void APIENTRY debug_message(
  GLenum source, GLenum type, GLuint id, GLenum severity,
  GLsizei, const GLchar* message, void*) {
  printf("%#010x:%#010x:%#010x:%#010x:\n%s\n", source, type, id, severity, message);
  if (type == GL_DEBUG_TYPE_ERROR) {
    printf("Press [ENTER] to exit...");
    getchar();
    exit(1);
  }
}

GLuint64 ticks(void) {
  const GLuint64 ticks_per_second = 10000000;
  static LARGE_INTEGER freq;
  static GLuint64 start_time;
  LARGE_INTEGER value;
  QueryPerformanceCounter(&value);
  if (!freq.QuadPart) {
    QueryPerformanceFrequency(&freq);
    start_time = value.QuadPart;
  }
  GLuint64 current_time = value.QuadPart;
  return ((current_time - start_time) * ticks_per_second) / freq.QuadPart;
}

int main(int argc, char const * argv[]) {
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
  bool debug = false;
#ifndef NDEBUG
  debug = true;
#endif
  if (debug || argc > 2) {
    GLint attribs[] = { WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB, 0 };
    wglMakeCurrent(device, gl.CreateContextAttribsARB(device, nullptr, attribs));
    GL::initialize((GLDEBUGPROC) &debug_message);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  }

  printf("OpenGL %s\n\t%s\n\t%s\n", glGetString(GL_VERSION), glGetString(GL_VENDOR), glGetString(GL_RENDERER));

  GLsizeiptr min_count = 1024;
  GLsizeiptr max_count = 67108864;
  if (argc > 1) {
    GLuint64 count;
    sscanf(argv[1], "%llu", &count);
    if (count < (GLuint64)min_count) max_count = min_count;
    if (count > UINT_MAX) max_count = UINT_MAX;
    min_count = max_count = (GLsizeiptr)count;
  }
  struct { GLuint objects[2]; } buffers = { 0 };
  gl.GenBuffers(sizeof(buffers) / sizeof(GLuint), reinterpret_cast<GLuint *>(&buffers));
  gl.BindBuffer(GL_COPY_WRITE_BUFFER, buffers.objects[0]);
  gl.BufferData(GL_COPY_WRITE_BUFFER, sizeof(GLint) * max_count, nullptr, GL_DYNAMIC_COPY);
  gl.BindBuffer(GL_COPY_WRITE_BUFFER, buffers.objects[1]);
  gl.BufferData(GL_COPY_WRITE_BUFFER, sizeof(GLint) * max_count, nullptr, GL_DYNAMIC_COPY);
#ifdef NDEBUG
  //radix_sort(gl, buffers.objects[0], min_count, buffers.objects[1], true, true);
#endif
  for (GLsizeiptr count = max_count; count >= min_count; count >>= 1) {
    gl.BindBuffer(GL_COPY_WRITE_BUFFER, buffers.objects[0]);
    auto keys = static_cast<GLint *>(gl.MapBuffer(GL_COPY_WRITE_BUFFER, GL_WRITE_ONLY));
    gl.BindBuffer(GL_COPY_READ_BUFFER, buffers.objects[1]);
    auto indexes = static_cast<GLuint *>(gl.MapBuffer(GL_COPY_READ_BUFFER, GL_WRITE_ONLY));
    EACH(i, count) {
      GLuint j = i == 0 ? 0 : rand() % i;
      keys[i] = keys[j];
      keys[j] = GLint(i - count / 2);
      indexes[i] = indexes[j];
      indexes[j] = GLuint(i);
    }
    gl.UnmapBuffer(GL_COPY_READ_BUFFER);
    gl.UnmapBuffer(GL_COPY_WRITE_BUFFER);
    auto start = ticks();
    radix_sort(gl, buffers.objects[0], count, buffers.objects[1], true, true);
    auto elapsed = ticks() - start;
    auto passed = true;
    {
      gl.BindBuffer(GL_COPY_READ_BUFFER, buffers.objects[1]);
      auto ptr = static_cast<GLuint *>(gl.MapBuffer(GL_COPY_READ_BUFFER, GL_WRITE_ONLY));
      for (GLsizeiptr i = 0; i < count && passed; i++)
        passed &= ptr[i] == count - i - 1;
      gl.UnmapBuffer(GL_COPY_READ_BUFFER);
    }
    printf("count %10" PRIdPTR " elapsed %10" PRId64 " ticks %10.8f sec speed %10" PRId64 " per sec\t - ",
      count, elapsed, elapsed / 10000000., (count * 10000000ll) / elapsed);
    if (passed) printf("PASSED\n");
    else printf("FAILED\n");
  }
  printf("COMPLETE\nPress [ENTER] for exit...");
  getchar();
  return 0;
}
