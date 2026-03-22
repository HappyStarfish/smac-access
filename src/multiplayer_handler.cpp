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
#include "engine.h"

// Game globals (from engine.h / engine.cpp)
extern Win* NetWin;
extern int* const GameHalted;
extern HWND* phWnd;

// From gui.cpp
extern int __thiscall Win_is_visible(Win* This);

// Forward declaration for popup list state
extern SrPopupList sr_popup_list;

// Player slot data array (reverse-engineered from terranx.exe click handler at 0x47B760).
// Array at 0x90DB98 with 0x17C (380) byte stride, 1-based indexing (slots 1-7).
static const unsigned int SLOT_ARRAY_BASE = 0x90DB98;
static const int SLOT_STRIDE = 0x17C;
// Per-slot field offsets
static const int SLOT_OFF_STATUS     = 0x00;  // byte: 0x01=Human, 0xFF=Computer, 0x00=Closed
static const int SLOT_OFF_DIFFICULTY = 0x02;  // byte: 0-5
static const int SLOT_OFF_FACTION    = 0x03;  // byte: 0xFF=random, 0-13=faction
static const int SLOT_OFF_NAME       = 0x05;  // char[24]: leader name

// --- Popup override for calling game's faction/difficulty handlers ---
// When set to >= 0, the hooked popup call returns this value instead of showing UI.
// The game handler then processes the result normally (init, broadcast, etc.)
static int _popupOverride = -1;

// Original popup function at 0x59D250 (__thiscall, ecx=popup_obj, returns int)
typedef int(__thiscall *FPopupDisplay)(void* This);
static FPopupDisplay _origPopupDisplay = (FPopupDisplay)0x59D250;

// Hook for popup calls inside faction/difficulty handlers.
// If override is set, return it. Otherwise call original.
static int __fastcall hook_popup_display(void* This, void* /*edx*/) {
    if (_popupOverride >= 0) {
        int result = _popupOverride;
        sr_debug_log("NETSETUP: popup override -> %d", result);
        return result;
    }
    return _origPopupDisplay(This);
}

// Spot coordinates for player slots (from SpotList log data).
// type=3 = faction column, type=4 = difficulty column, type=5 = status toggle.
// Center of each spot rect, indexed by slot 0-6 (display order).
static const int SPOT_FACTION_X = 603;   // center of (533,673)
static const int SPOT_DIFFICULTY_X = 734; // center of (675,793)
static const int SPOT_STATUS_X = 343;    // center of (335,351)
static const int SPOT_Y[] = { 68, 90, 112, 134, 156, 178, 200 }; // center of 20px rows

// Player zone sub-columns
enum PlayerColumn { PCOL_STATUS = 0, PCOL_FACTION, PCOL_DIFFICULTY, PCOL_COUNT };

static unsigned char* slot_ptr(int slot_1based) {
    return (unsigned char*)(SLOT_ARRAY_BASE + slot_1based * SLOT_STRIDE);
}

static const char* slot_status_str(int slot_1based) {
    unsigned char status = slot_ptr(slot_1based)[SLOT_OFF_STATUS];
    if (status == 0x01) return loc(SR_NETSETUP_SLOT_HUMAN);
    if (status == 0xFF) return loc(SR_NETSETUP_SLOT_COMPUTER);
    return loc(SR_NETSETUP_SLOT_CLOSED);
}

// Max valid faction ID for SMACX (factions 1-14 in MFactions array)
static const int MAX_FACTION_ID = 14;

static const char* slot_faction_str(int slot_1based) {
    signed char fid = (signed char)slot_ptr(slot_1based)[SLOT_OFF_FACTION];
    if (fid < 1 || fid > MAX_FACTION_ID) return loc(SR_NETSETUP_FACTION_RANDOM);
    // Read faction adjective name from MFactions array (Windows-1252 -> UTF-8)
    if (MFactions && !IsBadReadPtr(&MFactions[fid], sizeof(MFaction))) {
        const char* name = MFactions[fid].adj_name_faction;
        if (name && name[0]) return sr_game_str(name);
    }
    return loc(SR_NETSETUP_FACTION_RANDOM);
}

static const char* slot_difficulty_str(int slot_1based) {
    int diff = slot_ptr(slot_1based)[SLOT_OFF_DIFFICULTY];
    static const SrStr diff_keys[] = {
        SR_NETSETUP_DIFF_CITIZEN, SR_NETSETUP_DIFF_SPECIALIST,
        SR_NETSETUP_DIFF_TALENT, SR_NETSETUP_DIFF_LIBRARIAN,
        SR_NETSETUP_DIFF_THINKER, SR_NETSETUP_DIFF_TRANSCEND
    };
    if (diff >= 0 && diff <= 5) return loc(diff_keys[diff]);
    return "?";
}

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

// Checkbox items with direct memory addresses for toggle.
// Addresses and bitmasks from disassembly of click handler at 0x47B760.
// Game stores checkbox state as XOR bits in globals at 0x90E8EC/F0/F4.
struct CheckboxItem {
    SrStr nameKey;
    uint32_t* addr;   // global address (0x90E8EC, 0x90E8F0, or 0x90E8F4)
    uint32_t mask;     // XOR bitmask for this checkbox
};

static const CheckboxItem checkboxes[] = {
    // Left column (spot positions: 100, 0-7)
    { SR_NETSETUP_CB_SIMULTANEOUS,    (uint32_t*)0x90E8F4, 0x10 },
    { SR_NETSETUP_CB_TRANSCENDENCE,   (uint32_t*)0x90E8EC, 0x0800 },
    { SR_NETSETUP_CB_CONQUEST,        (uint32_t*)0x90E8EC, 0x02 },
    { SR_NETSETUP_CB_DIPLOMATIC,      (uint32_t*)0x90E8EC, 0x08 },
    { SR_NETSETUP_CB_ECONOMIC,        (uint32_t*)0x90E8EC, 0x04 },
    { SR_NETSETUP_CB_COOPERATIVE,     (uint32_t*)0x90E8EC, 0x1000 },
    { SR_NETSETUP_CB_DO_OR_DIE,       (uint32_t*)0x90E8EC, 0x01 },
    { SR_NETSETUP_CB_LOOK_FIRST,      (uint32_t*)0x90E8EC, 0x10 },
    { SR_NETSETUP_CB_TECH_STAGNATION, (uint32_t*)0x90E8EC, 0x20 },
    // Right column (spot positions: 8-14, 16-17)
    { SR_NETSETUP_CB_SPOILS,          (uint32_t*)0x90E8EC, 0x4000 },
    { SR_NETSETUP_CB_BLIND_RESEARCH,  (uint32_t*)0x90E8EC, 0x0200 },
    { SR_NETSETUP_CB_INTENSE_RIVALRY, (uint32_t*)0x90E8EC, 0x40 },
    { SR_NETSETUP_CB_NO_UNITY_SURVEY, (uint32_t*)0x90E8EC, 0x0100 },
    { SR_NETSETUP_CB_NO_UNITY_SCATTER,(uint32_t*)0x90E8EC, 0x2000 },
    { SR_NETSETUP_CB_BELL_CURVE,      (uint32_t*)0x90E8EC, 0x8000 },
    { SR_NETSETUP_CB_TIME_WARP,       (uint32_t*)0x90E8EC, 0x80 },
    { SR_NETSETUP_CB_RAND_PERSONALITY,(uint32_t*)0x90E8F0, 0x800000 },
    { SR_NETSETUP_CB_RAND_SOCIAL,     (uint32_t*)0x90E8F0, 0x1000000 },
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
static DWORD _suppressTimerUntil = 0;  // suppress OnTimer until this tick count
static int _currentZone = ZONE_SETTINGS;
static int _currentIndex = 0;
static int _playerColumn = PCOL_STATUS;  // current column in player zone

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

// Checkbox state is read directly from game memory (0x90E8EC/F0/F4).
// No local tracking needed.

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

// Settings are stored as individual BYTES in a global block at 0x90E8E0.
// Discovered by disassembling the click handler and byte-scanning the block.
// The handler functions (0x47E340 etc.) write popup results as single bytes.
struct SettingField {
    unsigned char* addr;  // global byte address (NULL = not discovered)
    int max_val;          // maximum value (cycles 0..max_val)
};

static SettingField setting_fields[] = {
    { (unsigned char*)0x90E8E2, 5 },  // [0] Difficulty: 0-5
    { (unsigned char*)0x90E8E3, 5 },  // [1] Time controls: 0=Aus,1=Kurz,...
    { NULL, 0 },                       // [2] Game type: byte unknown
    { (unsigned char*)0x90E8E6, 4 },  // [3] Planet size: 0-4
    { (unsigned char*)0x90E8E7, 2 },  // [4] Ocean coverage: 0-2
    { (unsigned char*)0x90E8E8, 2 },  // [5] Erosion: 0-2
    { (unsigned char*)0x90E8E9, 2 },  // [6] Native life: 0-2
    { (unsigned char*)0x90E8EA, 2 },  // [7] Cloud cover: 0-2
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

/// Cycle a setting value: increment (or decrement with dir=-1), write byte
/// to global address, force redraw, and announce the new value text.
static void cycle_setting(int idx, int dir) {
    if (idx < 0 || idx >= SETTINGS_COUNT) return;
    if (!NetWin) return;

    SettingField& f = setting_fields[idx];
    if (!f.addr) {
        // Address not known — just re-announce current value
        char val_text[256];
        read_setting_text(idx, val_text, sizeof(val_text));
        char buf[512];
        snprintf(buf, sizeof(buf), "%s: %s",
            loc(settings[idx].nameKey), val_text[0] ? val_text : "?");
        sr_output(buf, true);
        return;
    }

    int old_val = *f.addr;
    int new_val = old_val + dir;
    if (new_val > f.max_val) new_val = 0;
    if (new_val < 0) new_val = f.max_val;
    *f.addr = (unsigned char)new_val;

    sr_debug_log("NETSETUP: cycle setting %d (%s) @0x%X: %d -> %d",
        idx, loc(settings[idx].nameKey), (unsigned int)(uintptr_t)f.addr,
        old_val, new_val);

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

/// Toggle a checkbox by directly XORing the bit in game memory.
/// Same operation as the game's click handler at 0x47B760.
static void activate_checkbox(int idx) {
    if (idx < 0 || idx >= CHECKBOXES_COUNT) return;
    const CheckboxItem& cb = checkboxes[idx];
    uint32_t old_val = *cb.addr;
    *cb.addr ^= cb.mask;
    bool now_enabled = (*cb.addr & cb.mask) != 0;
    sr_debug_log("NETSETUP: toggle checkbox %d (%s) @0x%X ^= 0x%X: 0x%X -> 0x%X (%s)",
        idx, loc(cb.nameKey), (unsigned int)(uintptr_t)cb.addr, cb.mask,
        old_val, *cb.addr, now_enabled ? "ON" : "OFF");
    // Force redraw so the visual display matches
    if (NetWin) {
        GraphicWin_redraw(NetWin);
    }
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
    // If Start button, dump slot data before launch for diagnostics
    if (idx == 1) {
        _launching = true;
        sr_debug_log("NETSETUP: === SLOT DATA AT START ===");
        unsigned char* slot_base = (unsigned char*)SLOT_ARRAY_BASE;
        for (int s = 1; s <= 7; s++) {
            unsigned char* sp = slot_base + s * SLOT_STRIDE;
            if (IsBadReadPtr(sp, SLOT_STRIDE)) continue;
            sr_debug_log("  slot[%d]: status=0x%02X diff=%d faction=%d name=[%.24s]",
                s, sp[0], sp[2], (int)(signed char)sp[3], (const char*)(sp + 5));
        }
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
            int slot = _currentIndex + 1;
            const char* status = slot_status_str(slot);
            const char* faction = slot_faction_str(slot);
            const char* diff = slot_difficulty_str(slot);
            snprintf(buf, sizeof(buf), loc(SR_NETSETUP_SLOT_FMT),
                _currentIndex + 1, count, status, faction, diff);
            break;
        }
        case ZONE_CHECKBOXES: {
            const CheckboxItem& cb = checkboxes[_currentIndex];
            bool enabled = (*cb.addr & cb.mask) != 0;
            snprintf(buf, sizeof(buf), loc(SR_NETSETUP_CHECKBOX_FMT),
                _currentIndex + 1, count,
                loc(cb.nameKey),
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

/// Announce only the current player column value (short form for Left/Right and Enter).
static void announce_player_column() {
    int slot = _currentIndex + 1;
    char buf[256];
    const char* col_name = "";
    const char* value = "";
    if (_playerColumn == PCOL_STATUS) {
        col_name = loc(SR_NETSETUP_COL_STATUS);
        value = slot_status_str(slot);
    } else if (_playerColumn == PCOL_FACTION) {
        col_name = loc(SR_NETSETUP_COL_FACTION);
        value = slot_faction_str(slot);
    } else if (_playerColumn == PCOL_DIFFICULTY) {
        col_name = loc(SR_NETSETUP_COL_DIFFICULTY);
        value = slot_difficulty_str(slot);
    }
    snprintf(buf, sizeof(buf), "%s: %s", col_name, value);
    sr_output(buf, true);
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
    _playerColumn = PCOL_STATUS;
    // Clear player text
    for (int i = 0; i < PLAYER_COUNT; i++) {
        _lastPlayerText[i][0] = '\0';
    }
    // Checkbox state is read directly from game memory (0x90E8EC/F0/F4).
    // No local init needed — announce_current reads the real bits.
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

    // Dump player slot data from the per-slot array at 0x90DB98.
    // Stride: 0x17C (380 bytes), 1-based indexing (slots 1-7).
    // Fields: +0x00=status, +0x02=difficulty, +0x03=faction, +0x05=name, +0x178=H/C flag
    {
        sr_debug_log("NETSETUP: === PLAYER SLOT DATA (0x90DB98 + N*0x17C) ===");
        unsigned char* slot_base = (unsigned char*)0x90DB98;
        for (int s = 1; s <= 7; s++) {
            unsigned char* slot = slot_base + s * 0x17C;
            if (IsBadReadPtr(slot, 0x17C)) {
                sr_debug_log("  slot[%d]: UNREADABLE at %p", s, (void*)slot);
                continue;
            }
            int status = slot[0];
            int difficulty = slot[2];
            int faction = (int)(signed char)slot[3];
            const char* name = (const char*)(slot + 5);
            int hc_flag = *(int*)(slot + 0x178);
            // Also dump first 16 bytes for field verification
            char hex[64];
            int pos = 0;
            for (int b = 0; b < 16; b++) {
                pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", slot[b]);
            }
            sr_debug_log("  slot[%d] @0x%X: status=0x%02X diff=%d faction=%d hc=%d name=[%.24s]",
                s, (unsigned int)(uintptr_t)slot, status, difficulty, faction, hc_flag, name);
            sr_debug_log("    raw: %s", hex);
        }
        // Also log key globals
        sr_debug_log("  LocalPlayerID(@0x93D4F0) = %d", *(int*)0x93D4F0);
        sr_debug_log("  NetworkMode(@0x90E778) = %d", *(int*)0x90E778);
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

    // Left/Right: context-dependent
    if (wParam == VK_LEFT || wParam == VK_RIGHT) {
        int dir = (wParam == VK_RIGHT) ? 1 : -1;
        if (_currentZone == ZONE_SETTINGS) {
            cycle_setting(_currentIndex, dir);
            return true;
        }
        if (_currentZone == ZONE_PLAYERS) {
            // Switch column within player zone
            _playerColumn += dir;
            if (_playerColumn < 0) _playerColumn = PCOL_COUNT - 1;
            if (_playerColumn >= PCOL_COUNT) _playerColumn = 0;
            announce_player_column();
            return true;
        }
        return false;
    }

    // Enter or Space: activate
    if (wParam == VK_RETURN || wParam == VK_SPACE) {
        switch (_currentZone) {
            case ZONE_SETTINGS:
                cycle_setting(_currentIndex, 1);
                break;
            case ZONE_CHECKBOXES:
                activate_checkbox(_currentIndex);
                break;
            case ZONE_BUTTONS:
                activate_button(_currentIndex);
                break;
            case ZONE_PLAYERS: {
                int slot = _currentIndex + 1;  // 1-based
                unsigned char* sp = slot_ptr(slot);

                if (_playerColumn == PCOL_STATUS) {
                    unsigned char cur = sp[SLOT_OFF_STATUS];
                    if (cur == 0x01) {
                        // Human — can't change own status
                    } else if (cur == 0xFF) {
                        // Computer -> Closed
                        sp[SLOT_OFF_STATUS] = 0x00;
                        sr_debug_log("NETSETUP: close slot %d", slot);
                    } else {
                        // Closed -> Computer
                        sp[SLOT_OFF_STATUS] = 0xFF;
                        sr_debug_log("NETSETUP: reopen slot %d", slot);
                    }
                } else if (_playerColumn == PCOL_FACTION) {
                    // Cycle faction: random(-1) -> 1 -> ... -> 14 -> random
                    // Direct byte write + name copy from MFactions.
                    // NOTE: This does NOT call the game's full init (0x47C970).
                    // The game may not fully recognize the faction at start.
                    // See project_status.md for next steps.
                    signed char fid = (signed char)sp[SLOT_OFF_FACTION];
                    fid++;
                    if (fid == 0) fid = 1;
                    if (fid > MAX_FACTION_ID) fid = -1;
                    sp[SLOT_OFF_FACTION] = (unsigned char)fid;
                    if (fid >= 1 && fid <= MAX_FACTION_ID
                        && MFactions && !IsBadReadPtr(&MFactions[fid], sizeof(MFaction))) {
                        strncpy((char*)(sp + SLOT_OFF_NAME),
                            MFactions[fid].name_leader, 23);
                        sp[SLOT_OFF_NAME + 23] = '\0';
                    } else {
                        sp[SLOT_OFF_NAME] = '\0';
                    }
                    sr_debug_log("NETSETUP: cycle slot %d faction -> %d name=[%.24s]",
                        slot, (int)fid, (const char*)(sp + SLOT_OFF_NAME));
                } else if (_playerColumn == PCOL_DIFFICULTY) {
                    // Cycle difficulty: 0-5
                    int diff = sp[SLOT_OFF_DIFFICULTY];
                    diff++;
                    if (diff > 5) diff = 0;
                    sp[SLOT_OFF_DIFFICULTY] = (unsigned char)diff;
                    sr_debug_log("NETSETUP: cycle slot %d difficulty -> %d", slot, diff);
                }
                // Suppress OnTimer briefly so it doesn't re-announce the change
                _suppressTimerUntil = GetTickCount() + 1500;
                // Force visual redraw and announce just the changed column
                if (NetWin) GraphicWin_redraw(NetWin);
                announce_player_column();
                break;
            }
        }
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
            case ZONE_PLAYERS: {
                int slot = _currentIndex + 1;
                item_name = slot_status_str(slot);
                break;
            }
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
    // Suppress briefly after player slot changes to avoid interrupting announce
    if (_suppressTimerUntil && GetTickCount() < _suppressTimerUntil) return;
    _suppressTimerUntil = 0;

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

void InstallHooks() {
    // Popup hooks disabled — direct_click + write_call approach crashed.
    // See project_status.md for analysis and next steps.
    sr_debug_log("NETSETUP: InstallHooks (no hooks installed yet)");
}

} // namespace MultiplayerHandler
