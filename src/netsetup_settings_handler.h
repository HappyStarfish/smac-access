/*
 * NetSetupSettingsHandler - Accessible multiplayer lobby settings editor.
 *
 * Ctrl+Shift+F10 in the NetWin lobby opens a modal dialog with 2 categories:
 *   1. Settings (8 dropdown items: difficulty, time, game type, etc.)
 *   2. Rules (18 checkbox items: simultaneous, victory types, etc.)
 *
 * On save (Enter), applies changes via IAT-hooked GetAsyncKeyState to
 * interact with the game's native popup menus, plus simulate_click for
 * checkboxes. On cancel (Escape), discards all changes.
 *
 * Keys:
 *   Tab/Shift+Tab  - switch category
 *   Up/Down        - navigate within category
 *   Left/Right     - change value (category 1) / toggle (category 2)
 *   Enter          - save and close
 *   Space          - toggle checkbox (category 2)
 *   S              - summary
 *   Escape         - cancel and close
 *   Ctrl+F1        - help
 */

#pragma once

// Global flag for IAT hook — true during apply phase
extern bool sr_fake_lbutton;

namespace NetSetupSettingsHandler {

bool IsActive();
void RunModal();
void RunModalAt(int category, int item);
void Update(unsigned int msg, unsigned int wParam);

} // namespace NetSetupSettingsHandler
