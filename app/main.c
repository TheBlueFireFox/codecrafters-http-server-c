#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "server.h"

bool is_running = true;

void sig_int_handler(int signum) {
  (void)signum;
  printf("sigint <%i>\n", signum);
  is_running = false;
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

  return start_server(&state, &is_running);
}
