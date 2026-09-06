#include <stdarg.h>
#include <stddef.h>

#include "utils.h"

size_t align_up(size_t offset, size_t alignment) {
  return (offset + alignment - 1) & ~(alignment - 1);
}

bool starts_with(const char *buf, const char *with) {
  for (size_t i = 0;; i += 1) {
    if (with[i] == '\0') {
      break;
    }

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
ssize_t starts_with_wildcard(const char *buf, const char *with) {
  ssize_t idx = 0;
  for (;;) {
    char left = *(buf + idx);
    char right = *(with + idx);

    // A > B (no wildcard)
    if (right == WILDCARD) {
      return idx;
    }

    if (left != right) {
      return NO_MATCH;
    }

    // A == B (no wildcard)
    if (left == '\0' && right == '\0') {
      return ALL_MATCH;
    }

    // A > B (no wildcard)
    // A < B (no wildcard)
    if (left == '\0' || right == '\0') {
      return NO_MATCH;
    }

    idx += 1;
  }
}
