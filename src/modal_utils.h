#pragma once

#include "main.h"

/// Run a modal message pump until *want_close becomes true.
/// Drains leftover WM_CHAR and WM_KEYUP messages after the loop exits.
void sr_run_modal_pump(bool* want_close);

/// Check for global utility keys inside modal loops.
/// Call this on WM_KEYDOWN in any PeekMessage modal. Handles:
///   - Ctrl+F12: toggle debug logging
///   - Ctrl+Shift+R: silence speech
/// Returns true if the key was consumed (caller should skip further processing).
bool sr_modal_handle_utility_key(WPARAM key);
