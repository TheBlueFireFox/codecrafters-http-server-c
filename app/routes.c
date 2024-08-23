#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "http.h"
#include "routes.h"
#include "utils.h"

typedef const char *HttpParams;

typedef size_t (*fnPtr)(uint8_t *const buf, HttpRequest *req, HttpParams params,
                        AppState *state);

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

size_t handle_not_found(uint8_t *const buf, HttpRequest *req) {
  HttpResponse resp = init_response(NOT_FOUND);

  size_t res = write_response(buf, &resp);

  free_http_response(&resp);

  return res;
}
size_t handle_root(uint8_t *const buf, HttpRequest *req, HttpParams params,
                   AppState *state) {
  (void)params;
  (void)state;

  HttpResponse resp = init_response(OK);

  size_t res = write_response_helper(buf, &resp);

  free_http_response(&resp);

  return res;
}

size_t handle_echo(uint8_t *const buf, HttpRequest *req, HttpParams params,
                   AppState *state) {
  (void)state;

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
                         HttpParams params, AppState *state) {

  (void)state;
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

size_t handle_file(uint8_t *const buf, HttpRequest *req, HttpParams params,
                   AppState *state) {
  // TODO: create file path
  char filepath[100];
  assert(state->directory != NULL);

  char *delim =
      params[0] == '/' || state->directory[strlen(state->directory) - 1] == '/'
          ? ""
          : "/";

  sprintf(filepath, "%s%s%s", state->directory, delim, params);

  struct stat file_stat;
  size_t res = stat(filepath, &file_stat);

  if (res != 0) {
    return handle_not_found(buf, req);
  }

  // TODO: alloc correct body size
  size_t size = file_stat.st_size;

  uint8_t *body_buf = malloc(sizeof(uint8_t) * size);
  assert(body_buf != NULL);

  // read file into buffe
  int fd = open(filepath, O_RDONLY);
  assert(fd != -1);

  size_t size_read = read(fd, body_buf, size);

  assert(size_read == size);

  HttpBody body = {
      .body = body_buf,
      .len = size,
  };

  HttpHeader content_type = {
      .key = CONTENT_TYPE,
      .value = OCTET_STREAM,
  };

  HttpResponse resp = init_response(OK);
  push_vector_HttpHeader(&resp.headers.headers, content_type);

  resp.body = body;

  res = write_response_helper(buf, &resp);

  free_http_response(&resp);
  free(body_buf);

  return res;
}

#define MAX_MATCH_COUNT 1

size_t handle_routes(uint8_t *const buf, HttpRequest *req, AppState *state) {
  fnPtr routes_ptr[] = {
      &handle_root,
      &handle_echo,
      &handle_user_agent,
      &handle_file,
  };

  // USING REGEX TO MATCH ROUTES
  char *routes_str[] = {
      "/",
      "/echo/*",
      "/user-agent",
      "/files/*",
  };

  printf("request for %s\n", req->url);

  for (size_t i = 0; i < ARRAY_SIZE(routes_str); i += 1) {

    size_t res = starts_with_wildcard(req->url, routes_str[i]);

    if (res == ALL_MATCH) {
      printf("match no wildcard -- <%s>\n", routes_str[i]);
      return routes_ptr[i](buf, req, NULL, state);
    } else if (res == NO_MATCH) {
      continue;
    } else {
      printf("match with wildcard -- <%s>\n", routes_str[i]);
      HttpParams params = req->url + res;
      return routes_ptr[i](buf, req, params, state);
    }
  }

  printf("NO MATCH FOR <%s>\n", req->url);
  return handle_not_found(buf, req);
}
