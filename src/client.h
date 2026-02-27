#ifndef CLIENT_H
#define CLIENT_H
#include <stdatomic.h>

#include "routes.h"

#define INITIAL_BUFFER (16 * 1014)

void handle_client(int client_fd, AppState *state, atomic_bool *server_running);

#endif
