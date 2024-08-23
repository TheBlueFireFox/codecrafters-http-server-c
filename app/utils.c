#include <assert.h>
#include <stddef.h>

#include "utils.h"

bool starts_with(const char *buf, const char *with) {
  for (size_t i = 0; with[i] == '\0'; i += 1) {
    if (buf[i] != with[i]) {
      return false;
    }
  }
  return true;
}

// Returns
// - -1 if there is no match at all
// - 0 if everything matches and no wildcards are used
// - n from which offset the wildcard WILDCARD is found
size_t starts_with_wildcard(const char *buf, const char *with) {
  // TODO:
  size_t i = 0;
  while (1) {
    char a = *(buf + i);
    char b = *(with + i);

    // A == B (no wildcard)
    if (a == '\0' && b == '\0') {
      return ALL_MATCH;
    }

    // A > B (no wildcard)
    // A < B (no wildcard)
    if (a == '\0' || b == '\0') {
      return NO_MATCH;
    }

    // A > B (no wildcard)
    if (b == WILDCARD) {
      return i;
    }

    if (a != b) {
      return NO_MATCH;
    }

    i += 1;
  }
}
