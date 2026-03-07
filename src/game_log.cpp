#include "main.h"
#include "game_log.h"
#include "screen_reader.h"
#include <cstdarg>
#include <cstdio>

static FILE* game_log_file = NULL;

void game_log_init() {
    if (game_log_file) {
        return;
    }
    const char* log_dir = sr_get_log_dir();
    char path[MAX_PATH];
    if (log_dir[0]) {
        snprintf(path, sizeof(path), "%sgame_events.txt", log_dir);
    } else {
        snprintf(path, sizeof(path), "game_events.txt");
    }
    game_log_file = fopen(path, "w");
    if (!game_log_file) {
        debug("game_log: failed to open %s\n", path);
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
