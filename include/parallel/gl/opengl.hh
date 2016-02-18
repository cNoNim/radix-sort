#pragma once

#define WIN32_LEAN_AND_MEAN 1
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>

#pragma comment(lib, "opengl32.lib")

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
  FUNCTION(BufferData,           BUFFERDATA)           \
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
  static GL const & instance();
  static GL const & initialize(HDC device, GLDEBUGPROC debug_message_callback, bool debug = false);
  static void deinitialize();
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
