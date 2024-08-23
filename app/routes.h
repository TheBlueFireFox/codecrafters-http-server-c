#ifndef ROUTES
#define ROUTES

#include <stdint.h>

#include "http.h"

struct AppState {
  char *directory;
};

typedef struct AppState AppState;

size_t handle_routes(uint8_t *const buf, HttpRequest *req, AppState *state);

#endif // !ROUTES
