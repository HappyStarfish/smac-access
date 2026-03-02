#pragma once

#include "main.h"

namespace StatusHandler {

/// Returns true if the modal base operations list is active.
bool IsActive();

/// Handle key events during modal loop. Returns true if consumed.
bool Update(UINT msg, WPARAM wParam);

/// Run the modal base operations list (called when F4 is pressed).
void RunModal();

}
