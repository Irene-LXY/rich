#ifndef MONOPOLY_STARTUP_H
#define MONOPOLY_STARTUP_H

#include <stddef.h>
#include "monopoly/game.h"

typedef enum {
    STARTUP_OK = 0,
    STARTUP_INVALID_ARGUMENT,
    STARTUP_ALREADY_STARTED,
    STARTUP_INTERNAL_ERROR
} StartupResult;

StartupResult application_start(
    Game *game,
    int argument_count,
    char *const arguments[],
    char *message,
    size_t message_size
);

#endif

