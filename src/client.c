#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

#include "client.h"
#include "utils.h"

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
    if (req->body.len == 0) {
      continue;
    }
    // there is a body attached to this msg
    //
    // body attached check msg buffer size and if required expand it
    // some more
    size_t header_len = *in_buf - req->body.body;
    *buffer_size = header_len + req->body.len;
    *in_buf = realloc(*in_buf, *buffer_size);
    memset((*in_buf) + s, 0, (*buffer_size) - s);
  }

  return s;
}

bool handle_client_request(int client_fd, uint8_t **in_buf, uint8_t *out_buf,
                           size_t *buffer_size, AppState *state) {
  HttpRequest req = {0};

  size_t s = load_request(client_fd, in_buf, buffer_size, &req);

  if (s == 0) {
    return false;
  }

  s = handle_routes(out_buf, &req, state);

  write(client_fd, out_buf, s);

  bool active = req.headers.connection.active;

  free_http_request(&req);
  return active;
}

void handle_client_loop(int client_fd, uint8_t **in_buf, uint8_t *out_buf,
                        size_t org_buffer_size, AppState *state,
                        atomic_bool *server_running) {
  struct pollfd fds[1] = {{
      .fd = client_fd,
      .events = POLLIN,
      .revents = 0,
  }};
  size_t *buffer_size = &org_buffer_size;

  const suseconds_t INTERVAL = 500;
  const size_t MAX_TIMEOUT_US = INTERVAL * 10;

  // counts iterations between messages => creates a timeout after a while
  size_t iterCount = MAX_TIMEOUT_US;

  while (atomic_load(server_running) && iterCount > 0) {
    iterCount -= INTERVAL;

    int ret = poll(fds, ARRAY_SIZE(fds), INTERVAL);

    if (ret == 0) {
      // we timed out back to looping
      continue;
    } else if (ret == -1) {
      if (errno != EINTR) {
        error("ERROR: poll() errored out\n");
      }
      break;
    }

    iterCount = MAX_TIMEOUT_US;
    if (!handle_client_request(client_fd, in_buf, out_buf, buffer_size,
                               state)) {
      break;
    }
  }
}

void handle_client(int client_fd, AppState *state,
                   atomic_bool *server_running) {

  thrd_t self = thrd_current();

  debug("Client connected to thread_id <%lu>\n", self);

  uint8_t *in_buf = calloc(INITIAL_BUFFER, sizeof(uint8_t));
  uint8_t *out_buf = calloc(INITIAL_BUFFER, sizeof(uint8_t));

  handle_client_loop(client_fd, &in_buf, out_buf, INITIAL_BUFFER, state,
                     server_running);

  shutdown(client_fd, SHUT_RDWR);

  while (1) {
    size_t s = read(client_fd, in_buf, INITIAL_BUFFER);
    if (s == 0)
      break;
  }

  debug("Client connection closed from thread_id <%lu>\n", self);

  free(out_buf);
  free(in_buf);

  close(client_fd);
}
