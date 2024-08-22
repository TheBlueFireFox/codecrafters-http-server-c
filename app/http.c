#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "utils.h"

#define ENDLINE "\r\n"

#define STRVAL(X, Y)                                                           \
  memcpy(X, Y, strlen(Y));                                                     \
  return strlen(Y)

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

size_t write_endline(uint8_t *const buf) {
  memcpy(buf, ENDLINE, strlen(ENDLINE));
  return strlen(ENDLINE);
}

size_t write_version(uint8_t *const buf, HttpVersion status) {
  switch (status) {
  case HTTP1_1:
    STRVAL(buf, "HTTP/1.1");
  }
  return 0;
}

size_t write_status(uint8_t *const buf, HttpStatus status) {
  switch (status) {
  case OK:
    STRVAL(buf, "200 OK");
  case BAD_REQ:
    STRVAL(buf, "400 Bad Request");
  case NOT_FOUND:
    STRVAL(buf, "404 Not Found");
  default:
    printf("INVALID OR NOT SUPPORTED HTTP Status sent");
    exit(1);
  }
}

size_t write_headers(uint8_t *const buf, HttpHeaders *headers) {
  size_t size = 0;
  for (size_t i = 0; i < headers->headers.len; i += 1) {
    const char *const key = headers->headers.ptr[i].key;
    const char *const value = headers->headers.ptr[i].value;
    size += sprintf((char *)buf + size, "%s: %s" ENDLINE, key, value);
  }
  size += write_endline(buf + size);

  return size;
}

size_t write_body(uint8_t *const buf, HttpBody *body) {
  memcpy(buf, body->body, body->len);
  return body->len;
}

size_t write_response(uint8_t *const buf, HttpResponse *resp) {
  size_t s = 0;
  s += write_version(buf, resp->version);
  buf[s] = ' ';
  buf[s + 1] = '\0';
  s += 1;
  s += write_status(buf + s, resp->status);
  s += write_endline(buf + s);
  s += write_headers(buf + s, &resp->headers);
  s += write_body(buf + s, &resp->body);

  return s;
}

size_t parse_method(const uint8_t *buf, HttpMethod *meth) {
  char *methods_str[] = {"GET", "POST"};
  HttpMethod methods_enum[] = {GET, POST};

  for (size_t i = 0; i < sizeof(methods_enum) / sizeof(methods_enum[0]);
       i += 1) {
    if (starts_with((char *)buf, methods_str[i])) {
      *meth = methods_enum[i];
      return strlen(methods_str[i]);
    }
  }

  printf("INVALID OR NOT SUPPORTED HTTP METHOD");
  exit(1);
}

size_t parse_version(const uint8_t *buf, HttpVersion *version) {
  const char *const VERSION = "HTTP/1.1" ENDLINE;

  if (!starts_with((char *)buf, VERSION)) {
    printf("INVALID: line does not stop with \\r\\n");
    exit(1);
  }

  *version = HTTP1_1;

  return strlen(VERSION);
}
// // Headers
// Host: localhost:4221\r\n     // Header that specifies the server's host and
// port User-Agent: curl/7.64.1\r\n  // Header that describes the client's user
// agent Accept: */*\r\n              // Header that specifies which media types
// the client can accept \r\n                         // CRLF that marks the end
// of the headers

size_t parse_headers(const uint8_t *buf, HttpHeaders *headers) {
  size_t s = 0;
  for (;;) {
    // printf("%s", (char *)buf + s);
    // end of headers
    if (*(buf + s) == '\r' && *(buf + s + 1) == '\n') {
      s += 2;
      break;
    }
    // otherwise headers
    // key
    char *key = (char *)buf + s;
    char *value = strstr((char *)buf + s, ": ");
    *value = '\0';

    // value
    value += 2;
    char *end_value = strstr((char *)value, "\r\n");
    *end_value = '\0';

    // + 2 for \r\n

    s += end_value - key + 2;

    HttpHeader header = {
        .key = key,
        .value = value,
    };

    push_vector_HttpHeader(&headers->headers, header);
  }
  return s;
}

HttpRequest parse_request(const uint8_t *buf) {
  // GET                          // HTTP method
  // /index.html                  // Request target
  // HTTP/1.1                     // HTTP version
  // \r\n                         // CRLF that marks the end of the request line
  HttpMethod method;
  size_t s = parse_method(buf, &method);
  if (buf[s] != ' ') {
    printf("INVALID HTTP string");
    exit(1);
  }
  s += 1;

  const char *url = (char *)buf + s;
  char *end = strstr((char *)buf + s, " ");
  // allow the url to automatically work
  *end = '\0';

  s += strlen(url) + 1;

  if (!starts_with((char *)buf + s, ENDLINE)) {
    printf("INVALID: line does not stop with \\r\\n");
    exit(1);
  }

  HttpVersion version;
  s += parse_version(buf, &version);

  HttpHeaders headers = {
      .headers = init_vector_HttpHeader(),
  };

  s += parse_headers(buf + s, &headers);

  if (!starts_with((char *)buf + s, ENDLINE)) {
    printf("INVALID: header don't not stop with \\r\\n");
    exit(1);
  }

  HttpRequest req = {
      .method = GET,
      .url = url,
      .version = version,
      .headers = headers,
      .body = buf + s,
  };

  return req;
}

void free_http_request(HttpRequest *req) {
  free_vector_HttpHeader(&req->headers.headers);
}

HttpResponse init_response(HttpStatus status) {
  HttpHeaders headers = {
      .headers = init_vector_HttpHeader(),
  };
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

void free_http_response(HttpResponse *resp) {
  free_vector_HttpHeader(&resp->headers.headers);
}
