#ifndef SERVER_H
#define SERVER_H
#include <stdatomic.h>

#include "routes.h"

int start_server(AppState *state, atomic_bool *is_running);

#endif
