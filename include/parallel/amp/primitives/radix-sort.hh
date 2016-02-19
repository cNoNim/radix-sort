#pragma once

#include <amp.h>

namespace parallel {
namespace amp {

void radix_sort(concurrency::accelerator_view & av,
  concurrency::array_view<uint32_t> key, concurrency::array_view<uint32_t> index,
  bool descending = false, bool is_signed = false, bool is_float = false);

}
}

#undef EACH
