#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#if defined(__GLIBCXX__) && !defined(__UCLIBC__) && !defined(_OS_ANDROID)
#include <cxxabi.h>
#include <execinfo.h>
#endif

#if defined(_OS_ANDROID)
#define LOG_TAG "LOGCAT"
#include <android/log.h>
#endif

#include <os_log.h>
#include <os_assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOG_BUFFER_SIZE  (4096)

static int32_t gs_log_level = eLogVerbose;

int32_t log_setlevel(int32_t level) {
  gs_log_level = level;
  return gs_log_level;
}

int32_t log_getlevel() {
  return gs_log_level;
}

#define CONSOLE_COLOR_R "\033[0;31m"
#define CONSOLE_COLOR_G "\033[0;32m"
#define CONSOLE_COLOR_Y "\033[0;33m"
#define CONSOLE_COLOR_E "\033[m"

static void os_printf(int32_t level, const char *tag, const char *fmt, va_list args) {
#if defined(_OS_ANDROID)
  int32_t log_level = ANDROID_LOG_INFO;
  switch(level) {
    case eLogVerbose:
      log_level = ANDROID_LOG_VERBOSE;
    break;
    case eLogInfo:
      log_level = ANDROID_LOG_INFO;
    break;
    case eLogWarn:
      log_level = ANDROID_LOG_WARN;
    break;
    case eLogError:
      log_level = ANDROID_LOG_ERROR;
    break;
  }
  __android_log_vprint(log_level, tag, fmt, args);
#endif
  const char *pcolor = CONSOLE_COLOR_R;
  switch(level) {
    case eLogVerbose:
    case eLogInfo:
      pcolor = CONSOLE_COLOR_G;
    break;
    case eLogWarn:
      pcolor = CONSOLE_COLOR_Y;
    break;
    case eLogError:
      pcolor = CONSOLE_COLOR_R;
    break;
  }

  fprintf(stderr, "[%s%s%s]", pcolor, tag, CONSOLE_COLOR_E);
  vfprintf(stderr, fmt, args);
}

void log_trace(int32_t level, const char *tag, const char *fmt, ...) {
  if (gs_log_level > level)
    return;
  va_list args;
  va_start(args, fmt);
  os_printf(level, tag, fmt, args);
  va_end(args);
}

void log_verbose(const char *tag, const char *fmt, ...) {
  if (gs_log_level > eLogVerbose)
    return;
  va_list args;
  va_start(args, fmt);
  os_printf(eLogVerbose, tag, fmt, args);
  va_end(args);

}
void log_info(const char *tag, const char *fmt, ...) {
  if (gs_log_level > eLogInfo)
    return;
  va_list args;
  va_start(args, fmt);
  os_printf(eLogInfo, tag, fmt, args);
  va_end(args);

}
void log_warn(const char *tag, const char *fmt, ...) {
  if (gs_log_level > eLogWarn)
    return;
  va_list args;
  va_start(args, fmt);
  os_printf(eLogWarn, tag, fmt, args);
  va_end(args);

}
void log_error(const char *tag, const char *fmt, ...) {
  if (gs_log_level > eLogError)
    return;
  va_list args;
  va_start(args, fmt);
  os_printf(eLogError, tag, fmt, args);
  va_end(args);
}

static void dump_backtrace() {
#if defined(__GLIBCXX__) && !defined(__UCLIBC__) && !defined(_OS_ANDROID)
  void* trace[100];
  int32_t size = backtrace(trace, sizeof(trace) / sizeof(*trace));
  char** symbols = backtrace_symbols(trace, size);
  log_error("backtrace", "\n==== C stack trace ===============================\n\n");
  if (size == 0) {
    log_error("backtrace", "(empty)\n");
  } else if (symbols == NULL) {
    log_error("backtrace", "(no symbols)\n");
  } else {
    for (int i = 1; i < size; ++i) {
      char mangled[201];
      if (sscanf(symbols[i], "%*[^(]%*[(]%200[^)+]", mangled) == 1) {
        log_error("backtrace", "%2d: ", i);
        int status;
        size_t length;
        char* demangled = abi::__cxa_demangle(mangled, (char *)NULL, &length, &status);
        log_error("backtrace", "%s\n", demangled != (char *)NULL ? demangled : mangled);
        free(demangled);
      } else {
        // If parsing failed, at least print the unparsed symbol.
        log_error("backtrace", "%s\n", symbols[i]);
      }
    }
  }
  free(symbols);
#endif
}

#ifdef __cplusplus
} // extern "C"
#endif

namespace os {

FatalLog::FatalLog(const char* file, int line) {
  _stream << std::endl << std::endl << "#" << std::endl << "# Fatal error in "
        << file << ", line " << line << std::endl << "# ";
}

FatalLog::FatalLog(const char* file, int line, std::string * str) {
  _stream << std::endl << std::endl << "#" << std::endl << "# Fatal error in "
        << file << ", line " << line << std::endl << "# ";

  _stream << "Check failed: " << *str << std::endl << "# ";
}

FatalLog::~FatalLog() {
  fflush(stdout);
  fflush(stderr);
  _stream << std::endl << "#" << std::endl;
  log_error("FatalLog", _stream.str().c_str());
  dump_backtrace();
  fflush(stderr);
  abort();
}

} //namespace os
