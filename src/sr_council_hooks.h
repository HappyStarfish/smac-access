/*
 * Council vote hooks: governor election, proposal voting, buy-votes.
 * Extracted from screen_reader.cpp — Pattern A full modal replacements.
 */

#pragma once

#include <stdint.h>

// Install council-related inline hooks (call_council, gfx_event_loop, buy_votes).
// tramp_slot must point to executable memory (3 * 32 bytes needed).
// Returns number of hooks installed and advances tramp_slot accordingly.
int sr_install_council_hooks(uint8_t*& tramp_slot);
