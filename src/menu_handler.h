#pragma once

#include "main.h"
#include "gui.h"

namespace MenuHandler {

/// Returns true if the menu bar navigation is currently active.
bool IsActive();

/// Called for WM_KEYDOWN in ModWinProc. Returns true if the key was consumed.
bool HandleKey(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/// Called when leaving the world map — deactivates menu bar.
void OnLeaveWorldMap();

/// Handle UP/DOWN arrow keys in dialog/menu contexts (mcache logic).
/// Returns true if the key was consumed (cache hit).
/// Sets sr_arrow_active/dir/time on cache miss for Trigger 2 fallback.
bool OnArrowKey(WPARAM dir, DWORD now, bool on_world_map, GameWinState cur_win,
                bool& sr_arrow_active, WPARAM& sr_arrow_dir, DWORD& sr_arrow_time);

/// Invalidate the menu item cache (called on non-navigation keys, screen changes).
void InvalidateCache();

/// Check if the menu item cache is active.
bool IsCacheActive();

/// Store a text into the mcache at current position (called by Trigger 2).
void CacheStore(int pos, const char* text);

/// Store text at a specific position (for leaving-item cache).
void CacheStoreAt(int pos, const char* text);

/// Get current mcache position.
int CacheGetPos();

/// Debug log helper for mcache.
void McLog(const char* fmt, ...);

}
