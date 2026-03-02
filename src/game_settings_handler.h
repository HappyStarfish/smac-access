/*
 * GameSettingsHandler - Accessible game settings editor for singleplayer.
 *
 * Opens a modal dialog (Ctrl+F10) in the main menu that allows blind players
 * to configure difficulty, faction, map settings, and game rules before
 * starting a new game. Settings are saved to Alpha Centauri.ini via the
 * game's own prefs_save() function.
 *
 * 4 categories navigated with Tab/Shift+Tab:
 *   1. General (difficulty)
 *   2. Faction (player faction selection + BLURB/DATALINKS reading)
 *   3. Map (size, ocean, land, erosion, clouds, natives)
 *   4. Rules (15 toggleable victory/game rules)
 *
 * Keys:
 *   Tab/Shift+Tab  - switch category
 *   Up/Down        - navigate within category
 *   Left/Right     - change value (categories 1-3)
 *   Enter/Space    - toggle rule (category 4)
 *   D              - read description (faction BLURB, rule description)
 *   I              - read faction info (DATALINKS1)
 *   S              - summary of all settings
 *   Enter          - save and close (when not on a toggle)
 *   Escape         - cancel and close
 *   Ctrl+F1        - help
 */

#pragma once

namespace GameSettingsHandler {

bool IsActive();
void RunModal();
void Update(unsigned int msg, unsigned int wParam);

} // namespace GameSettingsHandler
