#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "routes.h"
#include "thread.h"

#define INITIAL_BUFFER 16 * 1014

bool is_running = true;

void sig_int_handler(int signum) {
  (void)signum;
  printf("sigint <%i>\n", signum);
  is_running = false;
}

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

struct ThreadFunctionHelper {
  int client_fd;
  AppState *state;
};

void thread_function(void *args) {
  struct ThreadFunctionHelper *state = args;
  handle_client(state->client_fd, state->state);
  free(state);
}

int internal_bind(int *server_fd, int domain, struct sockaddr *addr,
                  size_t addr_size) {
  *server_fd = socket(domain, SOCK_STREAM, 0);
  if (*server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  if (bind(*server_fd, addr, addr_size) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  const int connection_backlog = 5;
  if (listen(*server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  return 0;
}

const uint16_t PORT = 4221;

int bind_ipv4(int *server_fd) {
  struct sockaddr_in s_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(PORT),
      .sin_addr = {.s_addr = INADDR_ANY},
  };

  return internal_bind(server_fd, AF_INET, (struct sockaddr *)&s_addr,
                       sizeof(s_addr));
}

int bind_ipv6(int *server_fd) {
  struct sockaddr_in6 s_addr = {
      .sin6_family = AF_INET6,
      .sin6_port = htons(PORT),
      .sin6_addr = in6addr_loopback,
  };

  return internal_bind(server_fd, AF_INET6, (struct sockaddr *)&s_addr,
                       sizeof(s_addr));
}

void send_task(int client_fd, AppState *state, ThreadPool *pool) {

  struct ThreadFunctionHelper *tf = malloc(sizeof(struct ThreadFunctionHelper));

  assert(tf != NULL);

  tf->client_fd = client_fd;
  tf->state = state;

  printf("Client connection added to the thread pool\n");

  // move client to thread pool
  add_threaded_task(pool, tf);
}

int init_bindings(int *server_fd_ipv4, int *server_fd_ipv6) {
  if (bind_ipv4(server_fd_ipv4) != 0) {
    printf("Unable to bind ipv4\n");
    return 1;
  }

  if (bind_ipv6(server_fd_ipv6) != 0) {
    close(*server_fd_ipv4);
    printf("Unable to bind ipv6\n");
    return 1;
  }

  return 0;
}

int server_loop(int server_fd_ipv4, int server_fd_ipv6, AppState *state,
                ThreadPool *pool) {
  fd_set rfds;
  FD_ZERO(&rfds);

  const int max_server_fd =
      server_fd_ipv4 > server_fd_ipv6 ? server_fd_ipv4 : server_fd_ipv6;

  // set select time on the socket
  struct timeval tv = {
      .tv_sec = 0,
      .tv_usec = 500000,
  };

  struct sockaddr_in client_addr;
  struct sockaddr_in6 client_addr_v6;

  socklen_t client_addr_len = sizeof(client_addr);
  socklen_t client_addr_len_v6 = sizeof(client_addr_len_v6);

  while (is_running) {

    FD_SET(server_fd_ipv4, &rfds);
    FD_SET(server_fd_ipv6, &rfds);

    int ret = select(max_server_fd + 1, &rfds, NULL, NULL, &tv);

    if (ret == -1 && errno == EINTR) {
      break;
    } else if (ret == -1) {
      printf("ERROR: select() errored out\n");
      is_running = false;
      break;
    } else if (ret == 0) {
      continue;
    }

    int client_fd = -1;

    if (FD_ISSET(server_fd_ipv4, &rfds)) {
      client_fd = accept(server_fd_ipv4, (struct sockaddr *)&client_addr,
                         &client_addr_len);
    } else if (FD_ISSET(server_fd_ipv6, &rfds)) {
      client_fd = accept(server_fd_ipv6, (struct sockaddr *)&client_addr_v6,
                         &client_addr_len_v6);
    } else {
      printf("no connections ready to process\n");
      continue;
    }

    if (client_fd == -1) {
      break;
    }

    send_task(client_fd, state, pool);
  }

  return 0;
}

int start_server(AppState *state) {
  printf("ONLINE\n");

  ThreadPool pool = init_threadpool(&thread_function);

  int server_fd_ipv4;
  int server_fd_ipv6;

  if (init_bindings(&server_fd_ipv4, &server_fd_ipv6) != 0) {
    free_threadpool(&pool);
    return 1;
  }

  printf("Waiting for a client to connect...\n");

  server_loop(server_fd_ipv4, server_fd_ipv6, state, &pool);

  close(server_fd_ipv6);
  close(server_fd_ipv4);

  free_threadpool(&pool);

  return 0;
}

int main(int argc, char *argv[]) {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  signal(SIGINT, sig_int_handler);

  char *directory = "/tmp";
  // get directory from
  for (int i = 1; i < argc; i += 1) {
    if (strcmp(argv[i], "--directory") == 0) {
      directory = argv[i + 1];
      break;
    }
  }

  AppState state = {
      .directory = directory,
  };

  return start_server(&state);
}
