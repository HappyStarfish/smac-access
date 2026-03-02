#pragma once

#include "main.h"

namespace OrbitalHandler {

/// Returns true if the modal orbital status screen is active.
bool IsActive();

/// Handle key events during modal loop. Returns true if consumed.
bool Update(UINT msg, WPARAM wParam);

/// Run the modal orbital status screen (called when F6 is pressed on world map).
void RunModal();

}
