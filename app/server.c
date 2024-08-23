#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "routes.h"
#include "thread.h"

void handle_client(int client_fd, AppState *state) {
  printf("Client connected\n");

  uint8_t *in_buf = malloc(sizeof(uint8_t) * MAX_BUFFER);
  memset(in_buf, 0, MAX_BUFFER);

  size_t s = read(client_fd, in_buf, MAX_BUFFER);

  if (s == MAX_BUFFER) {
    printf("read max buffer size -- resizing of buffer required!");
    exit(1);
  }

  HttpRequest req = parse_request(in_buf);

  uint8_t out_buf[MAX_BUFFER];
  memset(out_buf, 0, MAX_BUFFER);

  s = handle_routes(out_buf, &req, state);

  write(client_fd, out_buf, s);

  free_http_request(&req);

  close(client_fd);
}

struct ThreadFunctionHelper {
  int client_fd;
  AppState *state;
};

void thread_function(void *args) {
  struct ThreadFunctionHelper *state = args;
  handle_client(state->client_fd, state->state);
}

int main(int argc, char *argv[]) {
  char *directory = NULL;
  // get directory from
  for (size_t i = 1; i < argc; i += 1) {
    if (strcmp(argv[i], "--directory") == 0) {
      directory = argv[i + 1];
      break;
    }
  }

  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  // You can use print statements as follows for debugging, they'll be visible
  // when running tests.
  printf("Logs from your program will appear here!\n");

  ThreadPool pool = init_threadpool(&thread_function);

  AppState state = {
      .directory = directory,
  };

  int server_fd;
  struct sockaddr_in client_addr;
  socklen_t client_addr_len;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    printf("Socket creation failed: %s...\n", strerror(errno));
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    printf("SO_REUSEADDR failed: %s \n", strerror(errno));
    return 1;
  }

  struct sockaddr_in serv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(4221),
      .sin_addr = {htonl(INADDR_ANY)},
  };

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
    printf("Bind failed: %s \n", strerror(errno));
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    printf("Listen failed: %s \n", strerror(errno));
    return 1;
  }

  printf("Waiting for a client to connect...\n");
  client_addr_len = sizeof(client_addr);

  for (;;) {
    int client_fd_raw =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_fd_raw == -1) {
      break;
    }

    struct ThreadFunctionHelper *tf =
        malloc(sizeof(struct ThreadFunctionHelper));

    assert(tf != NULL);

    tf->client_fd = client_fd_raw;
    tf->state = &state;

    printf("Client connection added to the thread pool");

    // move client to thread pool
    add_threaded_task(&pool, tf);
  }

  free_threadpool(&pool);

  close(server_fd);

  return 0;
}
