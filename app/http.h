#ifndef HTTP
#define HTTP

#include <stddef.h>
#include <stdint.h>

#define MAX_BUFFER 20 * 1024

enum HttpVersion {
  HTTP1_1,
};

typedef enum HttpVersion HttpVersion;

size_t write_version(uint8_t *const buf, HttpVersion status);

enum HttpStatus {
  OK,
  BAD_REQ,
};

typedef enum HttpStatus HttpStatus;

size_t write_status(uint8_t *const buf, HttpStatus status);

struct HttpHeader {
  char *key;
  char *value;
};

typedef struct HttpHeader HttpHeader;

struct HttpHeaders {
  HttpHeader *headers;
  size_t len;
};

typedef struct HttpHeaders HttpHeaders;

size_t write_headers(uint8_t *const buf, HttpHeaders* headers);

typedef uint8_t *HttpBody;

struct HttpResponse {
  HttpVersion version;
  HttpStatus status;
  HttpHeaders headers;
  HttpBody body;
};

typedef struct HttpResponse HttpResponse;

size_t write_response(uint8_t *const buf, HttpResponse *resp);

#endif // !HTTP
