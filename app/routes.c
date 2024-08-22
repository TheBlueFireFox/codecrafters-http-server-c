#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "http.h"
#include "routes.h"
#include "utils.h"

typedef const char *HttpParams;

typedef size_t (*fnPtr)(uint8_t *const buf, HttpRequest *req,
                        HttpParams params);

size_t handle_root(uint8_t *const buf, HttpRequest *req, HttpParams params) {
  (void)params;

  HttpResponse resp = {
      .version = HTTP1_1,
      .status = OK,
      .headers = {},
      .body = NULL,
  };

  return write_response(buf, &resp);
}

size_t handle_echo(uint8_t *const buf, HttpRequest *req, HttpParams params) {
  uint8_t body_buf[1024];
  strcpy((char *)body_buf, params);
  HttpBody body = {
      .body = body_buf,
      .len = strlen(params),
  };

  char content_len[100];
  sprintf(content_len, "%zu", body.len);

  HttpHeader content_length = {
      .key = CONTENT_LENGTH,
      .value = content_len,
  };

  Vector_HttpHeader header_vec = init_vector_HttpHeader();
  HttpHeader header_arr[] = {
      {
          .key = CONTENT_TYPE,
          .value = TEXT_PLAIN,
      },
      content_length,
  };

  for (size_t i = 0; i < ARRAY_SIZE(header_arr); i += 1) {
    push_vector_HttpHeader(&header_vec, header_arr[i]);
  }

  HttpHeaders headers = {
      .headers = header_vec,
  };

  HttpResponse resp = {
      .version = HTTP1_1,
      .status = OK,
      .headers = headers,
      .body = body,
  };

  return write_response(buf, &resp);
}

size_t handle_not_found(uint8_t *const buf, HttpRequest *req) {
  HttpResponse resp = {
      .version = HTTP1_1,
      .status = NOT_FOUND,
      .headers = {},
      .body = NULL,
  };

  return write_response(buf, &resp);
}

#define MAX_MATCH_COUNT 1

size_t handle_routes(uint8_t *const buf, HttpRequest *req) {
  fnPtr routes_ptr[] = {
      &handle_root,
      &handle_echo,
  };

  // USING REGEX TO MATCH ROUTES
  char *routes_str[] = {
      "/",
      "/echo/*",
  };

  printf("request for %s\n", req->url);

  for (size_t i = 0; i < ARRAY_SIZE(routes_str); i += 1) {

    size_t res = starts_with_wildcard(req->url, routes_str[i]);

    if (res == ALL_MATCH) {
      printf("match no wildcard -- <%s>\n", routes_str[i]);
      return routes_ptr[i](buf, req, NULL);
    } else if (res == NO_MATCH) {
      continue;
    } else {
      printf("match with wildcard -- <%s>\n", routes_str[i]);
      HttpParams params = req->url + res;
      return routes_ptr[i](buf, req, params);
    }
  }

  printf("NO MATCH FOR <%s>\n", req->url);
  return handle_not_found(buf, req);
}
