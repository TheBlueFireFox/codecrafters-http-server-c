#include "client.h"
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>

size_t load_request(int client_fd, uint8_t **in_buf, size_t *buffer_size,
                    HttpRequest *req) {
  size_t s = 0;

  while (1) {
    // there should be enough space for the initial request headers
    s = read(client_fd, *in_buf + s, INITIAL_BUFFER);

    if (s == 0) {
      break;
    }

    *req = parse_request(*in_buf);

    if (s <= *buffer_size) {
      // there might be a body or we are large enough at this point :)
      break;
    }

    // get new lenght requirement
    if (req->body.len > 0) {
      // there is a body attached to this msg
      //
      // body attached check msg buffer size and if required expand it
      // some more
      size_t header_len = *in_buf - req->body.body;
      *buffer_size = header_len + req->body.len;
      *in_buf = realloc(*in_buf, *buffer_size);
      memset((*in_buf) + s, 0, (*buffer_size) - s);
    }
  }

  return s;
}

void handle_client_requests(int client_fd, uint8_t **in_buf, uint8_t *out_buf,
                            size_t org_buffer_size, AppState *state) {
  HttpRequest req;
  size_t *buffer_size = &org_buffer_size;

  bool active = true;

  while (active) {
    req = (HttpRequest){0};

    size_t s = load_request(client_fd, in_buf, buffer_size, &req);

    if (s == 0) {
      break;
    }

    s = handle_routes(out_buf, &req, state);

    write(client_fd, out_buf, s);

    active = req.headers.connection.active;

    free_http_request(&req);
  }
}


void handle_client(int client_fd, AppState *state) {

  pthread_t self = pthread_self();

  printf("Client connected to thread_id <%lu>\n", self);

  uint8_t *in_buf = calloc(INITIAL_BUFFER, sizeof(uint8_t));
  uint8_t *out_buf = calloc(INITIAL_BUFFER, sizeof(uint8_t));

  handle_client_requests(client_fd, &in_buf, out_buf, INITIAL_BUFFER, state);

  shutdown(client_fd, SHUT_RDWR);

  while (1) {
    size_t s = read(client_fd, in_buf, INITIAL_BUFFER);
    if (s == 0)
      break;
  }

  printf("Client connection closed from thread_id <%lu>\n", self);

  free(out_buf);
  free(in_buf);

  close(client_fd);
}
