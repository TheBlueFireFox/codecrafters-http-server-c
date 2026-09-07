#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "server.h"

static atomic_bool is_running = false;

static void sig_int_handler(int signum) {
  (void)signum;
  atomic_store(&is_running, false);
}

int main(int argc, char *argv[]) {
  // Disable output buffering
  (void)setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  (void)setvbuf(stderr, NULL, _IONBF, BUFSIZ);

  (void)signal(SIGINT, sig_int_handler);
  atomic_store(&is_running, true);

  const char *directory = "/tmp";
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

  int res = start_server(&state, &is_running);

  return res;
}
