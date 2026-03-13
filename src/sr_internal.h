/*
 * Internal shared declarations for screen_reader split files.
 * NOT part of the public API — only include from sr_*.cpp files.
 */

#pragma once

#include <windows.h>
#include <stdint.h>

// Install an inline hook at target_addr, redirecting to hook_func.
// tramp_slot must point to 32 bytes of executable memory for the trampoline.
// Returns trampoline pointer (to call original), or NULL on failure.
void* install_inline_hook(uint32_t target_addr, void* hook_func, uint8_t* tramp_slot);

// Log a message to the persistent hook log file (access_hooks.log).
void sr_hook_log(const char* msg);

// Log a message to the session log file.
void sr_log(const char* msg);
