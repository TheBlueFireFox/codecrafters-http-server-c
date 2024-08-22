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

size_t write_response_helper(uint8_t *const buf, HttpResponse *resp) {
  char content_len[100];
  if (resp->body.body != NULL && resp->body.len > 0) {
    sprintf(content_len, "%zu", resp->body.len);

    HttpHeader content_length = {
        .key = CONTENT_LENGTH,
        .value = content_len,
    };
    push_vector_HttpHeader(&resp->headers.headers, content_length);
  }

  return write_response(buf, resp);
}

size_t handle_root(uint8_t *const buf, HttpRequest *req, HttpParams params) {
  (void)params;

  HttpResponse resp = init_response(OK);

  size_t res = write_response_helper(buf, &resp);

  free_http_response(&resp);

  return res;
}

size_t handle_echo(uint8_t *const buf, HttpRequest *req, HttpParams params) {
  HttpResponse resp = init_response(OK);

  uint8_t body_buf[1024];
  strcpy((char *)body_buf, params);

  HttpBody body = {
      .body = body_buf,
      .len = strlen(params),
  };

  HttpHeader content_type = {
      .key = CONTENT_TYPE,
      .value = TEXT_PLAIN,
  };

  push_vector_HttpHeader(&resp.headers.headers, content_type);

  resp.body = body;

  size_t res = write_response_helper(buf, &resp);

  free_http_response(&resp);

  return res;
}

size_t handle_user_agent(uint8_t *const buf, HttpRequest *req,
                         HttpParams params) {

  uint8_t body_buf[1024];

  // Assuming there is a user agent header
  for (size_t i = 0; i < req->headers.headers.len; i += 1) {
    HttpHeader *header = &req->headers.headers.ptr[i];

    if (strcmp(header->key, USER_AGENT) == 0) {
      strcpy((char *)body_buf, header->value);
      break;
    }
  }

  HttpBody body = {
      .body = body_buf,
      .len = strlen((char *)body_buf),
  };

  HttpHeader content_type = {
      .key = CONTENT_TYPE,
      .value = TEXT_PLAIN,
  };

  HttpResponse resp = init_response(OK);
  push_vector_HttpHeader(&resp.headers.headers, content_type);

  resp.body = body;

  size_t res = write_response_helper(buf, &resp);

  free_http_response(&resp);

  return res;
}

size_t handle_not_found(uint8_t *const buf, HttpRequest *req) {
  HttpResponse resp = init_response(NOT_FOUND);

  size_t res = write_response(buf, &resp);

  free_http_response(&resp);

  return res;
}

#define MAX_MATCH_COUNT 1

size_t handle_routes(uint8_t *const buf, HttpRequest *req) {
  fnPtr routes_ptr[] = {
      &handle_root,
      &handle_echo,
      &handle_user_agent,
  };

  // USING REGEX TO MATCH ROUTES
  char *routes_str[] = {
      "/",
      "/echo/*",
      "/user-agent",
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
