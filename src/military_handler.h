#pragma once

#include "main.h"

namespace MilitaryHandler {

/// Returns true if the modal military status screen is active.
bool IsActive();

/// Handle key events during modal loop. Returns true if consumed.
bool Update(UINT msg, WPARAM wParam);

/// Run the modal military status screen (called when F7 is pressed on world map).
void RunModal();

}
