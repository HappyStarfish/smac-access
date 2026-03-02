#pragma once

#include "main.h"

namespace LabsHandler {

/// Returns true if the modal labs status screen is active.
bool IsActive();

/// Handle key events during modal loop. Returns true if consumed.
bool Update(UINT msg, WPARAM wParam);

/// Run the modal labs status screen (called when F2 is pressed on world map).
void RunModal();

}
