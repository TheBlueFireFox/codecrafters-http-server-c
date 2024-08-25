#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "http.h"
#include "routes.h"
#include "utils.h"

typedef const char *HttpParams;

typedef size_t (*fnPtr)(uint8_t *const buf, HttpRequest *req, HttpParams params,
                        AppState *state);

// SEE: stackoverflow
// https://stackoverflow.com/questions/49622938/gzip-compression-using-zlib-into-buffer
uint8_t *compress_to_gzip(const uint8_t *data, int input_size, int *len) {
  z_stream stream = {0};
  deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 0x1F, 8,
               Z_DEFAULT_STRATEGY);
  size_t max_len = deflateBound(&stream, input_size);
  uint8_t *output = malloc(sizeof(uint8_t) * max_len);
  memset(output, 0, max_len);
  stream.next_in = (Bytef *)data;
  stream.avail_in = input_size;
  stream.next_out = (Bytef *)output;
  stream.avail_out = max_len;
  deflate(&stream, Z_FINISH);
  *len = stream.total_out;
  deflateEnd(&stream);

  return output;
}

size_t write_response_helper(uint8_t *const buf, HttpResponse *resp) {
  HttpBody org_body = resp->body;

  uint8_t *new_buf_body = NULL;

  if (resp->headers.encoding == GZIP && resp->body.body != NULL) {
    HttpHeader content_encoding = {
        .key = CONTENT_ENCODING,
        .value = GZIP_ENCODING,
    };
    push_vector_HttpHeader(&resp->headers.headers, content_encoding);

    int len = 0;
    new_buf_body = compress_to_gzip(org_body.body, org_body.len, &len);
    resp->body = (HttpBody){
        .body = new_buf_body,
        .len = len,
    };
  }

  char content_len[100];
  if (resp->body.body != NULL && resp->body.len > 0) {
    sprintf(content_len, "%zu", resp->body.len);

    HttpHeader content_length = {
        .key = CONTENT_LENGTH,
        .value = content_len,
    };
    push_vector_HttpHeader(&resp->headers.headers, content_length);
  }

  size_t res = write_response(buf, resp);

  resp->body = org_body;
  if (new_buf_body != NULL) {
    free(new_buf_body);
  }

  printf("wrote response\n");
  return res;
}

size_t handle_bad_req(uint8_t *const buf, HttpRequest *req) {

  HttpResponse resp = init_response(BAD_REQ, req->headers.encoding);

  size_t res = write_response(buf, &resp);

  free_http_response(&resp);

  return res;
}

size_t handle_not_found(uint8_t *const buf, HttpRequest *req) {

  HttpResponse resp = init_response(NOT_FOUND, req->headers.encoding);

  size_t res = write_response(buf, &resp);

  free_http_response(&resp);

  return res;
}

size_t handle_root(uint8_t *const buf, HttpRequest *req, HttpParams params,
                   AppState *state) {
  (void)params;
  (void)state;

  HttpResponse resp = init_response(OK, req->headers.encoding);

  size_t res = write_response_helper(buf, &resp);

  free_http_response(&resp);

  return res;
}

size_t handle_echo(uint8_t *const buf, HttpRequest *req, HttpParams params,
                   AppState *state) {
  (void)state;

  HttpResponse resp = init_response(OK, req->headers.encoding);

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

  (void)params;
  (void)state;
  uint8_t body_buf[1024];

  // Assuming there is a user agent header
  const char *user_agent = find_in_header(&req->headers, USER_AGENT);
  strcpy((char *)body_buf, user_agent);

  HttpBody body = {
      .body = body_buf,
      .len = strlen((char *)body_buf),
  };

  HttpHeader content_type = {
      .key = CONTENT_TYPE,
      .value = TEXT_PLAIN,
  };

  HttpResponse resp = init_response(OK, req->headers.encoding);
  push_vector_HttpHeader(&resp.headers.headers, content_type);

  resp.body = body;

  size_t res = write_response_helper(buf, &resp);

  free_http_response(&resp);

  return res;
}

size_t handle_file_get(uint8_t *const buf, HttpRequest *req, HttpParams params,
                       AppState *state) {
  assert(state->directory != NULL);

  char filepath[100];

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

  // alloc correct body size
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

  HttpResponse resp = init_response(OK, req->headers.encoding);
  push_vector_HttpHeader(&resp.headers.headers, content_type);

  resp.body = body;

  res = write_response_helper(buf, &resp);

  free_http_response(&resp);
  free(body_buf);

  return res;
}

size_t handle_file_post(uint8_t *const buf, HttpRequest *req, HttpParams params,
                        AppState *state) {

  assert(state->directory != NULL);

  char filepath[100];

  char *delim =
      params[0] == '/' || state->directory[strlen(state->directory) - 1] == '/'
          ? ""
          : "/";

  sprintf(filepath, "%s%s%s", state->directory, delim, params);

  printf("%s\n", filepath);

  int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  if (fd == -1) {
    printf("INVALID: open returned an error <%i>", errno);
    exit(1);
  }

  write(fd, req->body.body, req->body.len);

  HttpResponse resp = init_response(CREATED, req->headers.encoding);
  size_t res = write_response_helper(buf, &resp);
  free_http_response(&resp);

  return res;
}

#define MAX_MATCH_COUNT 1

struct Route {
  fnPtr fn;
  const char *route;
  HttpMethod method;
};

size_t handle_routes(uint8_t *const buf, HttpRequest *req, AppState *state) {

  struct Route routes[] = {
      {
          .fn = &handle_root,
          .route = "/",
          .method = GET,
      },
      {
          .fn = &handle_echo,
          .route = "/echo/*",
          .method = GET,
      },
      {
          .fn = &handle_user_agent,
          .route = "/user-agent",
          .method = GET,
      },
      {
          .fn = &handle_file_get,
          .route = "/files/*",
          .method = GET,
      },
      {
          .fn = &handle_file_post,
          .route = "/files/*",
          .method = POST,
      },
  };

  printf("request for %s\n", req->url);

  for (size_t i = 0; i < ARRAY_SIZE(routes); i += 1) {
    struct Route *curr = &routes[i];

    size_t res = starts_with_wildcard(req->url, curr->route);
    if (res == (size_t)NO_MATCH) {
      continue;
    } else if (res == (size_t)ALL_MATCH && curr->method == req->method) {
      printf("match no wildcard -- <%s>\n", curr->route);
      return curr->fn(buf, req, NULL, state);
    } else if (curr->method == req->method) {
      printf("match with wildcard -- <%zu> -- <%s>\n", i, curr->route);
      HttpParams params = req->url + res;
      return curr->fn(buf, req, params, state);
    }
  }

  printf("NO MATCH FOR <%s>\n", req->url);
  return handle_not_found(buf, req);
}
