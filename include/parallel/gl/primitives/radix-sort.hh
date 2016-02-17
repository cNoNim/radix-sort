#pragma once

#include <parallel/gl/opengl.hh>

namespace parallel {
namespace gl {

void radix_sort(GL const & gl, GLuint key, GLsizeiptr size, GLuint index = 0,
  bool descending = false, bool is_signed = false, bool is_float = false);

}
}

