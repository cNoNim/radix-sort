#include "debug.hh"

#include <cstdarg>
#include <cstdio>

void debug_printf(char const * format, ...) {
  va_list args = nullptr;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}
