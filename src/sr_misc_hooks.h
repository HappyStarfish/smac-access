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

// Hooks for AlphaNet_do_create/do_join (hypothesis 5 approach).
// Do accessible text input, then call original with X_pop returning 0.
// Both are __thiscall with ECX = AlphaNet* this, no stack args.
int __thiscall sr_hook_do_create(void* This);
int __thiscall sr_hook_do_join(void* This);

// X_pop stub for NETCONNECT dialogs. Hooked at call sites inside
// do_create (0x4E2AB8, 0x4E2B0F) and do_join (0x4E2EB6).
// When sr_xpop_skip is set, restores pre-filled buffers and returns 0.
int __cdecl sr_xpop_stub(const char* filename, const char* label,
                          int maxlen, int flags, int extra);
