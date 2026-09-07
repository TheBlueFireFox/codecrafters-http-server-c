#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

#include "client.h"
#include "utils.h"

static size_t load_request(int client_fd, uint8_t **in_buf, size_t *buffer_size,
                           HttpRequest *req) {
  size_t read_size = 0;

  while (1) {
    // there should be enough space for the initial request headers
    read_size = read(client_fd, *in_buf + read_size, (size_t)INITIAL_BUFFER);

    if (read_size == 0) {
      break;
    }

    *req = parse_request(*in_buf);

    if (read_size <= *buffer_size) {
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
    memset((*in_buf) + read_size, 0, (*buffer_size) - read_size);
  }

  return read_size;
}

static bool handle_client_request(int client_fd, uint8_t **in_buf,
                                  uint8_t *out_buf, size_t *buffer_size,
                                  AppState *state) {
  HttpRequest req;

  size_t size = load_request(client_fd, in_buf, buffer_size, &req);

  if (size == 0) {
    return false;
  }

  size = handle_routes(out_buf, &req, state);

  write(client_fd, out_buf, size);

  bool active = req.headers.connection.active;

  free_http_request(&req);
  return active;
}

static const suseconds_t INTERVAL = 500;
static const size_t MAX_TIMEOUT_US = INTERVAL * 10;

enum LoopInnerState {
  Contiinue,
  Break,
  Client,
};

static enum LoopInnerState handle_client_loop_inner(struct pollfd *fds,
                                                    size_t fds_count,
                                                    size_t *iter_count) {
  *iter_count -= INTERVAL;

  int ret = poll(fds, fds_count, (int)INTERVAL);

  if (ret == 0) {
    // we timed out back to looping
    return Contiinue;
  }

  if (ret == -1 && errno != EINTR) {
    error("ERROR: poll() errored out\n");
  }

  if (ret == -1) {
    return Break;
  }

  *iter_count = MAX_TIMEOUT_US;
  return Client;
}

static void handle_client_loop(int client_fd, uint8_t **in_buf,
                               uint8_t *out_buf, size_t org_buffer_size,
                               AppState *state, atomic_bool *server_running) {
  struct pollfd fds[1] = {{
      .fd = client_fd,
      .events = POLLIN,
      .revents = 0,
  }};
  size_t *buffer_size = &org_buffer_size;

  // counts iterations between messages => creates a timeout after a while
  size_t iter_count = MAX_TIMEOUT_US;

  while (atomic_load(server_running) && iter_count > 0) {
    enum LoopInnerState loop_state =
        handle_client_loop_inner(fds, ARRAY_SIZE(fds), &iter_count);
    if (loop_state == Contiinue) {
      continue;
    }

    if (loop_state == Break) {
      break;
    }

    if (!handle_client_request(client_fd, in_buf, out_buf, buffer_size,
                               state)) {
      break;
    }
  }
}

void handle_client(int client_fd, AppState *state,
                   atomic_bool *server_running) {

  debug("Client connected to thread_id <%lu>\n", thrd_current());

  uint8_t *in_buf = calloc((size_t)INITIAL_BUFFER, sizeof(uint8_t));
  uint8_t *out_buf = calloc((size_t)INITIAL_BUFFER, sizeof(uint8_t));

  handle_client_loop(client_fd, &in_buf, out_buf, (size_t)INITIAL_BUFFER, state,
                     server_running);

  shutdown(client_fd, SHUT_RDWR);

  while (1) {
    size_t read_size = read(client_fd, in_buf, (size_t)INITIAL_BUFFER);
    if (read_size == 0) {
      break;
    }
  }

  debug("Client connection closed from thread_id <%lu>\n", thrd_current());

  free(out_buf);
  free(in_buf);

  close(client_fd);
}
