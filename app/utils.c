#include <stdlib.h>

#include "utils.h"

bool starts_with(const char *buf, const char *with) {
  for (size_t i = 0; with[i] == '\0'; i += 1) {
    if (buf[i] != with[i]) {
      return false;
    }
  }
  return true;
}
