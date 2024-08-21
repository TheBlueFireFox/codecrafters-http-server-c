#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "http.h"
#include "routes.h"

typedef size_t (*fnPtr)(uint8_t *const buf, HttpRequest *req);

size_t handle_root(uint8_t *const buf, HttpRequest *req) {
  HttpResponse resp = {
      .version = HTTP1_1,
      .status = OK,
      .headers = {},
      .body = NULL,
  };

  return write_response(buf, &resp);
}

size_t handle_not_found(uint8_t *const buf, HttpRequest *req) {
  // TODO:
  HttpResponse resp = {
      .version = HTTP1_1,
      .status = NOT_FOUND,
      .headers = {},
      .body = NULL,
  };

  return write_response(buf, &resp);
}

size_t handle_routes(uint8_t *const buf, HttpRequest *req) {
  fnPtr routes_ptr[] = {
      &handle_root,
  };

  char *routes_str[] = {
      "/",
  };

  for (size_t i = 0; i < sizeof(routes_str) / sizeof(routes_str[0]); i += 1) {
    if (strcmp(req->url, routes_str[i]) == 0) {
      return routes_ptr[i](buf, req);
    }
  }

  return handle_not_found(buf, req);
}
