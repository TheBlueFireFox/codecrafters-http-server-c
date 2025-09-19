#ifndef UTILS
#define UTILS
#include "stdbool.h"
#include "stdlib.h"
#include "stdio.h"

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

#define log(level, fmt, ...)                                                   \
  do {                                                                         \
    /* __VA_OPT__(, ) __VA_ARGS__ allows for optional __VA_ARGS__ only C23 */  \
    if (level >= LEVEL) {                                                      \
      const char *ll;                                                          \
      switch (level) {                                                         \
      case DEBUG:                                                              \
        ll = "Debug";                                                          \
        break;                                                                 \
      case INFO:                                                               \
        ll = "Info";                                                           \
        break;                                                                 \
      case WARN:                                                               \
        ll = "Warn";                                                           \
        break;                                                                 \
      case ERROR:                                                              \
        ll = "Error";                                                          \
        break;                                                                 \
      }                                                                        \
      fprintf(stderr, "%s:%s:%d:%s(): " fmt, ll, __FILE_NAME__, __LINE__,      \
              __func__ __VA_OPT__(, ) __VA_ARGS__);                            \
    }                                                                          \
  } while (0)

#define debug(fmt, ...) log(DEBUG, fmt, __VA_ARGS__)
#define info(fmt, ...) log(INFO, fmt, __VA_ARGS__)
#define warn(fmt, ...) log(WARN, fmt, __VA_ARGS__)
#define error(fmt, ...) log(ERROR, fmt, __VA_ARGS__)

#define ARRAY_SIZE(X) sizeof(X) / sizeof(X[0])

#define WILDCARD '*'

bool starts_with(const char *buf, const char *with);

#define NO_MATCH -1
#define ALL_MATCH 0

// Returns
// - -1 if there is no match at all
// - 0 if everything matches and no wildcards are used
// - n from which offset the wildcard WILDCARD is found
ssize_t starts_with_wildcard(const char *buf, const char *with);

#endif // !UTILS
