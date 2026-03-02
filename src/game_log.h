#pragma once

// Game event log — writes notable game events to game_events.txt
// for testing and debugging purposes.

void game_log_init();
void game_log_close();
void game_log(const char* fmt, ...);
