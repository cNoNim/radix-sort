#pragma once

#ifndef NDEBUG
void debug_printf(char const * format, ...);
#define DEBUGF(format, ...) debug_printf(format, __VA_ARGS__)
#define BREAKPOINT __debugbreak()
#else
#define DEBUGF(...) ((void)0)
#define BREAKPOINT ((void)0)
#endif
