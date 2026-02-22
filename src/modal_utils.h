#pragma once

#include "main.h"

/// Run a modal message pump until *want_close becomes true.
/// Drains leftover WM_CHAR and WM_KEYUP messages after the loop exits.
void sr_run_modal_pump(bool* want_close);
