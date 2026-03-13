/*
 * Miscellaneous game hooks: tech_achieved, planetfall, file browser, number input.
 * Extracted from screen_reader.cpp.
 */

#pragma once

#include <stdint.h>

// Install misc hooks (tech_achieved, planetfall, load_game, save_game, pop_ask_number).
// tramp_slot must point to executable memory (4 * 32 bytes needed for inline hooks).
// Returns number of hooks installed and advances tramp_slot accordingly.
int sr_install_misc_hooks(uint8_t*& tramp_slot);
