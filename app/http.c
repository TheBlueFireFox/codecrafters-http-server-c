#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"

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
}

size_t write_status(uint8_t *const buf, HttpStatus status) {
  switch (status) {
  case OK:
    STRVAL(buf, "200 OK");
  case BAD_REQ:
    STRVAL(buf, "400 Bad Request");
  default:
    printf("INVALID OR NOT SUPPORTED HTTP Status sent");
    exit(1);
  }
}

size_t write_headers(uint8_t *const buf, HttpHeaders *headers) {
  size_t size = 0;
  for (size_t i = 0; i < headers->len; i += 1) {
    const char *const key = headers->headers[i].key;
    const char *const value = headers->headers[i].value;
    size += sprintf((char *)buf + size, "%s: %s" ENDLINE, key, value);
  }
  if (size == 0) {
    size += write_endline(buf);
  }

  return size;
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

  return s;
}
