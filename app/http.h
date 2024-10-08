#ifndef HTTP
#define HTTP

#include <stddef.h>
#include <stdint.h>

#include "vector.h"

enum HttpVersion {
  HTTP1_1,
};

typedef enum HttpVersion HttpVersion;

size_t write_version(uint8_t *const buf, HttpVersion status);

enum HttpStatus {
  OK,
  BAD_REQ,
  CREATED,
  NOT_FOUND,
};

typedef enum HttpStatus HttpStatus;

size_t write_status(uint8_t *const buf, HttpStatus status);

struct HttpHeader {
  const char *key;
  const char *value;
};

typedef struct HttpHeader HttpHeader;

enum HttpContentEncoding {
  GZIP,
  NO_ENCODING,
};

typedef enum HttpContentEncoding HttpContentEncoding;

INIT_VECTOR(HttpHeader);

struct HttpHeaders {
  Vector_HttpHeader headers;
  HttpContentEncoding encoding;
};

typedef struct HttpHeaders HttpHeaders;

const char *find_in_header(HttpHeaders *headers, const char *const key);

void push_header_headers(HttpHeaders *headers, const char *const key, const char*const value);

size_t write_headers(uint8_t *const buf, HttpHeaders *headers);

struct HttpBody {
  const uint8_t *body;
  size_t len;
};

typedef struct HttpBody HttpBody;

struct HttpResponse {
  HttpVersion version;
  HttpStatus status;
  HttpHeaders headers;
  HttpBody body;
};

typedef struct HttpResponse HttpResponse;

HttpResponse init_response(HttpStatus status, HttpContentEncoding encoding);
void push_header_response(HttpResponse *resp, const char* const key, const char* const value);
void free_http_response(HttpResponse *resp);

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
  const char *url;
  HttpVersion version;
  HttpHeaders headers;
  HttpBody body;
};

typedef struct HttpRequest HttpRequest;

HttpRequest parse_request(const uint8_t *buf);

void free_http_request(HttpRequest *req);

// headers
#define CONTENT_TYPE "Content-Type"
#define CONTENT_LENGTH "Content-Length"
#define USER_AGENT "User-Agent"
#define CONTENT_ENCODING "Content-Encoding"
#define ACCEPT_ENCODING "Accept-Encoding"

// content types
#define TEXT_PLAIN "text/plain"
#define OCTET_STREAM "application/octet-stream"

// encodings
#define GZIP_ENCODING "gzip"

#endif // !HTTP
