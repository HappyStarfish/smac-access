/*
 * NetSetupSettingsHandler - Accessible multiplayer lobby settings editor.
 *
 * Ctrl+Shift+F10 in the NetWin lobby opens a modal dialog with 2 categories:
 *   1. Settings (8 dropdown items)
 *   2. Rules (18 checkbox items)
 *
 * Settings are loaded by reading the currently rendered text from the lobby
 * display and mapping it to option indices via lookup tables.
 *
 * On save, dropdown settings are applied via IAT-hooked GetAsyncKeyState
 * (fakes VK_LBUTTON pressed so the game's popup menus work correctly),
 * and checkboxes are toggled via simulate_click.
 */

#include "netsetup_settings_handler.h"
#include "main.h"
#include "engine.h"
#include "gui.h"
#include "modal_utils.h"
#include "screen_reader.h"
#include "localization.h"
#include <stdio.h>
#include <string.h>

// Game globals
extern Win* NetWin;
extern HWND* phWnd;

// From gui.cpp
extern int __thiscall Win_is_visible(Win* This);

// From multiplayer_handler.cpp
extern SrPopupList sr_popup_list;

// Global flag for IAT hook
bool sr_fake_lbutton = false;

namespace NetSetupSettingsHandler {

// --- Constants ---

static const int NUM_CATEGORIES = 2;
static const int CAT_SETTINGS = 0;
static const int CAT_RULES = 1;

static const int NUM_SETTINGS = 8;
static const int NUM_CHECKBOXES = 18;

// Win.rRect1 offset (RECT at 0x13C in Win struct)
static const int WIN_RECT_OFFSET = 0x13C;

// Known difficulty offset in NetWin
static const int DIFFICULTY_OFFSET = 0xE68;

// Setting definitions with click coordinates and value counts
struct SettingDef {
    SrStr nameKey;
    int clickX;
    int clickY;
    int numValues;
    const SrStr* valueKeys;
};

// Difficulty values (6)
static const SrStr DifficultyValues[] = {
    SR_NETSETUP_DIFF_CITIZEN, SR_NETSETUP_DIFF_SPECIALIST,
    SR_NETSETUP_DIFF_TALENT, SR_NETSETUP_DIFF_LIBRARIAN,
    SR_NETSETUP_DIFF_THINKER, SR_NETSETUP_DIFF_TRANSCEND
};

// Time control values (2 — None and Custom, text-matched)
static const SrStr TimeValues[] = {
    SR_NETSETTINGS_TIME_NONE, SR_NETSETTINGS_TIME_OTHER
};

// Game type values (3)
static const SrStr GameTypeValues[] = {
    SR_NETSETUP_GT_RANDOM, SR_NETSETUP_GT_SCENARIO, SR_NETSETUP_GT_LOAD_MAP
};

// Planet size values (5)
static const SrStr PlanetSizeValues[] = {
    SR_NETSETUP_SIZE_TINY, SR_NETSETUP_SIZE_SMALL,
    SR_NETSETUP_SIZE_STANDARD, SR_NETSETUP_SIZE_LARGE,
    SR_NETSETUP_SIZE_HUGE
};

// Ocean values (3)
static const SrStr OceanValues[] = {
    SR_NETSETUP_OCEAN_30, SR_NETSETUP_OCEAN_50, SR_NETSETUP_OCEAN_70
};

// Erosion values (3)
static const SrStr ErosionValues[] = {
    SR_NETSETUP_EROSION_STRONG, SR_NETSETUP_EROSION_AVG, SR_NETSETUP_EROSION_WEAK
};

// Native life values (3)
static const SrStr NativeValues[] = {
    SR_NETSETUP_NATIVE_RARE, SR_NETSETUP_NATIVE_AVG, SR_NETSETUP_NATIVE_ABUND
};

// Cloud cover values (3)
static const SrStr CloudValues[] = {
    SR_NETSETUP_CLOUD_SPARSE, SR_NETSETUP_CLOUD_AVG, SR_NETSETUP_CLOUD_DENSE
};

// Setting definitions array — click coordinates updated from SpotList in load_values()
static SettingDef settingDefs[NUM_SETTINGS] = {
    { SR_NETSETUP_SET_DIFFICULTY,  150, 62,  6, DifficultyValues },
    { SR_NETSETUP_SET_TIME,        150, 79,  2, TimeValues },
    { SR_NETSETUP_SET_GAMETYPE,    150, 113, 3, GameTypeValues },
    { SR_NETSETUP_SET_PLANETSIZE,  150, 130, 5, PlanetSizeValues },
    { SR_NETSETUP_SET_OCEAN,       150, 147, 3, OceanValues },
    { SR_NETSETUP_SET_EROSION,     150, 164, 3, ErosionValues },
    { SR_NETSETUP_SET_NATIVES,     150, 181, 3, NativeValues },
    { SR_NETSETUP_SET_CLOUD,       150, 198, 3, CloudValues },
};

// Checkbox definitions with click coordinates
struct CheckboxDef {
    SrStr nameKey;
    int clickX;
    int clickY;
};

static const CheckboxDef checkboxDefs[NUM_CHECKBOXES] = {
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

// Setting y-coordinates for text reading (must match multiplayer_handler)
static const int setting_y[] = { 62, 79, 113, 130, 147, 164, 181, 198 };

// --- State ---

static bool _active = false;
static bool _wantClose = false;
static bool _confirmed = false;
static int _currentCat = 0;
static int _currentItem = 0;

// Current selected values
static int _settingValues[NUM_SETTINGS];
static bool _checkboxStates[NUM_CHECKBOXES];

// Original values for change detection
static int _origSettings[NUM_SETTINGS];
static bool _origCheckboxes[NUM_CHECKBOXES];

// --- Helpers ---

/// Simulate a mouse click at NetWin-local coordinates.
static void simulate_click(int netwin_x, int netwin_y) {
    if (!NetWin || !phWnd || !*phWnd) return;

    RECT* rect = (RECT*)((char*)NetWin + WIN_RECT_OFFSET);
    if (IsBadReadPtr(rect, sizeof(RECT))) return;

    int client_x = rect->left + netwin_x;
    int client_y = rect->top + netwin_y;
    LPARAM lp = MAKELPARAM(client_x, client_y);

    WinProc(*phWnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    WinProc(*phWnd, WM_LBUTTONUP, 0, lp);
}

/// Read the rendered text for a setting from the last redraw snapshot.
static void read_setting_text(int idx, char* out, int out_size) {
    out[0] = '\0';
    if (idx < 0 || idx >= NUM_SETTINGS) return;

    int target_y = setting_y[idx];
    int count = sr_item_count();
    for (int i = 0; i < count; i++) {
        int y = sr_item_get_y(i);
        if (y == target_y) {
            const char* text = sr_item_get(i);
            if (text && text[0]) {
                // Strip the "{Label: } " prefix
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

/// Match rendered text against a setting's value list. Returns index or 0.
static int match_setting_value(int setting_idx, const char* text) {
    if (!text || !text[0]) return 0;
    const SettingDef& def = settingDefs[setting_idx];
    for (int v = 0; v < def.numValues; v++) {
        const char* expected = loc(def.valueKeys[v]);
        if (_strnicmp(text, expected, strlen(expected)) == 0) {
            return v;
        }
    }
    // No match — try substring match (game may add extra text)
    for (int v = 0; v < def.numValues; v++) {
        const char* expected = loc(def.valueKeys[v]);
        if (strstr(text, expected)) {
            return v;
        }
    }
    return 0;
}

/// Get number of items in the current category.
static int category_count() {
    switch (_currentCat) {
        case CAT_SETTINGS: return NUM_SETTINGS;
        case CAT_RULES:    return NUM_CHECKBOXES;
        default:           return 0;
    }
}

/// Get current value name for a setting.
static const char* setting_value_name(int idx) {
    if (idx < 0 || idx >= NUM_SETTINGS) return "?";
    const SettingDef& def = settingDefs[idx];
    int val = _settingValues[idx];
    if (val >= 0 && val < def.numValues) {
        return loc(def.valueKeys[val]);
    }
    return "?";
}

// --- Load values from game state ---

static void load_values() {
    // Update click coordinates from SpotList (same logic as MultiplayerHandler)
    if (NetWin) {
        // SpotList at NetWin+0xD34
        SpotList* sl = (SpotList*)((char*)NetWin + 0xD34);
        if (!IsBadReadPtr(sl, sizeof(SpotList)) && sl->spots && sl->cur_count > 0
            && !IsBadReadPtr(sl->spots, sl->cur_count * sizeof(Spot))) {
            int setting_idx = 0;
            for (int i = 0; i < sl->cur_count && i < 100; i++) {
                Spot* s = &sl->spots[i];
                if (s->type == 0 && setting_idx < NUM_SETTINGS) {
                    int cx = (s->rect.left + s->rect.right) / 2;
                    int cy = (s->rect.top + s->rect.bottom) / 2;
                    settingDefs[setting_idx].clickX = cx;
                    settingDefs[setting_idx].clickY = cy;
                    setting_idx++;
                }
            }
        }
    }

    // Force a snapshot + redraw to get fresh text
    sr_force_snapshot();
    if (NetWin) GraphicWin_redraw(NetWin);

    // Read difficulty directly from known offset
    if (NetWin) {
        int diff = *(int*)((char*)NetWin + DIFFICULTY_OFFSET);
        if (diff >= 0 && diff < 6) {
            _settingValues[0] = diff;
        } else {
            _settingValues[0] = 0;
        }
    }

    // Read other settings from rendered text
    for (int i = 1; i < NUM_SETTINGS; i++) {
        char text[256];
        read_setting_text(i, text, sizeof(text));
        _settingValues[i] = match_setting_value(i, text);
        sr_debug_log("NETSETTINGS: load setting %d text=[%s] -> value=%d",
            i, text, _settingValues[i]);
    }

    // Read checkbox states from MultiplayerHandler's tracking.
    // We access the _cbState array indirectly by checking the rendered text.
    // Since checkboxes are toggled by clicking and tracked locally, we need to
    // force a redraw and read checkbox text. However, there's no reliable way
    // to read checkbox state from rendered text alone. Instead, we assume
    // default states and let the user toggle from there.
    // TODO: read checkbox state from game memory once offsets are found
    for (int i = 0; i < NUM_CHECKBOXES; i++) {
        _checkboxStates[i] = false;
    }

    // Save originals for change detection
    for (int i = 0; i < NUM_SETTINGS; i++) {
        _origSettings[i] = _settingValues[i];
    }
    for (int i = 0; i < NUM_CHECKBOXES; i++) {
        _origCheckboxes[i] = _checkboxStates[i];
    }
}

// --- Announcements ---

static void announce_category() {
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_NETSETTINGS_CAT_FMT),
        _currentCat + 1, NUM_CATEGORIES,
        loc(_currentCat == CAT_SETTINGS ? SR_NETSETTINGS_CAT_SETTINGS : SR_NETSETTINGS_CAT_RULES),
        category_count());
    sr_output(buf, true);
}

static void announce_item() {
    char buf[512];
    int count = category_count();

    switch (_currentCat) {
        case CAT_SETTINGS:
            snprintf(buf, sizeof(buf), loc(SR_GSETTINGS_ITEM_FMT),
                _currentItem + 1, count,
                loc(settingDefs[_currentItem].nameKey),
                setting_value_name(_currentItem));
            break;
        case CAT_RULES: {
            bool on = _checkboxStates[_currentItem];
            snprintf(buf, sizeof(buf), loc(SR_GSETTINGS_TOGGLE_FMT),
                _currentItem + 1, count,
                loc(checkboxDefs[_currentItem].nameKey),
                loc(on ? SR_GSETTINGS_ON : SR_GSETTINGS_OFF));
            break;
        }
        default:
            return;
    }
    sr_output(buf, true);
}

static void announce_summary() {
    char buf[512];
    int changed_settings = 0;
    int changed_checkboxes = 0;
    for (int i = 0; i < NUM_SETTINGS; i++) {
        if (_settingValues[i] != _origSettings[i]) changed_settings++;
    }
    for (int i = 0; i < NUM_CHECKBOXES; i++) {
        if (_checkboxStates[i] != _origCheckboxes[i]) changed_checkboxes++;
    }
    snprintf(buf, sizeof(buf), loc(SR_NETSETTINGS_SUMMARY_FMT),
        setting_value_name(0), // difficulty
        setting_value_name(3), // planet size
        changed_settings, changed_checkboxes);
    sr_output(buf, true);
}

// --- Navigation ---

static void change_category(int dir) {
    _currentCat = (_currentCat + dir + NUM_CATEGORIES) % NUM_CATEGORIES;
    _currentItem = 0;
    announce_category();
    announce_item();
}

static void navigate(int dir) {
    int count = category_count();
    if (count <= 1) return;
    _currentItem = (_currentItem + dir + count) % count;
    announce_item();
}

static void change_value(int dir) {
    if (_currentCat == CAT_SETTINGS) {
        const SettingDef& def = settingDefs[_currentItem];
        _settingValues[_currentItem] = (_settingValues[_currentItem] + dir + def.numValues) % def.numValues;
        announce_item();
    } else if (_currentCat == CAT_RULES) {
        _checkboxStates[_currentItem] = !_checkboxStates[_currentItem];
        announce_item();
    }
}

static void toggle_checkbox() {
    if (_currentCat == CAT_RULES) {
        _checkboxStates[_currentItem] = !_checkboxStates[_currentItem];
        announce_item();
    }
}

// --- Apply changes ---

/// Apply a single dropdown setting change via IAT-hooked GetAsyncKeyState.
/// Opens the game's native popup by clicking the setting area, then sends
/// mouse move + release to the desired option position.
static void apply_setting_popup(int idx) {
    const SettingDef& def = settingDefs[idx];
    int desired = _settingValues[idx];

    sr_debug_log("NETSETTINGS: apply setting %d (%s) -> value %d via popup",
        idx, loc(def.nameKey), desired);

    // Enable fake LButton for the popup mechanism
    sr_fake_lbutton = true;

    // Click the setting area to open popup
    simulate_click(def.clickX, def.clickY);

    // Wait for popup to render
    Sleep(80);

    // Calculate target Y position within the popup.
    // Popup items are spaced 20px apart, starting at y offset ~1 from popup top.
    // The popup opens near the click position. We send mouse move to the
    // desired item position, then release.
    // Popup item Y positions (relative to popup): 1, 21, 41, 61, 81, 101
    RECT* rect = (RECT*)((char*)NetWin + WIN_RECT_OFFSET);
    if (!IsBadReadPtr(rect, sizeof(RECT))) {
        // The popup appears at roughly the setting's Y position.
        // We need to target the desired_value'th item within the popup.
        int popup_item_y = def.clickY + (desired * 20) - (_origSettings[idx] * 20);
        int client_x = rect->left + def.clickX;
        int client_y = rect->top + popup_item_y;
        LPARAM lp = MAKELPARAM(client_x, client_y);

        // Move mouse to target item
        WinProc(*phWnd, WM_MOUSEMOVE, MK_LBUTTON, lp);
        Sleep(30);

        // Release — popup selects the item under cursor
        sr_fake_lbutton = false;
        WinProc(*phWnd, WM_LBUTTONUP, 0, lp);
    } else {
        sr_fake_lbutton = false;
    }

    Sleep(50);
}

/// Apply difficulty directly via known memory offset.
static void apply_difficulty(int value) {
    if (!NetWin) return;
    int* field = (int*)((char*)NetWin + DIFFICULTY_OFFSET);
    *field = value;
    sr_debug_log("NETSETTINGS: wrote difficulty=%d to NetWin+0x%X", value, DIFFICULTY_OFFSET);
}

/// Apply all changes to the game.
static void save_settings() {
    if (!NetWin) return;

    sr_debug_log("NETSETTINGS: === APPLYING CHANGES ===");

    // Apply difficulty via direct memory write (known offset)
    if (_settingValues[0] != _origSettings[0]) {
        apply_difficulty(_settingValues[0]);
    }

    // Apply other settings via popup mechanism
    for (int i = 1; i < NUM_SETTINGS; i++) {
        if (_settingValues[i] != _origSettings[i]) {
            apply_setting_popup(i);
        }
    }

    // Apply checkbox changes via simulate_click (toggles)
    for (int i = 0; i < NUM_CHECKBOXES; i++) {
        if (_checkboxStates[i] != _origCheckboxes[i]) {
            sr_debug_log("NETSETTINGS: toggle checkbox %d (%s)", i, loc(checkboxDefs[i].nameKey));
            simulate_click(checkboxDefs[i].clickX, checkboxDefs[i].clickY);
            Sleep(30);
        }
    }

    // Force redraw to show updated state
    sr_force_snapshot();
    GraphicWin_redraw(NetWin);

    sr_debug_log("NETSETTINGS: === APPLY COMPLETE ===");
}

// --- Public API ---

bool IsActive() {
    return _active;
}

void Update(unsigned int /*msg*/, unsigned int wParam) {
    if (!_active) return;

    bool ctrl = ctrl_key_down();
    bool shift = shift_key_down();

    switch (wParam) {
        case VK_ESCAPE:
            _confirmed = false;
            _wantClose = true;
            break;

        case VK_RETURN:
            _confirmed = true;
            _wantClose = true;
            break;

        case VK_SPACE:
            toggle_checkbox();
            break;

        case VK_TAB:
            if (shift) {
                change_category(-1);
            } else {
                change_category(1);
            }
            break;

        case VK_UP:
            navigate(-1);
            break;

        case VK_DOWN:
            navigate(1);
            break;

        case VK_LEFT:
            change_value(-1);
            break;

        case VK_RIGHT:
            change_value(1);
            break;

        case VK_HOME:
            _currentItem = 0;
            announce_item();
            break;

        case VK_END:
            _currentItem = category_count() - 1;
            if (_currentItem < 0) _currentItem = 0;
            announce_item();
            break;

        case VK_F1:
            if (ctrl) {
                sr_output(loc(SR_NETSETTINGS_HELP), true);
            }
            break;

        case 'S':
            if (!ctrl && !shift) {
                announce_summary();
            }
            break;
    }
}

static void run_modal_internal(int startCat, int startItem) {
    if (!NetWin) return;

    _active = true;
    _wantClose = false;
    _confirmed = false;
    _currentCat = startCat;
    _currentItem = startItem;

    sr_debug_log("NetSetupSettingsHandler::RunModal enter (cat=%d item=%d)", startCat, startItem);

    // Load current values from game display
    load_values();

    // Clamp item to valid range after load
    int count = category_count();
    if (_currentItem >= count) _currentItem = count > 0 ? count - 1 : 0;

    sr_output(loc(SR_NETSETTINGS_OPEN), true);
    // Announce starting position if not at default
    if (startCat != 0 || startItem != 0) {
        announce_category();
        announce_item();
    }

    // Run modal pump
    sr_run_modal_pump(&_wantClose);

    // Apply or discard
    if (_confirmed) {
        save_settings();
        sr_output(loc(SR_NETSETTINGS_SAVED), true);
        sr_debug_log("NetSetupSettingsHandler::RunModal confirm");
    } else {
        sr_output(loc(SR_NETSETTINGS_CANCELLED), true);
        sr_debug_log("NetSetupSettingsHandler::RunModal cancel");
    }

    _active = false;
}

void RunModal() {
    run_modal_internal(0, 0);
}

void RunModalAt(int category, int item) {
    run_modal_internal(category, item);
}

} // namespace NetSetupSettingsHandler
