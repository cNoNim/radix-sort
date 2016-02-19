#pragma once

#include <parallel/gl/opengl.hh>

namespace parallel {
namespace gl {

void radix_sort(GL const & gl, buffer key, GLsizeiptr size = 0, buffer index = buffer::empty(),
  bool descending = false, bool is_signed = false, bool is_float = false);

}
}

