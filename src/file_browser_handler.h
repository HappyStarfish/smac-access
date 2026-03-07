#pragma once

#include "main.h"

namespace FileBrowserHandler {

bool IsActive();
bool Update(UINT msg, WPARAM wParam);

// Run modal file browser. Returns selected path (relative to game dir,
// e.g. "saves/auto/Autosave_2101.sav") or empty string if cancelled.
// is_save: false=load, true=save
const char* RunModal(bool is_save);

}
