#pragma once

#include "main.h"

namespace DatalinksHandler {

/// Returns true if the modal Datalinks browser is active.
bool IsActive();

/// Handle key events during modal loop. Returns true if consumed.
bool Update(UINT msg, WPARAM wParam);

/// Run the modal Datalinks browser (called when F1 is pressed on world map).
void RunModal();

}
