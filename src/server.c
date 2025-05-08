#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
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

  printf("Client connection added to the thread pool\n");

  // move client to thread pool
  add_threaded_task(pool, tf);
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

  const int connection_backlog = 50;
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

int init_bindings(int *server_fd_ipv4, int *server_fd_ipv6) {
  if (bind_ipv4(server_fd_ipv4) != 0) {
    printf("Unable to bind ipv4\n");
    return 1;
  }
  printf("Bound ipv4 at %d\n", PORT);

  if (bind_ipv6(server_fd_ipv6) != 0) {
    close(*server_fd_ipv4);
    printf("Unable to bind ipv6\n");
    return 1;
  }
  printf("Bound ipv6 at %d\n", PORT);

  return 0;
}

int server_loop(int fd_ipv4, int fd_ipv6, AppState *state, ThreadPool *pool,
                atomic_bool *is_running) {

  struct pollfd fds[2] = {{
                              .fd = fd_ipv4,
                              .events = POLLIN,
                              .revents = 0,
                          },
                          {
                              .fd = fd_ipv6,
                              .events = POLLIN,
                              .revents = 0,
                          }};

  struct sockaddr_in client_addr;
  struct sockaddr_in6 client_addr_v6;

  socklen_t client_addr_len = sizeof(client_addr);
  socklen_t client_addr_len_v6 = sizeof(client_addr_len_v6);

  while (atomic_load(is_running)) {
    int ret = poll(fds, ARRAY_SIZE(fds), 500);

    if (ret == -1 && errno == EINTR) {
      break;
    } else if (ret == -1) {
      printf("ERROR: poll() errored out\n");
      break;
    } else if (ret == 0) {
      continue;
    }

    int client_fd = -1;

    if (fds[0].revents & POLLIN) {
      client_fd =
          accept(fd_ipv4, (struct sockaddr *)&client_addr, &client_addr_len);
      printf("connected via IPv4\n");
    } else if (fds[1].revents & POLLIN) {
      client_fd = accept(fd_ipv6, (struct sockaddr *)&client_addr_v6,
                         &client_addr_len_v6);
      printf("connected via IPv6\n");
    } else {
      printf("no connections ready to process\n");
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

int start_server(AppState *state, atomic_bool *is_running) {
  printf("ONLINE\n");

  ThreadPool pool = init_threadpool(&thread_function, THREADPOOL_SIZE);

  int server_fd_ipv4;
  int server_fd_ipv6;

  if (init_bindings(&server_fd_ipv4, &server_fd_ipv6) != 0) {
    free_threadpool(&pool);
    return 1;
  }

  printf("Waiting for a client to connect...\n");

  server_loop(server_fd_ipv4, server_fd_ipv6, state, &pool, is_running);

  close(server_fd_ipv6);
  close(server_fd_ipv4);

  free_threadpool(&pool);

  return 0;
}
