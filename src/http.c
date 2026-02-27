#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "utils.h"

#define ENDLINE "\r\n"

#define STRVAL(X, Y)                                                           \
  do {                                                                         \
    size_t __slen_ = strlen(Y);                                                \
    memcpy(X, Y, __slen_);                                                     \
    return __slen_;                                                            \
  } while (0);

#define MAP(X, Y, Z)                                                           \
  case X:                                                                      \
    STRVAL(Z, Y);

// cmpheaders is the reference function used by qsort to sort
// all the HttpHeaders.
static int cmpheaders(const void *left, const void *right) {
  //  The  contents of the array are sorted in ascending order according to a
  //  comparison function pointed to by compar, which is called with two
  //  arguments that point to the objects being compared.
  //
  //  The comparison function must return an integer less than, equal to, or
  //  greater than zero if the first argument is considered to be respec‐ tively
  //  less than, equal to, or greater than the second.  If two members compare
  //  as equal, their order in the sorted array is undefined.
  const HttpHeader *header1 = left;
  const HttpHeader *header2 = right;
  return strcasecmp(header1->key, header2->key);
}

// comkey is the reference function used by bsearch to find the matching header
// pair
static int cmpkey(const void *raw_key, const void *raw_elem) {
  //   The contents of the array should be in ascending sorted order according
  //   to the comparison function referenced by compkey. The compkey routine is
  //   expected to have two arguments which point to the key object and to an
  //   array member, in that order, and should return an integer less than,
  //   equal to, or greater than zero if the key object is found, respectively,
  //   to be less than, to match, or be greater than the  array member.
  const char *const *key = (const char *const *)raw_key;
  const HttpHeader *elem = raw_elem;
  return strcasecmp(*key, elem->key);
}

// sort_headers is a helper function that makes sure to sort the http headers
// if they are not in a sorted order (this uses is_sorted to determine if the
// array requires sorting)
static void sort_headers(HttpHeaders *headers) {
  if (!headers->is_sorted) {
    sort_vector(&headers->headers, cmpheaders);
  }
  headers->is_sorted = true;
}

// find_in_header will sort if required search through all the header keys for
// the fitting key
//
// RETURN: NULL if not found or a ptr to the value
const char *find_in_header(HttpHeaders *headers, const char *const key) {
  sort_headers(headers);

  const HttpHeader *res = search_vector(&headers->headers, &key, cmpkey);

  if (res == NULL) {
    return NULL;
  }
  return res->value;
}

void push_header_headers(HttpHeaders *headers, const char *const key,
                         const char *const value) {
  headers->is_sorted = false;

  HttpHeader header = {
      .value = value,
      .key = key,
  };

  push_vector(&headers->headers, header);
}

// // Status line
// HTTP/1.1  // HTTP version
// 200       // Status code
// OK        // Optional reason phrase
// \r\n      // CRLF that marks the end of the status line
//
// // Headers (empty)
// \r\n      // CRLF that marks the end of the headers
//
// // Response body (empty)

size_t write_endline(uint8_t *const buf) { STRVAL(buf, ENDLINE); }

size_t write_version(uint8_t *const buf, HttpVersion status) {
  switch (status) { MAP(HTTP1_1, "HTTP/1.1", buf) }
  return 0;
}

size_t write_status(uint8_t *const buf, HttpStatus status) {
  switch (status) {
    MAP(OK, "200 OK", buf);
    MAP(CREATED, "201 Created", buf);
    MAP(BAD_REQ, "400 Bad Request", buf);
    MAP(NOT_FOUND, "404 Not Found", buf);
  default:
    warn("INVALID OR NOT SUPPORTED HTTP Status sent");
    exit(1);
  }
}

size_t write_headers(uint8_t *const buf, HttpHeaders *headers) {
  size_t size = 0;
  for (each_vector(header, &headers->headers)) {
    size += sprintf((char *)buf + size, "%s: %s" ENDLINE, header->key,
                    header->value);
  }

  size += write_endline(buf + size);

  return size;
}

size_t write_body(uint8_t *const buf, HttpBody *body) {
  memcpy(buf, body->body, body->len);
  return body->len;
}

size_t write_response(uint8_t *const buf, HttpResponse *resp) {
  size_t offset = 0;
  offset += write_version(buf, resp->version);
  buf[offset] = ' ';
  offset += 1;
  offset += write_status(buf + offset, resp->status);
  offset += write_endline(buf + offset);
  offset += write_headers(buf + offset, &resp->headers);
  offset += write_body(buf + offset, &resp->body);

  return offset;
}

size_t parse_method(const uint8_t *buf, HttpMethod *meth) {
  const char *const methods_str[] = {"GET", "POST"};
  HttpMethod methods_enum[] = {GET, POST};

  for (size_t i = 0; i < ARRAY_SIZE(methods_enum); i += 1) {
    bool res = starts_with((const char *)buf, methods_str[i]);
    if (res) {
      *meth = methods_enum[i];
      return strlen(methods_str[i]);
    }
  }

  error("INVALID OR NOT SUPPORTED HTTP METHOD");
  exit(1);
}

size_t parse_version(const uint8_t *buf, HttpVersion *version) {
  const char *const VERSION = "HTTP/1.1";

  if (!starts_with((const char *)buf, VERSION)) {
    error("INVALID: missing VERSION\n");
    exit(1);
  }

  *version = HTTP1_1;

  return strlen(VERSION);
}

// Headers
// Host: localhost:4221\r\n     // Header that specifies the server's host and
// User-Agent: curl/7.64.1\r\n  // Header that describes the client's user
// Accept: */*\r\n              // Header that specifies which media types
size_t parse_headers(uint8_t *buf, HttpHeaders *headers) {
  size_t offset = 0;
  // end of headers
  while (!(*(buf + offset) == '\r' && *(buf + offset + 1) == '\n')) {
    // otherwise headers
    // key
    char *key = (char *)buf + offset;
    char *value = strstr((char *)buf + offset, ": ");
    *value = '\0';

    // value
    value += 2;
    char *end_value = strstr(value, "\r\n");
    *end_value = '\0';

    // + 2 for \r\n

    offset += end_value - key + 2;

    push_header_headers(headers, key, value);
  }

  const char *content_encoding = find_in_header(headers, ACCEPT_ENCODING);

  // just see if gzip is requested
  if (content_encoding != NULL &&
      strstr(content_encoding, GZIP_ENCODING) != NULL) {
    headers->encoding = GZIP;
  }

  return offset;
}

size_t convert_to_int(const char *content_len) {
  if (content_len == NULL) {
    return 0;
  }
  // there is a body attached to this msg
  errno = 0;
  const int BASE = 10;
  size_t res = strtoll(content_len, NULL, BASE);

  if (errno != 0) {
    perror("strtol");
    exit(EXIT_FAILURE);
  }
  return res;
}

size_t parse_request_line(uint8_t *buf, HttpMethod *method,
                          HttpVersion *version, const char **url) {
  // GET                          // HTTP method
  // /index.html                  // Request target
  // HTTP/1.1                     // HTTP version
  // \r\n                         // CRLF that marks the end of the request line
  size_t offset = parse_method(buf, method);

  if (buf[offset] != ' ') {
    error("INVALID: HTTP string\n");
    exit(1);
  }
  offset += 1;

  *url = (char *)buf + offset;
  char *end = strstr((char *)buf + offset, " ");
  // allow the url to automatically work
  *end = '\0';

  offset += strlen(*url) + 1;

  offset += parse_version(buf + offset, version);

  if (!starts_with((const char *)buf + offset, ENDLINE)) {
    error("INVALID: line does not stop with \\r\\n\n");
    exit(1);
  }

  offset += 2;
  return offset;
}

HttpRequest parse_request(uint8_t *buf) {

  HttpMethod method = GET;
  HttpVersion version = HTTP1_1;
  const char *url = NULL;
  size_t offset = parse_request_line(buf, &method, &version, &url);

  HttpHeaders headers = {.encoding = NO_ENCODING,
                         .connection = {.active = true},
                         .is_sorted = false};

  init_vector(&headers.headers);

  offset += parse_headers(buf + offset, &headers);

  if (!starts_with((const char *)buf + offset, ENDLINE)) {
    error("INVALID: headers don't stop with \\r\\n\n");
    exit(EXIT_FAILURE);
  }

  offset += 2;

  const char *connection_state = find_in_header(&headers, CONNECTION);

  // Connection is kept open or not
  headers.connection.active =
      ((connection_state == NULL ||
        strcmp(connection_state, CONNECTION_CLOSE) != 0) != 0);

  const char *content_len = find_in_header(&headers, CONTENT_LENGTH);

  HttpBody body = {
      .body = buf + offset,
      .len = convert_to_int(content_len),
  };

  HttpRequest req = {
      .method = method,
      .url = url,
      .version = version,
      .headers = headers,
      .body = body,
  };

  return req;
}

void free_http_request(HttpRequest *req) { free_vector(&req->headers.headers); }

HttpResponse init_response(HttpStatus status, HttpContentEncoding encoding,
                           HttpConnectionState connection_status) {
  HttpHeaders headers = {
      .encoding = encoding, .connection = connection_status, .is_sorted = true};
  init_vector(&headers.headers);

  const char *key = CONNECTION;
  const char *value = CONNECTION_ALIVE;

  if (!connection_status.active) {
    value = CONNECTION_CLOSE;
  }

  push_header_headers(&headers, key, value);

  HttpBody body = {
      .body = NULL,
      .len = 0,
  };

  HttpResponse resp = {
      .version = HTTP1_1,
      .status = status,
      .headers = headers,
      .body = body,
  };

  return resp;
}

void push_header_response(HttpResponse *resp, const char *const key,
                          const char *const value) {
  push_header_headers(&resp->headers, key, value);
}

void free_http_response(HttpResponse *resp) {
  free_vector(&resp->headers.headers);
}
