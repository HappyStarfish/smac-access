#pragma once

#include "main.h"
#include "gui.h"

namespace WorldMapHandler {

/// Called from ModWinProc's SR block when sr_is_available().
/// Handles world map transition, map-position tracking, targeting reset,
/// unit-change / "Your Turn" announce, and Trigger 4 polling.
void OnTimer(DWORD now, bool on_world_map, GameWinState cur_win, int cur_popup,
             char* sr_announced, int sr_announced_size);

/// Called for WM_KEYDOWN in ModWinProc. Returns true if the key was consumed.
bool HandleKey(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/// Virtual exploration cursor position (used by other parts of the code).
int GetCursorX();
int GetCursorY();

/// Sync exploration cursor to current unit position.
void SetCursorToUnit();

/// Check if targeting mode is currently active.
bool IsTargetingActive();

}
