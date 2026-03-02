#include "main.h"
#include "game_log.h"
#include <cstdarg>
#include <cstdio>

static FILE* game_log_file = NULL;

void game_log_init() {
    if (game_log_file) {
        return;
    }
    game_log_file = fopen("game_events.txt", "w");
    if (!game_log_file) {
        debug("game_log: failed to open game_events.txt\n");
        return;
    }
}

void game_log_close() {
    if (!game_log_file) {
        return;
    }
    fclose(game_log_file);
    game_log_file = NULL;
}

void game_log(const char* fmt, ...) {
    if (!game_log_file) {
        return;
    }
    fprintf(game_log_file, "[Turn %03d] ", *CurrentTurn + 1);

    va_list args;
    va_start(args, fmt);
    vfprintf(game_log_file, fmt, args);
    va_end(args);

    fprintf(game_log_file, "\n");
    fflush(game_log_file);
}
