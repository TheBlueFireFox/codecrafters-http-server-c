#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "routes.h"
#include "thread.h"
#include "utils.h"

struct ThreadFunctionHelper {
  int client_fd;
  atomic_bool *server_running;
  AppState *state;
};

void thread_function(void *args) {
  struct ThreadFunctionHelper *state = args;
  handle_client(state->client_fd, state->state, state->server_running);
  free(state);
}

void send_task(int client_fd, AppState *state, atomic_bool *server_running,
               ThreadPool *pool) {

  struct ThreadFunctionHelper *tf = malloc(sizeof(struct ThreadFunctionHelper));

  ASSERT(tf != NULL);

  tf->client_fd = client_fd;
  tf->state = state;
  tf->server_running = server_running;

  debug("Client connection added to the thread pool\n");

  // move client to thread pool
  add_threaded_task(pool, tf);
}

const char *PORT = "4221";

static int server_loop(int fd, AppState *state, ThreadPool *pool,
                       atomic_bool *is_running) {

  struct pollfd fds[] = {
      {
          .fd = fd,
          .events = POLLIN,
          .revents = 0,
      },
  };

  struct sockaddr_storage client_addr;

  socklen_t client_addr_len = sizeof(client_addr);

  while (atomic_load(is_running)) {
    int ret = poll(fds, ARRAY_SIZE(fds), -1);

    if (ret == -1) {
      if (errno == EINTR)
        break;

      warn("ERROR: poll() errord out\n");
      break;
    } else if (ret == 0) {
      continue;
    }

    int client_fd = -1;

    if (fds[0].revents & POLLIN) {
      client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_addr_len);
      debug("connected\n");
    } else {
      warn("no connections ready to process\n");
      continue;
    }

    if (client_fd == -1) {
      break;
    }

    send_task(client_fd, state, is_running, pool);
  }

  atomic_store(is_running, false);

  return 0;
}

#define SOCKET_RES_BREAK 0
#define SOCKET_RES_ERROR 1
#define SOCKET_RES_CONTINE 2

static int setup_socket(struct addrinfo *p, int *server_fd) {
  *server_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
  if (*server_fd == -1) {
    error("Socket creation failed: %s...\n", strerror(errno));
    return SOCKET_RES_CONTINE;
  }

  // Allow reuse of address
  int reuse = 1;
  if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse) ==
      -1) {
    error("SO_REUSEADDR failed: %s \n", strerror(errno));
    close(*server_fd);
    return SOCKET_RES_ERROR;
  }

  // Try binding
  if (bind(*server_fd, p->ai_addr, p->ai_addrlen) == -1) {
    error("Bind failed: %s \n", strerror(errno));
    close(*server_fd);
    return SOCKET_RES_CONTINE;
  }

  const int connection_backlog = 50;
  if (listen(*server_fd, connection_backlog) != 0) {
    error("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  return SOCKET_RES_BREAK;
}

static int init_binding(int *server_fd) {
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *p;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP socket
  hints.ai_flags = AI_PASSIVE;     // Use my IP

  // Get address info
  if ((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
    error("getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // Loop through all results and bind to the first we can
  for (p = res; p != NULL; p = p->ai_next) {
    rv = setup_socket(p, server_fd);

    if (rv == SOCKET_RES_BREAK) {
      break; // Successfully bound
    } else if (rv == SOCKET_RES_ERROR) {
      p = NULL;
      break;
    } else if (rv == SOCKET_RES_CONTINE) {
      continue;
    }
  }

  if (p == NULL) {
    fprintf(stderr, "Failed to bind\n");
    freeaddrinfo(res);
    return 1;
  }

  freeaddrinfo(res);
  return 0;
}

int start_server(AppState *state, atomic_bool *is_running) {
  info("server online\n");

  ThreadPool pool = init_threadpool(&thread_function, THREADPOOL_SIZE);

  int server_fd;

  if (init_binding(&server_fd) != 0) {
    free_threadpool(&pool);
    return 1;
  }

  debug("Waiting for a client to connect...\n");

  server_loop(server_fd, state, &pool, is_running);

  close(server_fd);

  free_threadpool(&pool);

  return 0;
}
