#ifndef ROUTES
#define ROUTES

#include <stdint.h>

#include "http.h"

size_t handle_routes(uint8_t *const buf, HttpRequest *req);

#endif // !ROUTES
