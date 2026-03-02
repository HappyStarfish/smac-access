/*
 * Multiplayer Setup Handler — keyboard accessibility for the NetWin lobby.
 *
 * Uses direct WinProc calls with WM_LBUTTONDOWN/UP to trigger the game's
 * own click handlers. This bypasses DDrawCompat's synthetic mouse blocking
 * because it's a direct function call, not a Windows message.
 *
 * - Settings: WinProc click opens game's native popup (sr_popup_list handles)
 * - Checkboxes: WinProc click toggles game's internal state
 * - Buttons: WinProc click triggers game's Start/Cancel handlers
 *
 * 4 zones: Settings, Players, Checkboxes, Buttons
 * Tab/Shift+Tab cycles zones. Up/Down navigates within zone.
 * Enter/Space activates.
 */

#include "main.h"
#include "multiplayer_handler.h"
#include "netsetup_settings_handler.h"
#include "screen_reader.h"
#include "localization.h"

// Game globals (from engine.h / engine.cpp)
extern Win* NetWin;
extern int* const GameHalted;
extern HWND* phWnd;

// From gui.cpp
extern int __thiscall Win_is_visible(Win* This);

// Forward declaration for popup list state
extern SrPopupList sr_popup_list;

namespace MultiplayerHandler {

// --- Zone definitions ---
enum Zone { ZONE_SETTINGS = 0, ZONE_PLAYERS, ZONE_CHECKBOXES, ZONE_BUTTONS, ZONE_COUNT };

// Setting items (left side, click opens popup)
struct SettingItem {
    SrStr nameKey;
    int clickX;
    int clickY;
};

static SettingItem settings[] = {
    { SR_NETSETUP_SET_DIFFICULTY,  150, 62 },
    { SR_NETSETUP_SET_TIME,        150, 79 },
    { SR_NETSETUP_SET_GAMETYPE,    150, 113 },
    { SR_NETSETUP_SET_PLANETSIZE,  150, 130 },
    { SR_NETSETUP_SET_OCEAN,       150, 147 },
    { SR_NETSETUP_SET_EROSION,     150, 164 },
    { SR_NETSETUP_SET_NATIVES,     150, 181 },
    { SR_NETSETUP_SET_CLOUD,       150, 198 },
};
static const int SETTINGS_COUNT = 8;

// Checkbox items (bottom, two columns)
struct CheckboxItem {
    SrStr nameKey;
    int clickX;
    int clickY;
};

static const CheckboxItem checkboxes[] = {
    // Left column
    { SR_NETSETUP_CB_SIMULTANEOUS,    15, 415 },
    { SR_NETSETUP_CB_TRANSCENDENCE,   15, 435 },
    { SR_NETSETUP_CB_CONQUEST,        15, 455 },
    { SR_NETSETUP_CB_DIPLOMATIC,      15, 475 },
    { SR_NETSETUP_CB_ECONOMIC,        15, 495 },
    { SR_NETSETUP_CB_COOPERATIVE,     15, 515 },
    { SR_NETSETUP_CB_DO_OR_DIE,       15, 535 },
    { SR_NETSETUP_CB_LOOK_FIRST,      15, 555 },
    { SR_NETSETUP_CB_TECH_STAGNATION, 15, 575 },
    // Right column
    { SR_NETSETUP_CB_SPOILS,          409, 415 },
    { SR_NETSETUP_CB_BLIND_RESEARCH,  409, 435 },
    { SR_NETSETUP_CB_INTENSE_RIVALRY, 409, 455 },
    { SR_NETSETUP_CB_NO_UNITY_SURVEY, 409, 475 },
    { SR_NETSETUP_CB_NO_UNITY_SCATTER,409, 495 },
    { SR_NETSETUP_CB_BELL_CURVE,      409, 515 },
    { SR_NETSETUP_CB_TIME_WARP,       409, 535 },
    { SR_NETSETUP_CB_RAND_PERSONALITY,409, 555 },
    { SR_NETSETUP_CB_RAND_SOCIAL,     409, 575 },
};
static const int CHECKBOXES_COUNT = 18;

// Button items (coordinates updated dynamically in OnOpen from CHILD windows)
struct ButtonItem {
    SrStr nameKey;
    int clickX;
    int clickY;
};

static ButtonItem buttons[] = {
    { SR_NETSETUP_BTN_CANCEL, 674, 222 },
    { SR_NETSETUP_BTN_START,  435, 222 },
};
static const int BUTTONS_COUNT = 2;

// Player slot count
static const int PLAYER_COUNT = 7;

// Win.rRect1 offset (RECT at 0x13C in Win struct — gives window position)
static const int WIN_RECT_OFFSET = 0x13C;

// Forward declarations
static void announce_current();

// --- Handler state (declared early so timer callback can access) ---
static bool _active = false;
static bool _launching = false;  // true after Start clicked, suppresses OnTimer
static bool _inDialog = false;   // true during setting dialog, suppresses OnTimer
static int _currentZone = ZONE_SETTINGS;
static int _currentIndex = 0;

// Deferred click: fires after a short timer, completely outside any WinProc call
static int _pendingSettingClick = -1;
static const UINT_PTR SETTING_CLICK_TIMER_ID = 19999;

// Setting click timer temporarily disabled — popup approach abandoned.
// Will be replaced with direct memory write approach once field offsets are found.
void CALLBACK SettingClickTimerProc(HWND, UINT, UINT_PTR id, DWORD) {
    KillTimer(*phWnd, id);
    _pendingSettingClick = -1;
    sr_debug_log("NETSETUP: setting click disabled (finding field offsets first)");
}

// Local checkbox state tracking (toggled on each click)
static bool _cbState[CHECKBOXES_COUNT];

// Player text change detection
static char _lastPlayerText[PLAYER_COUNT][256];

// CHILD[3] buffer pointer for player list area detection
static void* _playerBuffer = nullptr;

// Zone item counts
static int zone_count(int zone) {
    switch (zone) {
        case ZONE_SETTINGS:   return SETTINGS_COUNT;
        case ZONE_PLAYERS:    return PLAYER_COUNT;
        case ZONE_CHECKBOXES: return CHECKBOXES_COUNT;
        case ZONE_BUTTONS:    return BUTTONS_COUNT;
        default: return 0;
    }
}

// Zone name string
static SrStr zone_name_key(int zone) {
    switch (zone) {
        case ZONE_SETTINGS:   return SR_NETSETUP_ZONE_SETTINGS;
        case ZONE_PLAYERS:    return SR_NETSETUP_ZONE_PLAYERS;
        case ZONE_CHECKBOXES: return SR_NETSETUP_ZONE_CHECKBOXES;
        case ZONE_BUTTONS:    return SR_NETSETUP_ZONE_BUTTONS;
        default: return SR_NETSETUP_ZONE_SETTINGS;
    }
}

/// Simulate a mouse click at NetWin-local coordinates (x, y).
/// Converts to game client coordinates using NetWin's rRect1 position,
/// then calls the game's WinProc with WM_LBUTTONDOWN/UP.
static void simulate_click(int netwin_x, int netwin_y) {
    if (!NetWin || !phWnd || !*phWnd) return;

    // Get NetWin position from rRect1 (RECT at Win offset 0x13C)
    RECT* rect = (RECT*)((char*)NetWin + WIN_RECT_OFFSET);
    if (IsBadReadPtr(rect, sizeof(RECT))) {
        sr_debug_log("NETSETUP: bad rect pointer at NetWin+0x%X", WIN_RECT_OFFSET);
        return;
    }

    int client_x = rect->left + netwin_x;
    int client_y = rect->top + netwin_y;
    LPARAM lp = MAKELPARAM(client_x, client_y);

    sr_debug_log("NETSETUP: simulate_click netwin(%d,%d) -> client(%d,%d) rect=(%d,%d,%d,%d)",
        netwin_x, netwin_y, client_x, client_y,
        (int)rect->left, (int)rect->top, (int)rect->right, (int)rect->bottom);

    // Send full click sequence through game's original WinProc
    WinProc(*phWnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    WinProc(*phWnd, WM_LBUTTONUP, 0, lp);
}

/// Call NetWin's vtable click handler directly with NetWin-local coordinates.
/// Bypasses the game's WinProc entirely — goes straight to the Win's own
/// click dispatch which checks SpotList and opens the setting popup.
/// vtbl[19] is the OnLButtonDown virtual method (0x0047B760 in base Win class).
static void direct_click(int netwin_x, int netwin_y) {
    if (!NetWin) return;

    int* vtbl = NetWin->vtbl;
    if (!vtbl || IsBadReadPtr(vtbl + 19, 4)) {
        sr_debug_log("NETSETUP: bad vtable for direct_click");
        return;
    }

    sr_debug_log("NETSETUP: direct_click(%d,%d) vtbl[19]=%p",
        netwin_x, netwin_y, (void*)vtbl[19]);

    // Call the OnLButtonDown virtual method: int __thiscall (Win*, int x, int y, int flags)
    typedef int(__thiscall *FWin_OnLBDown)(Win* This, int x, int y, int flags);
    FWin_OnLBDown onClick = (FWin_OnLBDown)vtbl[19];
    onClick(NetWin, netwin_x, netwin_y, MK_LBUTTON);
}

// Known NetWin field offsets for settings (found by memory scan).
// -1 means not yet discovered.
struct SettingField {
    int offset;     // byte offset in NetWin struct
    int max_val;    // maximum stored value (cycles 0..max_val)
};

static SettingField setting_fields[] = {
    { 0xE68, 5 },  // [0] Difficulty: 0=Spezialist .. 5=Transzendenz (6 values)
    { -1, 0 },     // [1] Time controls: offset unknown
    { -1, 0 },     // [2] Game type: offset unknown
    { -1, 0 },     // [3] Planet size: offset unknown
    { -1, 0 },     // [4] Ocean coverage: offset unknown
    { -1, 0 },     // [5] Erosion: offset unknown
    { -1, 0 },     // [6] Native life: offset unknown
    { -1, 0 },     // [7] Cloud cover: offset unknown
};

/// Read the current rendered value text for a setting from the last redraw.
/// Setting texts are at fixed y-coordinates in the main canvas.
static const int setting_y[] = { 62, 79, 113, 130, 147, 164, 181, 198 };

static void read_setting_text(int idx, char* out, int out_size) {
    out[0] = '\0';
    if (idx < 0 || idx >= SETTINGS_COUNT) return;

    int target_y = setting_y[idx];
    int count = sr_item_count();
    for (int i = 0; i < count; i++) {
        int y = sr_item_get_y(i);
        if (y == target_y) {
            const char* text = sr_item_get(i);
            if (text && text[0]) {
                // Strip the "{Label: } " prefix — find "} " and skip past it
                const char* val = strstr(text, "} ");
                if (val) {
                    val += 2;
                } else {
                    val = text;
                }
                strncpy(out, val, out_size - 1);
                out[out_size - 1] = '\0';
                return;
            }
        }
    }
}

/// Cycle a setting value: increment (or decrement with dir=-1), write to
/// NetWin memory, force redraw, and announce the new value text.
static void cycle_setting(int idx, int dir) {
    if (idx < 0 || idx >= SETTINGS_COUNT) return;
    if (!NetWin) return;

    SettingField& f = setting_fields[idx];
    if (f.offset < 0) {
        // Offset not known — open the settings editor at this item
        NetSetupSettingsHandler::RunModalAt(0, idx);
        return;
    }

    int* field = (int*)((unsigned char*)NetWin + f.offset);
    int old_val = *field;
    int new_val = old_val + dir;
    if (new_val > f.max_val) new_val = 0;
    if (new_val < 0) new_val = f.max_val;
    *field = new_val;

    sr_debug_log("NETSETUP: cycle setting %d (%s) offset=0x%X: %d -> %d",
        idx, loc(settings[idx].nameKey), f.offset, old_val, new_val);

    // Force redraw to update displayed text
    sr_force_snapshot();
    GraphicWin_redraw(NetWin);

    // Read the newly rendered text for this setting
    char val_text[256];
    read_setting_text(idx, val_text, sizeof(val_text));

    char buf[512];
    snprintf(buf, sizeof(buf), "%s: %s",
        loc(settings[idx].nameKey), val_text[0] ? val_text : "?");
    sr_output(buf, true);
}

/// Activate a checkbox: simulate click to toggle, then flip local state and announce.
static void activate_checkbox(int idx) {
    if (idx < 0 || idx >= CHECKBOXES_COUNT) return;
    sr_debug_log("NETSETUP: toggle checkbox %d (%s)", idx, loc(checkboxes[idx].nameKey));
    simulate_click(checkboxes[idx].clickX, checkboxes[idx].clickY);
    _cbState[idx] = !_cbState[idx];
    announce_current();
}

/// Activate a button: send warmup click to empty area, then click the button.
/// The warmup initializes the game's internal click state which is needed
/// for button handlers to respond (observed: Start only works after prior click).
static void activate_button(int idx) {
    if (idx < 0 || idx >= BUTTONS_COUNT) return;
    sr_debug_log("NETSETUP: activate button %d (%s)", idx, loc(buttons[idx].nameKey));
    // Warmup: click empty area in NetWin header (no controls at y=10)
    simulate_click(10, 10);
    // Actual button click
    simulate_click(buttons[idx].clickX, buttons[idx].clickY);
    // If Start button, stop OnTimer to prevent sync spam
    if (idx == 1) {
        _launching = true;
    }
}

/// Announce the current item in the current zone.
static void announce_current() {
    char buf[512];
    int count = zone_count(_currentZone);
    if (count == 0) return;

    switch (_currentZone) {
        case ZONE_SETTINGS:
            snprintf(buf, sizeof(buf), loc(SR_NETSETUP_ITEM_FMT),
                _currentIndex + 1, count,
                loc(settings[_currentIndex].nameKey));
            break;
        case ZONE_PLAYERS: {
            const char* text = _lastPlayerText[_currentIndex];
            if (text[0] == '\0') {
                snprintf(buf, sizeof(buf), loc(SR_NETSETUP_PLAYER_FMT),
                    _currentIndex + 1, count,
                    loc(SR_NETSETUP_PLAYER_EMPTY));
            } else {
                snprintf(buf, sizeof(buf), loc(SR_NETSETUP_PLAYER_FMT),
                    _currentIndex + 1, count, text);
            }
            break;
        }
        case ZONE_CHECKBOXES: {
            bool enabled = _cbState[_currentIndex];
            snprintf(buf, sizeof(buf), loc(SR_NETSETUP_CHECKBOX_FMT),
                _currentIndex + 1, count,
                loc(checkboxes[_currentIndex].nameKey),
                enabled ? loc(SR_NETSETUP_ENABLED) : loc(SR_NETSETUP_DISABLED));
            break;
        }
        case ZONE_BUTTONS:
            snprintf(buf, sizeof(buf), loc(SR_NETSETUP_ITEM_FMT),
                _currentIndex + 1, count,
                loc(buttons[_currentIndex].nameKey));
            break;
    }
    sr_output(buf, true);
    sr_debug_log("NETSETUP: announce zone=%d idx=%d: %s", _currentZone, _currentIndex, buf);
}

/// Announce zone change.
static void announce_zone() {
    char buf[512];
    int count = zone_count(_currentZone);
    snprintf(buf, sizeof(buf), "%s, %d %s",
        loc(zone_name_key(_currentZone)),
        count,
        count == 1 ? "item" : "items");
    sr_output(buf, true);
    sr_debug_log("NETSETUP: zone=%d (%s)", _currentZone, loc(zone_name_key(_currentZone)));
    // Also announce current item (queued, non-interrupting)
    announce_current();
}

// --- Public API ---

bool IsActive() {
    return _active;
}

// SpotList offset in NetWin (found by runtime scan, contains settings/checkbox/player hitboxes)
static const int NETWIN_SPOTLIST_OFFSET = 0xD34;

void OnOpen() {
    _active = true;
    _launching = false;
    _currentZone = ZONE_SETTINGS;
    _currentIndex = 0;
    // Clear player text
    for (int i = 0; i < PLAYER_COUNT; i++) {
        _lastPlayerText[i][0] = '\0';
    }
    // Initialize checkbox state tracking (all off — defaults unknown)
    for (int i = 0; i < CHECKBOXES_COUNT; i++) {
        _cbState[i] = false;
    }
    // Player text is rendered to NetWin's main canvas (GraphicWin oCanvas at +0x444).
    // Use this as buffer filter in OnTimer.
    _playerBuffer = nullptr;
    if (NetWin) {
        _playerBuffer = (void*)((char*)NetWin + 0x444);
        sr_debug_log("NETSETUP: main canvas buffer=%p", _playerBuffer);
    }
    // Enable no-dedup mode so repeated player names ("Computer") are all captured
    sr_mp_no_dedup = true;

    // Read CHILD windows to find button positions dynamically.
    // NetWin has children including BaseButtons for Cancel and Start.
    // BaseButton.name is at offset 0xA7C within the BaseButton struct.
    // Win.rRect1 (at 0x13C) gives the child's position relative to parent.
    if (NetWin) {
        int nChildren = NetWin->iChildCount;
        if (nChildren > 20) nChildren = 20;
        for (int ci = 0; ci < nChildren; ci++) {
            Win* child = NetWin->apoChildren[ci];
            if (!child) continue;
            int* cr = (int*)child;
            // Check BaseButton name at offset 0xA7C
            if (IsBadReadPtr(cr + 0xA7C/4, 4)) continue;
            char* btn_name = (char*)cr[0xA7C/4];
            if (!btn_name || (unsigned int)btn_name < 0x10000
                || (unsigned int)btn_name > 0x7FFFFFFF
                || IsBadReadPtr(btn_name, 4)
                || btn_name[0] < 0x20 || btn_name[0] >= 0x7F) {
                continue;
            }
            // Calculate center of button from its rRect1
            RECT* r = &child->rRect1;
            int cx = (r->left + r->right) / 2;
            int cy = (r->top + r->bottom) / 2;
            sr_debug_log("NETSETUP: CHILD[%d] btn_name=[%s] rect=(%d,%d,%d,%d) center=(%d,%d)",
                ci, btn_name, (int)r->left, (int)r->top, (int)r->right, (int)r->bottom, cx, cy);
            // Match button names (game uses localized names, e.g. "ABBRECHEN"/"SPIEL STARTEN")
            // Use child index as primary: CHILD[0] = Cancel, CHILD[1] = Start
            // Also match known name patterns as fallback
            bool is_cancel = (ci == 0)
                || strstr(btn_name, "Cancel") || strstr(btn_name, "CANCEL")
                || strstr(btn_name, "ABBRECHEN");
            bool is_start = !is_cancel && (
                (ci == 1)
                || strstr(btn_name, "OK") || strstr(btn_name, "Start")
                || strstr(btn_name, "START") || strstr(btn_name, "Go")
                || strstr(btn_name, "STARTEN"));
            if (is_cancel) {
                buttons[0].clickX = cx;
                buttons[0].clickY = cy;
                sr_debug_log("NETSETUP: Cancel button at (%d,%d) [%s]", cx, cy, btn_name);
            } else if (is_start) {
                buttons[1].clickX = cx;
                buttons[1].clickY = cy;
                sr_debug_log("NETSETUP: Start button at (%d,%d) [%s]", cx, cy, btn_name);
            }
        }
    }

    // Read SpotList at NetWin+0xD34 to get actual hitbox rects for settings.
    // Spot: RECT(16 bytes) + type(4) + position(4) = 24 bytes each.
    // type=0 are settings, type=1 are checkboxes.
    if (NetWin) {
        SpotList* sl = (SpotList*)((char*)NetWin + NETWIN_SPOTLIST_OFFSET);
        if (!IsBadReadPtr(sl, sizeof(SpotList)) && sl->spots && sl->cur_count > 0
            && !IsBadReadPtr(sl->spots, sl->cur_count * sizeof(Spot))) {
            sr_debug_log("NETSETUP: SpotList at +0x%X: %d spots (max %d)",
                NETWIN_SPOTLIST_OFFSET, sl->cur_count, sl->max_count);
            int setting_idx = 0;
            for (int i = 0; i < sl->cur_count && i < 100; i++) {
                Spot* s = &sl->spots[i];
                sr_debug_log("NETSETUP: spot[%d] type=%d pos=%d rect=(%d,%d,%d,%d)",
                    i, s->type, s->position,
                    (int)s->rect.left, (int)s->rect.top,
                    (int)s->rect.right, (int)s->rect.bottom);
                // Update settings click coordinates from type=0 spots
                if (s->type == 0 && setting_idx < SETTINGS_COUNT) {
                    int cx = (s->rect.left + s->rect.right) / 2;
                    int cy = (s->rect.top + s->rect.bottom) / 2;
                    settings[setting_idx].clickX = cx;
                    settings[setting_idx].clickY = cy;
                    sr_debug_log("NETSETUP: setting[%d] updated to (%d,%d)", setting_idx, cx, cy);
                    setting_idx++;
                }
            }
        } else {
            sr_debug_log("NETSETUP: SpotList at +0x%X not readable or empty (spots=%p cur=%d)",
                NETWIN_SPOTLIST_OFFSET,
                sl ? (void*)sl->spots : nullptr,
                sl ? sl->cur_count : -1);
        }
    }

    // Memory dump: check game globals AND NetWin fields after SpotList.
    // Previous dump showed 0xA14-0xD34 is all RECT layout data.
    if (NetWin) {
        // 1. Check if game globals are really zero during NetWin
        sr_debug_log("NETSETUP: === GAME GLOBALS CHECK ===");
        sr_debug_log("  DiffLevel(@0x9A64C4) = %d", *DiffLevel);
        sr_debug_log("  MapSizePlanet(@0x94A2A0) = %d", *(int*)0x94A2A0);
        sr_debug_log("  MapOceanCoverage(@0x94A2A4) = %d", *(int*)0x94A2A4);
        sr_debug_log("  MapErosiveForces(@0x94A2AC) = %d", *(int*)0x94A2AC);
        sr_debug_log("  MapNativeLifeForms(@0x94A2B8) = %d", *(int*)0x94A2B8);
        sr_debug_log("  MapCloudCover(@0x94A2B4) = %d", *(int*)0x94A2B4);
        sr_debug_log("  GameRules(@0x9A649C) = 0x%X", *(int*)0x9A649C);

        // 2. Dump NetWin after SpotList (0xD40 to 0xF00)
        unsigned char* base = (unsigned char*)NetWin;
        const int dump_start = 0xD40;
        const int dump_end = 0xF00;
        if (!IsBadReadPtr(base + dump_start, dump_end - dump_start)) {
            sr_debug_log("NETSETUP: === MEMORY DUMP NetWin+0x%X to +0x%X ===", dump_start, dump_end);
            for (int off = dump_start; off < dump_end; off += 16) {
                int len = dump_end - off;
                if (len > 16) len = 16;
                char hex[80];
                int pos = 0;
                for (int b = 0; b < len; b++) {
                    pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", base[off + b]);
                }
                sr_debug_log("  +0x%04X: %s", off, hex);
            }
            sr_debug_log("NETSETUP: === INT32 VALUES (0xD40-0xF00) ===");
            for (int off = dump_start; off < dump_end; off += 4) {
                int val = *(int*)(base + off);
                if (val >= 0 && val <= 10) {
                    sr_debug_log("  +0x%04X = %d  <-- small int", off, val);
                }
            }
        }

        // 3. Also check AlphaIniPref at 0x94B464 (from OpenSMACX)
        int* prefs = (int*)0x94B464;
        if (!IsBadReadPtr(prefs, 40)) {
            sr_debug_log("NETSETUP: === AlphaIniPref @0x94B464 ===");
            for (int i = 0; i < 10; i++) {
                sr_debug_log("  [%d] = 0x%08X (%d)", i, prefs[i], prefs[i]);
            }
        }
    }

    sr_output(loc(SR_NETSETUP_OPEN), true);
    sr_debug_log("NETSETUP: OnOpen, NetWin=%p", (void*)NetWin);
}

void OnClose() {
    _active = false;
    sr_mp_no_dedup = false;
    sr_debug_log("NETSETUP: OnClose");
}

bool Update(UINT msg, WPARAM wParam) {
    if (!_active || msg != WM_KEYDOWN) return false;

    // Don't handle keys while a popup list is active (e.g. time_controls_dialog)
    if (sr_popup_list.active) {
        return false;
    }

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    // Ctrl+F1: help
    if (ctrl && wParam == VK_F1) {
        sr_output(loc(SR_NETSETUP_HELP), true);
        return true;
    }

    // Ctrl+Shift+F10: open net setup settings editor
    if (ctrl && shift && wParam == VK_F10) {
        NetSetupSettingsHandler::RunModal();
        return true;
    }

    // Escape: close lobby via GraphicWin_close (bypass mouse clicks entirely)
    if (wParam == VK_ESCAPE) {
        if (NetWin) {
            sr_debug_log("NETSETUP: Escape -> GraphicWin_close(NetWin)");
            GraphicWin_close(NetWin);
        }
        return true;
    }

    // Tab / Shift+Tab: cycle zones
    if (wParam == VK_TAB) {
        if (shift) {
            _currentZone--;
            if (_currentZone < 0) _currentZone = ZONE_COUNT - 1;
        } else {
            _currentZone++;
            if (_currentZone >= ZONE_COUNT) _currentZone = 0;
        }
        _currentIndex = 0;
        announce_zone();
        return true;
    }

    // Up/Down: navigate within zone
    if (wParam == VK_UP || wParam == VK_DOWN) {
        int count = zone_count(_currentZone);
        if (count == 0) return true;
        if (wParam == VK_DOWN) {
            _currentIndex++;
            if (_currentIndex >= count) _currentIndex = count - 1;
        } else {
            _currentIndex--;
            if (_currentIndex < 0) _currentIndex = 0;
        }
        announce_current();
        return true;
    }

    // Left/Right: cycle setting value (settings zone only)
    if ((wParam == VK_LEFT || wParam == VK_RIGHT) && _currentZone == ZONE_SETTINGS) {
        int dir = (wParam == VK_RIGHT) ? 1 : -1;
        cycle_setting(_currentIndex, dir);
        return true;
    }

    // Enter or Space: activate
    if (wParam == VK_RETURN || wParam == VK_SPACE) {
        switch (_currentZone) {
            case ZONE_SETTINGS:
                cycle_setting(_currentIndex, 1);  // cycle forward
                break;
            case ZONE_CHECKBOXES:
                activate_checkbox(_currentIndex);
                break;
            case ZONE_BUTTONS:
                activate_button(_currentIndex);
                break;
            case ZONE_PLAYERS:
                // Read-only, just re-announce
                announce_current();
                break;
        }
        return true;
    }

    // Ctrl+T: DIAGNOSTIC — write test values to suspected field offsets and redraw.
    // Tests one offset at a time; cycles through candidates on each press.
    if (ctrl && wParam == 'T' && NetWin) {
        static int test_phase = 0;
        unsigned char* base = (unsigned char*)NetWin;

        // Candidate offsets for difficulty (currently=2, write 0 → should show "Bürger"/"Citizen")
        // and map settings (currently=1, write 2 → should change display text)
        struct TestCase {
            int offset;
            int old_expect;  // what we expect to find
            int new_val;     // what we write
            const char* name;
        };
        static TestCase tests[] = {
            { 0xE68, 2, 0, "difficulty?" },
            { 0xE6C, 0, 1, "time_controls?" },
            { 0xE70, 1, 0, "game_type?" },
            { 0xE4C, 1, 0, "field_E4C?" },
            { 0xE50, 1, 0, "field_E50?" },
            { 0xEB4, 1, 0, "map_a?" },
            { 0xEB8, 1, 0, "map_b?" },
            { 0xEBC, 1, 0, "map_c?" },
            { 0xEC0, 1, 0, "map_d?" },
            { 0xEE0, 2, 0, "field_EE0?" },
        };
        int ntests = sizeof(tests) / sizeof(tests[0]);

        if (test_phase >= ntests) {
            sr_output("All test phases done. Restart lobby to reset.", true);
            test_phase = 0;
            return true;
        }

        TestCase& t = tests[test_phase];
        int* field = (int*)(base + t.offset);
        int old_val = *field;
        *field = t.new_val;
        GraphicWin_redraw(NetWin);

        char buf[256];
        snprintf(buf, sizeof(buf), "Test %d: +0x%X (%s) was %d, wrote %d. Check display!",
            test_phase, t.offset, t.name, old_val, t.new_val);
        sr_output(buf, true);
        sr_debug_log("NETSETUP: %s", buf);

        test_phase++;
        return true;
    }

    // S: summary (announce zone + current item)
    if (wParam == 'S' && !ctrl && !shift) {
        char buf[512];
        int count = zone_count(_currentZone);
        const char* item_name = "";
        switch (_currentZone) {
            case ZONE_SETTINGS:
                item_name = loc(settings[_currentIndex].nameKey);
                break;
            case ZONE_PLAYERS:
                item_name = _lastPlayerText[_currentIndex][0]
                    ? _lastPlayerText[_currentIndex]
                    : loc(SR_NETSETUP_PLAYER_EMPTY);
                break;
            case ZONE_CHECKBOXES:
                item_name = loc(checkboxes[_currentIndex].nameKey);
                break;
            case ZONE_BUTTONS:
                item_name = loc(buttons[_currentIndex].nameKey);
                break;
        }
        snprintf(buf, sizeof(buf), loc(SR_NETSETUP_SUMMARY),
            loc(zone_name_key(_currentZone)),
            _currentIndex + 1, count, item_name);
        sr_output(buf, true);
        return true;
    }

    return false;
}

void OnTimer() {
    if (!_active || !NetWin || _launching || _inDialog) return;

    // The main canvas only renders ONCE when the screen opens. Periodic redraws
    // only update child windows (timer, network debug). To capture player data,
    // we force a full re-render with dedup disabled.
    //
    // 1. Clear text capture buffers
    // 2. Force GraphicWin_redraw (synchronous -> fills sr_items with no dedup)
    // 3. Read captured items directly for player rows
    sr_force_snapshot(); // save old + clear
    GraphicWin_redraw(NetWin); // synchronous render fills sr_items

    // Player rows at exact y-coordinates in main canvas (22px spacing).
    static const int player_y[PLAYER_COUNT] = { 58, 80, 102, 124, 146, 168, 190 };

    char current_text[PLAYER_COUNT][256];
    for (int p = 0; p < PLAYER_COUNT; p++) {
        current_text[p][0] = '\0';
    }

    // Read directly from sr_items (populated by the forced redraw)
    int count = sr_item_count();
    for (int i = 0; i < count; i++) {
        void* buf = sr_item_get_buf(i);
        if (buf != _playerBuffer) continue; // Not from main canvas

        int y = sr_item_get_y(i);
        const char* text = sr_item_get(i);
        if (!text || !text[0]) continue;

        // Skip settings text (starts with "{")
        if (text[0] == '{') continue;

        // Exact y-match to player slot (no tolerance -- settings are at +/-1..4px)
        int slot = -1;
        for (int p = 0; p < PLAYER_COUNT; p++) {
            if (y == player_y[p]) {
                slot = p;
                break;
            }
        }
        if (slot < 0) continue;

        // Append text (name + faction + difficulty columns)
        int len = (int)strlen(current_text[slot]);
        if (len > 0 && len < 254) {
            current_text[slot][len] = ' ';
            current_text[slot][len + 1] = '\0';
            len++;
        }
        int remain = 255 - len;
        if (remain > 0) {
            strncat(current_text[slot], text, remain);
            current_text[slot][255] = '\0';
        }
    }

    // Detect changes
    for (int p = 0; p < PLAYER_COUNT; p++) {
        if (strcmp(current_text[p], _lastPlayerText[p]) != 0) {
            char buf[300];
            if (current_text[p][0] && !_lastPlayerText[p][0]) {
                snprintf(buf, sizeof(buf), loc(SR_NETSETUP_PLAYER_JOINED),
                    current_text[p]);
                sr_output(buf, false);
                sr_debug_log("NETSETUP: player joined slot %d: %s", p, current_text[p]);
            } else if (!current_text[p][0] && _lastPlayerText[p][0]) {
                snprintf(buf, sizeof(buf), loc(SR_NETSETUP_PLAYER_LEFT), p + 1);
                sr_output(buf, false);
                sr_debug_log("NETSETUP: player left slot %d", p);
            }
            strncpy(_lastPlayerText[p], current_text[p], 255);
            _lastPlayerText[p][255] = '\0';
        }
    }
}

} // namespace MultiplayerHandler
