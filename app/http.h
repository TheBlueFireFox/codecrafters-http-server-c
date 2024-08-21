#ifndef HTTP
#define HTTP

#include <stddef.h>
#include <stdint.h>

#include "vector.h"

#define MAX_BUFFER 20 * 1024

enum HttpVersion {
  HTTP1_1,
};

typedef enum HttpVersion HttpVersion;

size_t write_version(uint8_t *const buf, HttpVersion status);

enum HttpStatus {
  OK,
  BAD_REQ,
  NOT_FOUND,
};

typedef enum HttpStatus HttpStatus;

size_t write_status(uint8_t *const buf, HttpStatus status);

struct HttpHeader {
  char *key;
  char *value;
};

typedef struct HttpHeader HttpHeader;

INIT_VECTOR(HttpHeader);

struct HttpHeaders {
  Vector_HttpHeader headers;
};

typedef struct HttpHeaders HttpHeaders;

size_t write_headers(uint8_t *const buf, HttpHeaders *headers);

typedef uint8_t *HttpBody;

struct HttpResponse {
  HttpVersion version;
  HttpStatus status;
  HttpHeaders headers;
  HttpBody body;
};

typedef struct HttpResponse HttpResponse;

size_t write_response(uint8_t *const buf, HttpResponse *resp);

// // Request line
// GET                          // HTTP method
// /index.html                  // Request target
// HTTP/1.1                     // HTTP version
// \r\n                         // CRLF that marks the end of the request line
//
// // Headers
// Host: localhost:4221\r\n     // Header that specifies the server's host and
// port User-Agent: curl/7.64.1\r\n  // Header that describes the client's user
// agent Accept: */*\r\n              // Header that specifies which media types
// the client can accept \r\n                         // CRLF that marks the end
// of the headers
//
// // Request body (empty)

enum HttpMethod {
  GET,
  POST,
};

typedef enum HttpMethod HttpMethod;

struct HttpRequest {
  HttpMethod method;
  const char *const url;
  HttpVersion version;
  HttpHeaders headers;
  const uint8_t *body;
};

typedef struct HttpRequest HttpRequest;

HttpRequest parse_request(const uint8_t *buf);

void free_http_request(HttpRequest *req);

#endif // !HTTP
