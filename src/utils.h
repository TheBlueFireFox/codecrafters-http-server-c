#ifndef UTILS
#define UTILS
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef NDEBUG
#define ASSERT(x)                                                              \
  do {                                                                         \
    (void)sizeof(x);                                                           \
  } while (0)
#else
#include <assert.h>
#define ASSERT(x) assert(x)
#endif

enum Level {
  DEBUG,
  INFO,
  WARN,
  ERROR,
};

#ifdef NDEBUG
#define LEVEL WARN
#else
#define LEVEL INFO
#endif

static inline const char *level_name(enum Level level) {
  switch (level) {
  case DEBUG:
    return "Debug";
  case INFO:
    return "Info";
  case WARN:
    return "Warn";
  case ERROR:
    return "Error";
  default:
    return "Unknown";
  }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 5, 6)))
#endif
static inline void log_impl(enum Level level, const char *file, int line,
                            const char *func, const char *fmt, ...) {
  if (level > ERROR || level < LEVEL) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  (void)fprintf(stderr, "%s:%s:%d:%s(): ", level_name(level), file, line, func);
  (void)vfprintf(stderr, fmt, args);
  va_end(args);
}

#define log(level, fmt, ...)                                                   \
  log_impl((level), __FILE_NAME__, __LINE__, __func__,                         \
           (fmt)__VA_OPT__(, ) __VA_ARGS__)

#define debug(fmt, ...) log(DEBUG, fmt, __VA_ARGS__)
#define info(fmt, ...) log(INFO, fmt, __VA_ARGS__)
#define warn(fmt, ...) log(WARN, fmt, __VA_ARGS__)
#define error(fmt, ...) log(ERROR, fmt, __VA_ARGS__)

#define ARRAY_SIZE(X) (sizeof(X) / sizeof((X)[0]))

#define WILDCARD '*'

bool starts_with(const char *buf, const char *with);

#define NO_MATCH (-1)
#define ALL_MATCH 0

// Returns
// - -1 if there is no match at all
// - 0 if everything matches and no wildcards are used
// - n from which offset the wildcard WILDCARD is found
ssize_t starts_with_wildcard(const char *buf, const char *with);

#endif // !UTILS
