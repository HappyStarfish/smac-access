#pragma once

#include "main.h"

namespace DiplomacyHandler {

/// Returns true if a diplomacy session is currently active (DiploWinState != 0).
bool IsActive();

/// Called from ModWinProc timer block to detect diplomacy open/close transitions.
void OnTimer();

/// Handle key events during diplomacy. Returns true if key was consumed.
bool HandleKey(UINT msg, WPARAM wParam);

/// Accessible commlink dialog (replaces F12). Shows navigable faction list.
void RunCommlink();

}
