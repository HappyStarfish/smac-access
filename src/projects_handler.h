#pragma once

#include "main.h"

namespace ProjectsHandler {

/// Returns true if the modal secret projects screen is active.
bool IsActive();

/// Handle key events during modal loop. Returns true if consumed.
bool Update(UINT msg, WPARAM wParam);

/// Run the modal secret projects screen (called when F5 is pressed on world map).
void RunModal();

}
