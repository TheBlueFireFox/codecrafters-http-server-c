#ifndef UTILS
#define UTILS
#include "stdbool.h"
#include "stdlib.h"

#ifdef NDEBUG
#define ASSERT(x)                                                              \
  do {                                                                         \
    (void)sizeof(x);                                                           \
  } while (0)
#else
#include <assert.h>
#define ASSERT(x) assert(x)
#endif

#define ARRAY_SIZE(X) sizeof(X) / sizeof(X[0])

#define WILDCARD '*'

bool starts_with(const char *buf, const char *with);

#define NO_MATCH -1
#define ALL_MATCH 0

// Returns
// - -1 if there is no match at all
// - 0 if everything matches and no wildcards are used
// - n from which offset the wildcard WILDCARD is found
size_t starts_with_wildcard(const char *buf, const char *with);

#endif // !UTILS
