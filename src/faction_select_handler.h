#pragma once

#include "main.h"

namespace FactionSelectHandler {

/// Returns true if the faction selection screen is currently active.
bool IsActive();

/// Handle key events during faction selection. Returns true if key was consumed.
bool HandleKey(HWND hwnd, UINT msg, WPARAM wParam);

/// Called when a BLURB popup is detected from a faction file.
/// Returns the announcement string (faction name + blurb) for gui.cpp to output.
/// buf/bufsize: output buffer for the announcement.
/// Returns true if announcement was built successfully.
bool OnBlurbDetected(const char* filename, const char* blurb_text,
    char* buf, int bufsize);

/// Called when a non-BLURB BasePop fires (screen changed away from faction select).
void OnNonBlurbPopup();

}
