#pragma once
#include <cstdarg>
#include <cstdio>
#include <ctime>

// Minimal logging to stderr. systemd/journald captures this with timestamps,
// but we add our own level tag so `journalctl` output is self-describing.
// NB: named `logging`, not `log`, to avoid colliding with the C math builtin
// `log()` that <math.h> drags in (GCC treats `namespace log` as a redeclaration).
namespace logging {

inline void line(const char* level, const char* fmt, va_list ap) {
  char buf[1024];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  fprintf(stderr, "[%s] %s\n", level, buf);
  fflush(stderr);
}

inline void info(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt); line("info", fmt, ap); va_end(ap);
}
inline void warn(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt); line("warn", fmt, ap); va_end(ap);
}
inline void error(const char* fmt, ...) {
  va_list ap; va_start(ap, fmt); line("error", fmt, ap); va_end(ap);
}

} // namespace logging
