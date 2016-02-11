#pragma once

#include <cstdint>

struct GL;
void radix_sort(GL const & gl, unsigned int key, ptrdiff_t size, unsigned int index = 0,
  bool descending = false, bool is_signed = false, bool is_float = false);
