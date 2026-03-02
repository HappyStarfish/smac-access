#pragma once

#include "main.h"

namespace MonumentHandler {

/// Returns true if the modal monument screen is active.
bool IsActive();

/// Handle key events during modal loop. Returns true if consumed.
bool Update(UINT msg, WPARAM wParam);

/// Run the modal monument screen (called when F9 is pressed on world map).
void RunModal();

/// Install trampoline hooks on all 16 mon_* functions. Call from patch_setup().
void InstallHooks();

}
