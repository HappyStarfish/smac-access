#pragma once

#include "main.h"

namespace ScoreHandler {

/// Returns true if the modal score screen is active.
bool IsActive();

/// Handle key events during modal loop. Returns true if consumed.
bool Update(UINT msg, WPARAM wParam);

/// Run the modal score screen (called when F8 is pressed on world map).
void RunModal();

}
