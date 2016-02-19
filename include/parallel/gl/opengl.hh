#pragma once

#define WIN32_LEAN_AND_MEAN 1
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>

#pragma comment(lib, "opengl32.lib")

#define EACH(i, size) for (GLsizeiptr i = 0; i < size; i++)

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#define _STR(x) #x
#define STR(x) _STR(x)

#define GLSL(...) STR(__VA_ARGS__) "\n"
#define GLSL_DEFINE(name, ...) "#define " STR(name) " " GLSL(__VA_ARGS__)

#define GL_FUNCTIONS(FUNCTION)                         \
  FUNCTION(BindBuffer,           BINDBUFFER)           \
  FUNCTION(BindBufferBase,       BINDBUFFERBASE)       \
  FUNCTION(BindBufferRange,      BINDBUFFERRANGE)      \
  FUNCTION(BindProgramPipeline,  BINDPROGRAMPIPELINE)  \
  FUNCTION(GetBufferParameteriv, GETBUFFERPARAMETERIV) \
  FUNCTION(GetBufferParameteri64v, GETBUFFERPARAMETERI64V) \
  FUNCTION(BufferData,           BUFFERDATA)           \
  FUNCTION(BufferSubData,        BUFFERSUBDATA)        \
  FUNCTION(CopyBufferSubData,    COPYBUFFERSUBDATA)    \
  FUNCTION(CreateShaderProgramv, CREATESHADERPROGRAMV) \
  FUNCTION(DebugMessageCallback, DEBUGMESSAGECALLBACK) \
  FUNCTION(DebugMessageInsert,   DEBUGMESSAGEINSERT)   \
  FUNCTION(DispatchCompute,      DISPATCHCOMPUTE)      \
  FUNCTION(GenBuffers,           GENBUFFERS)           \
  FUNCTION(GenProgramPipelines,  GENPROGRAMPIPELINES)  \
  FUNCTION(GetProgramInfoLog,    GETPROGRAMINFOLOG)    \
  FUNCTION(GetProgramiv,         GETPROGRAMIV)         \
  FUNCTION(MapBuffer,            MAPBUFFER)            \
  FUNCTION(MemoryBarrier,        MEMORYBARRIER)        \
  FUNCTION(UnmapBuffer,          UNMAPBUFFER)          \
  FUNCTION(UseProgram,           USEPROGRAM)           \
  FUNCTION(UseProgramStages,     USEPROGRAMSTAGES)

namespace parallel {
namespace gl {

struct GL {
#define FUNCTION(name, NAME) \
PFNGL ## NAME ## PROC name;
  GL_FUNCTIONS(FUNCTION)
#undef FUNCTION
  GLint alignment;
  GL & initialize(HDC device, GLDEBUGPROC debug_message_callback, bool debug = false);
  void deinitialize();

  static GL & instance();
};

struct buffer
{
  GLuint id;
  template<GLenum USAGE>
  void allocate(GL const & gl, GLsizeiptr size) {
    gl.BindBuffer(GL_COPY_WRITE_BUFFER, id);
    gl.BufferData(GL_COPY_WRITE_BUFFER, size, nullptr, USAGE);
  }
  template<GLenum USAGE>
  GLsizeiptr allocate(GL const & gl, GLsizeiptr size, GLsizei count, bool aligned = false) {
    auto aligned_size = aligned
      ? ((size + gl.alignment - 1) / gl.alignment) * gl.alignment
      : size * count;
    gl.BindBuffer(GL_COPY_WRITE_BUFFER, id);
    gl.BufferData(GL_COPY_WRITE_BUFFER, aligned_size * count, nullptr, USAGE);
    return aligned_size;
  }
  void free(GL const & gl) {
    gl.BindBuffer(GL_COPY_WRITE_BUFFER, id);
    gl.BufferData(GL_COPY_WRITE_BUFFER, 0, nullptr, GL_DYNAMIC_COPY);
  }
  template<typename T>
  void sub_data(GL const & gl, T && data, GLsizeiptr offset) {
    gl.BindBuffer(GL_COPY_WRITE_BUFFER, id);
    gl.BufferSubData(GL_COPY_WRITE_BUFFER, offset, sizeof(T), &data);
  }
  template<typename T, size_t N>
  void sub_data(GL const & gl, T (&array)[N], GLsizeiptr alignment) {
    gl.BindBuffer(GL_COPY_WRITE_BUFFER, id);
    EACH (i, N)
      gl.BufferSubData(GL_COPY_WRITE_BUFFER, i * alignment, sizeof(T), &array[i]);
  }
  template<GLenum TARGET, GLenum ACCESS, typename T, typename F>
  void map(GL const & gl, F && f) {
    gl.BindBuffer(TARGET, id);
    auto ptr = reinterpret_cast<T *>(gl.MapBuffer(TARGET, ACCESS));
    f(gl, ptr);
    gl.UnmapBuffer(TARGET);
  }

  template<GLenum TARGET>
  void bind(GL const & gl, GLuint index) { gl.BindBufferBase(TARGET, index, id); }
  template<GLenum TARGET>
  void bind(GL const & gl, GLuint index, GLsizeiptr offset, GLsizeiptr size) {
    gl.BindBufferRange(TARGET, index, id, offset, size);
  }
  GLsizeiptr size(GL const & gl) {
    if (sizeof(GLsizeiptr) == sizeof(GLint)) {
      GLint size = 0;
      gl.BindBuffer(GL_COPY_READ_BUFFER, id);
      gl.GetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &size);
      return (GLsizeiptr)size;
    } else {
      GLint64 size = 0;
      gl.BindBuffer(GL_COPY_READ_BUFFER, id);
      gl.GetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &size);
      return (GLsizeiptr)size;
    }
  }
  bool is_empty() { return id == 0; }
  static buffer empty() {
    return buffer{ 0 };
  }
  static void factory(GL const & gl, GLsizei count, buffer * buffers) {
    gl.GenBuffers(count, reinterpret_cast<GLuint *>(buffers));
  }
};

template<GLuint TYPE>
struct program  {
  GLuint id;
};

template<>
struct program<GL_COMPUTE_SHADER> {
  GLuint id;
  void dispatch(GL const & gl, GLuint x = 1, GLuint y = 1, GLuint z = 1) {
    gl.UseProgram(id);
    gl.DispatchCompute(x, y, z);
  }
};

template<GLuint TYPE, typename... Sources>
program<TYPE>
make_program(GL const & gl, Sources... sources) {
  GLchar const * array_sources[] = {
    "#version 430 core\n"
    GLSL(
    precision highp float;
    precision highp int;
    layout(std140, column_major) uniform;
    layout(std430, column_major) buffer;
    ), sources...
  };
  auto id = gl.CreateShaderProgramv(TYPE, ARRAYSIZE(array_sources), array_sources);
  auto status = GL_TRUE;
  gl.GetProgramiv(id, GL_LINK_STATUS, &status);
  if (GL_TRUE != status) {
    char info[1024];
    gl.GetProgramInfoLog(id, sizeof(info), nullptr, info);
    gl.DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_ERROR,
      GL_LINK_STATUS, GL_DEBUG_SEVERITY_HIGH, -1, info);
  }
  return program<TYPE> { id };
}

using vertex_program = program<GL_VERTEX_SHADER>;
using fragment_program = program<GL_FRAGMENT_SHADER>;
using compute_program = program<GL_COMPUTE_SHADER>;

struct pipeline {
  GLuint id;
  pipeline(GL const & gl) : id(make(gl)) {}
  pipeline & use(GL const & gl, program<GL_VERTEX_SHADER> const & program) { gl.UseProgramStages(id, GL_VERTEX_SHADER_BIT, program.id); return *this; }
  pipeline & use(GL const & gl, program<GL_FRAGMENT_SHADER> const & program) { gl.UseProgramStages(id, GL_FRAGMENT_SHADER_BIT, program.id); return *this; }
  void rect(GL const & gl) {
    gl.UseProgram(0);
    gl.BindProgramPipeline(id);
    glRects(-1, -1, 1, 1);
  }
private:
  static GLuint
  make(GL const & gl) {
    auto id = 0u;
    gl.GenProgramPipelines(1, &id);
    return id;
  }
};

}
}

#undef EACH
