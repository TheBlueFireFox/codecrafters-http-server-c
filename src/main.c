#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "server.h"
#include "utils.h"

atomic_bool is_running = false;

void sig_int_handler(int signum) {
  (void)signum;
  info("sigint <%i>\n", signum);
  atomic_store(&is_running, false);
}

int main(int argc, char *argv[]) {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  signal(SIGINT, sig_int_handler);
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
