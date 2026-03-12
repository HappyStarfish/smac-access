/*
 * Screen reader integration for Thinker Accessibility Mod.
 * Uses Tolk library for screen reader output (NVDA, JAWS, etc.)
 * Tolk.dll is loaded dynamically at runtime via LoadLibrary.
 */

#pragma once

#include <windows.h>

// Initialize screen reader (load Tolk.dll)
bool sr_init();

// Shutdown screen reader (unload Tolk.dll)
void sr_shutdown();

// Output text to screen reader (ANSI string, auto-converts to wide)
bool sr_output(const char* text, bool interrupt = false);

// Output text to screen reader (wide string)
bool sr_output_w(const wchar_t* text, bool interrupt = false);

// Silence current speech
bool sr_silence();

// Check if screen reader is available
bool sr_is_available();

// Check if all SR features are disabled (no_hooks.txt or runtime toggle)
bool sr_all_disabled();

// Runtime toggle for accessibility (Ctrl+Shift+A)
void sr_set_disabled(bool disabled);
bool sr_get_disabled();

// Install hooks on Buffer_write_l family to capture game text
bool sr_install_text_hooks();

// Get the most recent captured text (for hotkey readback)
const char* sr_get_last_text();

// Clear the captured text buffer
void sr_clear_text();

// Called by hooks to record drawn text (x,y = screen position, -1 if unknown)
// buf = Buffer pointer that text was drawn to (for identifying child buffers)
void sr_record_text(const char* text, int x = -1, int y = -1, void* buf = nullptr);

// Individual item tracking (for dialog/menu navigation)
int sr_item_count();
const char* sr_item_get(int index);
int sr_item_get_x(int index);
int sr_item_get_y(int index);
void* sr_item_get_buf(int index);
void sr_items_clear();

// Timestamp of last text capture (for deferred announce)
DWORD sr_get_last_record_time();

// Snapshot: previous draw cycle's items (saved when buffer auto-clears)
// Use to announce screens that transition too fast for the deferred timer.
bool sr_snapshot_ready();
int sr_snapshot_count();
const char* sr_snapshot_get(int index);
int sr_snapshot_get_y(int index);
void* sr_snapshot_get_buf(int index);
void sr_snapshot_consume();

// Force save current items to snapshot and clear all buffers.
// Used by multiplayer handler to get fresh capture data via forced redraw.
void sr_force_snapshot();

// Read popup body text from a game text file (filename.txt, #LABEL)
// If substitute=false, skip game variable substitution (caller handles it).
bool sr_read_popup_text(const char* filename, const char* label,
                        char* buf, int bufsize, bool substitute = true);

// Defer announce: text capture continues but gui.cpp should skip announce triggers.
// Used by blocking hooks (planetfall) that announce all text after return.
bool sr_defer_active();
void sr_defer_set(bool active);

// Accessible menu modal: replaces startup menus with keyboard navigation.
// Returns 0-based index of selected item, or -1 if cancelled.
int sr_accessible_menu_modal(const char* title,
    char items[][256], int count);

// Popup list navigation: tracks selectable options in popup dialogs
// that use highlight bars instead of re-rendering text on arrow nav.
// Options are parsed from the script file (blank line separates
// description from options, or #itemlist directive).
#define SR_POPUP_LIST_MAX 16
struct SrPopupList {
    char items[SR_POPUP_LIST_MAX][256];
    char label[64];   // dialog label (e.g. "NETCONNECT_CREATE")
    int count;
    int index;      // current selection (0-based)
    bool active;
};
extern SrPopupList sr_popup_list;
void sr_popup_list_clear();
// Parse options from a script file section. Returns number of options found.
int sr_popup_list_parse(const char* filename, const char* label);

// Timestamp of last tutorial popup announcement (suppress worldmap trigger)
extern DWORD sr_tutorial_announce_time;

// Check if a popup hook is currently active (blocks duplicate announcements)
bool sr_popup_is_active();
void sr_popup_set_active(bool active);

// Number input modal: check if active, process keys from ModWinProc
bool sr_is_number_input_active();
void sr_number_input_update(UINT msg, WPARAM wParam);

// File browser: load/save dialog accessibility
bool sr_file_browser_active();
void sr_fb_on_text_captured(const char* text); // announce file/folder with type marker

// Debug logging (Ctrl+F12 toggle at runtime)
bool sr_debug_active();
void sr_debug_toggle();
void sr_debug_log(const char* fmt, ...);

// Returns path to Logs/ subdirectory in game folder (with trailing backslash).
// Creates the directory if it doesn't exist. Returns empty string on failure.
const char* sr_get_log_dir();

// Netsetup coordinate diagnostic mode: when true, hooks log x,y for every text render
extern bool sr_netsetup_log_coords;

// Multiplayer no-dedup: when true, skip strstr dedup in sr_record_text
// so that repeated player names ("Computer") at different y-coords are all captured.
extern bool sr_mp_no_dedup;

// Multiplayer Start Game: set true before GraphicWin_close to signal
// that create_game should return success (game start, not cancel).
extern bool sr_net_start_requested;

// Speech history: browse recent announcements (Ctrl+Shift+F11/F12)
void sr_history_add(const char* text);
int sr_history_count();
const char* sr_history_get(int offset); // 0 = newest, 1 = second newest, ...
void sr_history_set_browsing(bool active);

// Pending setup rules from accessible RULES modal.
// Set by sr_intercept_rules_popup, applied in init_world_config.
extern uint32_t sr_pending_setup_rules;
extern bool sr_pending_setup_rules_set;

// Current tech_id being processed by tech_achieved. -1 when not active.
// Used by tech_achieved_pop3 to fix $TECH0 variable resolution.
extern int sr_current_tech_achieved_id;

// Convert Windows-1252 (ANSI) game text to UTF-8.
// Used for all text originating from the game engine (Buffer_write hooks,
// game data arrays like Bases[].name, Facility[].name, etc.)
void sr_ansi_to_utf8(const char* src, char* dst, int dstsize);

// Substitute game variables in a text buffer ($WORD#, $NUM#, $<N:form0:...>).
void sr_substitute_game_vars(char* buf, int bufsize);

// Convenience wrapper: convert a game string to UTF-8 using rotating buffers.
// Returns a pointer valid until 8 more calls. Safe to use multiple times
// in a single snprintf call (up to 8 game strings per call).
const char* sr_game_str(const char* ansi_text);
