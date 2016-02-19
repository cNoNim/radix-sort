#include <cstdlib>
#include <cstdint>
#include <cinttypes>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <parallel/amp/primitives/radix-sort.hh>
#include <parallel/gl/opengl.hh>
#include <parallel/gl/primitives/radix-sort.hh>

#define EACH(i, size) for (size_t i = 0; i < size; i++)

uint64_t ticks(void) {
  const uint64_t ticks_per_second = UINT64_C(10000000);
  static LARGE_INTEGER freq;
  static uint64_t start_time;
  LARGE_INTEGER value;
  QueryPerformanceCounter(&value);
  if (!freq.QuadPart) {
    QueryPerformanceFrequency(&freq);
    start_time = value.QuadPart;
  }
  return ((value.QuadPart - start_time) * ticks_per_second) / freq.QuadPart;
}

template<typename F>
uint64_t timed(F && f) {
  auto start = ticks();
  f();
  return ticks() - start;
};

void APIENTRY debug_message(
  GLenum source, GLenum type, GLuint id, GLenum severity,
  GLsizei, GLchar const * message, void const *) {
  std::cout    << std::resetiosflags << std::hex << std::setfill('0')
    << "0x"    << std::setw(8)       << source   << ":"
    << "0x"    << std::setw(8)       << type     << ":"
    << "0x"    << std::setw(8)       << id       << ":"
    << "0x"    << std::setw(8)       << severity << std::endl
    << message << std::endl;
  if (type == GL_DEBUG_TYPE_ERROR) {
    std::cout << "Press [ENTER] to exit...";
    std::cin.ignore();
    exit(1);
  }
}

void test_gl(size_t min_count, size_t max_count, bool debug) {
  using namespace parallel::gl;
  auto window = CreateWindowExA(WS_EX_APPWINDOW, "static", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  auto device = GetDC(window);
  auto & gl = GL::instance().initialize(device, &debug_message, debug);

  std::cout
    << "OpenGL " << glGetString(GL_VERSION)  << std::endl
    << "\t"      << glGetString(GL_VENDOR)   << std::endl
    << "\t"      << glGetString(GL_RENDERER) << std::endl;

  struct { buffer objects[2]; } buffers = { 0 };
  buffer::factory(gl, sizeof(buffers) / sizeof(GLuint), buffers.objects);
  buffers.objects[0].allocate<GL_DYNAMIC_COPY>(gl, sizeof(GLint) * max_count);
  buffers.objects[1].allocate<GL_DYNAMIC_COPY>(gl, sizeof(GLuint) * max_count);
  if (!debug) {
    radix_sort(gl, buffers.objects[0], min_count, buffers.objects[1], true, true);
    glFinish();
  }
  for (size_t count = min_count; count <= max_count; count <<= 1) {
    buffers.objects[0].map<GL_COPY_WRITE_BUFFER, GL_WRITE_ONLY, GLint>(gl,
    [&buffers, &count](GL const & gl, GLint * keys) {
      buffers.objects[1].map<GL_COPY_READ_BUFFER, GL_WRITE_ONLY, GLuint>(gl,
      [&keys, &count](GL const &, GLuint * indexes) {
        EACH(i, count) {
          size_t j = i == 0 ? 0 : rand() % i;
          keys[i] = keys[j];
          keys[j] = GLint(i - count / 2);
          indexes[i] = indexes[j];
          indexes[j] = GLuint(i);
        }
      });
    });
    auto elapsed = timed([&gl, &buffers, &count] {
      radix_sort(gl, buffers.objects[0], count, buffers.objects[1], true, true);
      glFinish();
    });
    auto passed = true;
    buffers.objects[1].map<GL_COPY_READ_BUFFER, GL_WRITE_ONLY, GLuint>(gl,
    [&count, &passed](GL const &, GLuint * ptr) {
      for (size_t i = 0; i < count && passed; i++) passed &= ptr[i] == count - i - 1;
    });
    std::cout       << std::setprecision(8) << std::setfill(' ')
      << "count "   << std::setw(10)        << count   << " "
      << "elapsed " << std::setw(10)        << elapsed << " ticks " << std::setw(10) << elapsed / 10000000. << " sec "
      << "speed "   << std::setw(12)        << (count * 10000000ll) / elapsed                               << " per sec "
      << "- "       << (passed ? "PASSED" : "FAILED")                                                       << std::endl;
  }
  std::cout << "COMPLETE OpenGL" << std::endl;
  gl.deinitialize();
}

void test_amp(size_t min_count, size_t max_count, bool debug) {
  using namespace parallel::amp;
  using namespace concurrency;
  auto acc = accelerator();
  auto cpu_acc = accelerator(accelerator::cpu_accelerator);
  std::wcout << L"C++ AMP"      << std::endl
    << L"\t" << acc.description << std::endl;
  array<uint32_t> keys(max_count, cpu_acc.default_view, acc.default_view);
  array<uint32_t> indexes(max_count, cpu_acc.default_view, acc.default_view);
  for (size_t count = min_count; count <= max_count; count <<= 1) {
    EACH(i, count) {
      size_t j = i == 0 ? 0 : rand() % i;
      keys[i] = keys[j];
      keys[j] = int32_t(i - count / 2);
      indexes[i] = indexes[j];
      indexes[j] = uint32_t(i);
    }
    auto elapsed = timed([&acc, &keys, &indexes, &count] {
      radix_sort<int>(acc.default_view, keys.view_as(extent<1>(count)), indexes.view_as(extent<1>(count)), true);
      acc.default_view.wait();
    });
    auto passed = true;
    for (size_t i = 0; i < count && passed; i++)
      passed &= indexes[i] == count - i - 1;
    std::cout << std::setprecision(8) << std::setfill(' ')
      << "count " << std::setw(10) << count << " "
      << "elapsed " << std::setw(10) << elapsed << " ticks " << std::setw(10) << elapsed / 10000000. << " sec "
      << "speed " << std::setw(10) << (count * 10000000ll) / elapsed << " per sec "
      << "- " << (passed ? "PASSED" : "FAILED") << std::endl;
  }
  std::cout << "COMPLETE C++ AMP" << std::endl;
}

int main(int argc, char const * argv[]) {
#ifndef NDEBUG
  bool debug = true;
#else
  bool debug = argc > 2;
#endif
  size_t min_count = 1024;
  size_t max_count = 1024 * 1024;
  if (argc > 1) {
    uint64_t count;
    std::istringstream(argv[1]) >> count;
    if (count < min_count) max_count = min_count;
    if (count > UINT_MAX) max_count = UINT_MAX;
    min_count = max_count = static_cast<size_t>(count);
  }
  test_gl(min_count, max_count, debug);
  test_amp(min_count, max_count, debug);
  std::cout << "Press [ENTER] for exit...";
  std::cin.ignore();
  return 0;
}
