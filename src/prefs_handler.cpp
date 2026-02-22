/*
 * Preferences screen accessibility handler.
 * Replaces the game's Preferences dialog (Ctrl+P) with a keyboard-navigable,
 * screen-reader accessible modal loop. Reads/writes preference bitfields
 * directly and persists via prefs_save().
 */

#include "prefs_handler.h"
#include "engine.h"
#include "gui.h"
#include "screen_reader.h"
#include "localization.h"

namespace PrefsHandler {

enum PrefSource { SRC_PREF, SRC_MORE, SRC_WARN };

struct PrefOption {
    SrStr nameKey;
    PrefSource source;
    uint32_t flag;
    bool inverted; // true = flag set means DISABLED
};

struct PrefTab {
    SrStr nameKey;
    const PrefOption* options;
    int count;
};

// Tab 0: General (8 options)
static const PrefOption general_opts[] = {
    { SR_PREF_PAUSE_END_TURN,    SRC_PREF, 0x1,      false },
    { SR_PREF_AUTOSAVE,          SRC_PREF, 0x2,      false },
    { SR_PREF_QUICK_ENEMY,       SRC_PREF, 0x4,      true  },
    { SR_PREF_TUTORIAL,          SRC_PREF, 0x20,     false },
    { SR_PREF_MOUSE_EDGE_SCROLL, SRC_PREF, 0x1000,   false },
    { SR_PREF_AUTO_DESIGN,       SRC_PREF, 0x4000,   false },
    { SR_PREF_QUICK_ALLY,        SRC_PREF, 0x8000,   true  },
    { SR_PREF_AUTO_PRUNE,        SRC_MORE, 0x100000,  false },
};

// Tab 1: Warnings (17 options)
static const PrefOption warning_opts[] = {
    { SR_PREF_WARN_FAC_BUILT,        SRC_WARN, 0x1,     false },
    { SR_PREF_WARN_NON_COMBAT_BUILT, SRC_WARN, 0x2,     false },
    { SR_PREF_WARN_PROTOTYPE,        SRC_WARN, 0x4,     false },
    { SR_PREF_WARN_DRONE_RIOTS,      SRC_WARN, 0x8,     false },
    { SR_PREF_WARN_DRONE_RIOTS_END,  SRC_WARN, 0x10,    false },
    { SR_PREF_WARN_GOLDEN_AGE,       SRC_WARN, 0x20,    false },
    { SR_PREF_WARN_GOLDEN_AGE_END,   SRC_WARN, 0x40,    false },
    { SR_PREF_WARN_NUTRIENT_SHORTAGE, SRC_WARN, 0x80,   false },
    { SR_PREF_WARN_BUILD_OUT_OF_DATE, SRC_WARN, 0x200,  false },
    { SR_PREF_WARN_COMBAT_BUILT,     SRC_WARN, 0x400,   false },
    { SR_PREF_WARN_POP_LIMIT,        SRC_WARN, 0x800,   false },
    { SR_PREF_WARN_DELAY_TRANSCEND,  SRC_WARN, 0x1000,  false },
    { SR_PREF_WARN_BUILT_VIA_QUEUE,  SRC_WARN, 0x2000,  false },
    { SR_PREF_WARN_STARVATION,       SRC_WARN, 0x4000,  false },
    { SR_PREF_WARN_MINERAL_SHORTAGE, SRC_WARN, 0x8000,  false },
    { SR_PREF_WARN_ENERGY_SHORTAGE,  SRC_WARN, 0x10000, false },
    { SR_PREF_WARN_RANDOM_EVENT,     SRC_WARN, 0x20000, false },
};

// Tab 2: Advanced (11 options)
static const PrefOption advanced_opts[] = {
    { SR_PREF_FAST_BATTLE,       SRC_PREF, 0x8,       false },
    { SR_PREF_NO_CENTER_ORDERS,  SRC_PREF, 0x80000,   false },
    { SR_PREF_PAUSE_AFTER_BATTLE, SRC_MORE, 0x10,     false },
    { SR_PREF_ZOOM_NO_RECENTER,  SRC_MORE, 0x4,       false },
    { SR_PREF_QUICK_MOVE_ORDERS, SRC_MORE, 0x40,      false },
    { SR_PREF_QUICK_MOVE_ALL,    SRC_MORE, 0x80,      false },
    { SR_PREF_RIGHT_CLICK_MENU,  SRC_MORE, 0x100,     false },
    { SR_PREF_DETAIL_RIGHT_CLICK, SRC_MORE, 0x400,    false },
    { SR_PREF_CONFIRM_ODDS,      SRC_MORE, 0x8000,    false },
    { SR_PREF_DETAIL_MAIN_MENUS, SRC_MORE, 0x200000,  false },
    { SR_PREF_RADIO_SINGLE_CLICK, SRC_PREF, 0x20000000, true },
};

// Tab 3: Automation (14 options)
static const PrefOption automation_opts[] = {
    { SR_PREF_FORMER_RAISE_LOWER,  SRC_PREF, 0x20000,    false },
    { SR_PREF_FORMER_PLANT_FOREST, SRC_PREF, 0x1000000,  false },
    { SR_PREF_FORMER_BUILD_ADV,    SRC_PREF, 0x2000000,  false },
    { SR_PREF_FORMER_REMOVE_FUNGUS, SRC_MORE, 0x8,       false },
    { SR_PREF_FORMER_BUILD_SENSORS, SRC_MORE, 0x20,      false },
    { SR_PREF_FORMER_BUILD_ROADS,  SRC_MORE, 0x400000,   true  },
    { SR_PREF_END_MOVE_PACT,       SRC_PREF, 0x100000,   false },
    { SR_PREF_END_MOVE_TREATY,     SRC_PREF, 0x200000,   false },
    { SR_PREF_END_MOVE_TRUCE,      SRC_PREF, 0x400000,   false },
    { SR_PREF_END_MOVE_WAR,        SRC_PREF, 0x800000,   false },
    { SR_PREF_END_MOVE_DIFF_TRIAD, SRC_PREF, 0x40000000, false },
    { SR_PREF_AIR_RETURN_HOME,     SRC_PREF, 0x10000,    false },
    { SR_PREF_WAKE_ON_LAND,        SRC_PREF, 0x80000000, false },
    { SR_PREF_ALWAYS_INSPECT_MONOLITH, SRC_MORE, 0x800,  false },
};

// Tab 4: Audio/Visual (11 options)
static const PrefOption audio_visual_opts[] = {
    { SR_PREF_SOUND_EFFECTS,     SRC_PREF, 0x400,      false },
    { SR_PREF_BACKGROUND_MUSIC,  SRC_PREF, 0x800,      false },
    { SR_PREF_MAP_ANIMATIONS,    SRC_PREF, 0x80,       false },
    { SR_PREF_SLIDING_WINDOWS,   SRC_PREF, 0x8000000,  false },
    { SR_PREF_PROJECT_MOVIES,    SRC_PREF, 0x10000000, false },
    { SR_PREF_INTERLUDES,        SRC_PREF, 0x40000,    true  },
    { SR_PREF_VOICEOVER,         SRC_MORE, 0x4000,     false },
    { SR_PREF_STOP_VOICE_CLOSE,  SRC_MORE, 0x20000,    false },
    { SR_PREF_SLIDING_SCROLLBARS, SRC_MORE, 0x80000,   false },
    { SR_PREF_WHOLE_VEH_BLINKS,  SRC_MORE, 0x200,      false },
    { SR_PREF_MONUMENTS,         SRC_MORE, 0x1000000,  true  },
};

// Tab 5: Map Display (8 options)
static const PrefOption map_display_opts[] = {
    { SR_PREF_SHOW_GRID,         SRC_PREF, 0x100,     false },
    { SR_PREF_SHOW_BASE_GRID,    SRC_PREF, 0x200,     false },
    { SR_PREF_FOG_OF_WAR,        SRC_MORE, 0x1,       false },
    { SR_PREF_BASE_NAMES,        SRC_MORE, 0x2000,    false },
    { SR_PREF_PROD_WITH_NAMES,   SRC_MORE, 0x1000,    false },
    { SR_PREF_FLAT_TERRAIN,      SRC_MORE, 0x10000,   false },
    { SR_PREF_GRID_OCEAN,        SRC_MORE, 0x800000,  false },
    { SR_PREF_SHOW_GOTO_PATH,    SRC_MORE, 0x2000000, true  },
};

static const PrefTab tabs[] = {
    { SR_PREF_TAB_GENERAL,      general_opts,      8  },
    { SR_PREF_TAB_WARNINGS,     warning_opts,      17 },
    { SR_PREF_TAB_ADVANCED,     advanced_opts,     11 },
    { SR_PREF_TAB_AUTOMATION,   automation_opts,   14 },
    { SR_PREF_TAB_AUDIO_VISUAL, audio_visual_opts, 11 },
    { SR_PREF_TAB_MAP_DISPLAY,  map_display_opts,  8  },
};

static const int NUM_TABS = 6;

// State
static bool _active = false;
static bool _wantClose = false;
static bool _confirmed = false;
static int _currentTab = 0;
static int _currentOption = 0;

// Backup values for cancel/restore
static int _backupPref = 0;
static int _backupMore = 0;
static int _backupWarn = 0;

static int* get_field(PrefSource source) {
    switch (source) {
    case SRC_PREF: return GamePreferences;
    case SRC_MORE: return GameMorePreferences;
    case SRC_WARN: return GameWarnings;
    }
    return GamePreferences;
}

static bool is_enabled(const PrefOption& opt) {
    int val = *get_field(opt.source);
    bool set = (val & opt.flag) != 0;
    return opt.inverted ? !set : set;
}

static void toggle_option(const PrefOption& opt) {
    int* field = get_field(opt.source);
    *field ^= opt.flag;
}

static const char* enabled_str(bool on) {
    return on ? loc(SR_PREF_ENABLED) : loc(SR_PREF_DISABLED);
}

static void announce_option() {
    const PrefTab& tab = tabs[_currentTab];
    const PrefOption& opt = tab.options[_currentOption];
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_PREF_OPTION_FMT),
        _currentOption + 1, tab.count,
        loc(opt.nameKey), enabled_str(is_enabled(opt)));
    sr_output(buf, true);
}

static void announce_tab() {
    const PrefTab& tab = tabs[_currentTab];
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_PREF_TAB_FMT),
        loc(tab.nameKey), tab.count);
    sr_output(buf, true);
}

static void announce_summary() {
    const PrefTab& tab = tabs[_currentTab];
    char buf[2048] = "";
    int pos = 0;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s: ", loc(tab.nameKey));

    for (int i = 0; i < tab.count; i++) {
        const PrefOption& opt = tab.options[i];
        if (i > 0) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_PREF_SUMMARY_FMT),
            loc(opt.nameKey), enabled_str(is_enabled(opt)));
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, ".");
    sr_output(buf, true);
}

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return false;

    bool ctrl = ctrl_key_down();

    switch (wParam) {
    case VK_LEFT:
        if (ctrl) return false;
        _currentTab = (_currentTab + NUM_TABS - 1) % NUM_TABS;
        _currentOption = 0;
        announce_tab();
        return true;

    case VK_RIGHT:
        if (ctrl) return false;
        _currentTab = (_currentTab + 1) % NUM_TABS;
        _currentOption = 0;
        announce_tab();
        return true;

    case VK_UP: {
        if (ctrl) return false;
        const PrefTab& tab = tabs[_currentTab];
        _currentOption = (_currentOption + tab.count - 1) % tab.count;
        announce_option();
        return true;
    }

    case VK_DOWN: {
        if (ctrl) return false;
        const PrefTab& tab = tabs[_currentTab];
        _currentOption = (_currentOption + 1) % tab.count;
        announce_option();
        return true;
    }

    case VK_SPACE: {
        const PrefTab& tab = tabs[_currentTab];
        const PrefOption& opt = tab.options[_currentOption];
        toggle_option(opt);
        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_PREF_TOGGLED_FMT),
            loc(opt.nameKey), enabled_str(is_enabled(opt)));
        sr_output(buf, true);
        return true;
    }

    case VK_RETURN:
        _wantClose = true;
        _confirmed = true;
        return true;

    case VK_ESCAPE:
        _wantClose = true;
        _confirmed = false;
        return true;

    case 'I':
        if (ctrl) {
            announce_option();
            return true;
        }
        return false;

    case 'S':
        if (!ctrl) {
            announce_summary();
            return true;
        }
        return false;

    case VK_TAB:
        announce_summary();
        return true;

    case VK_F1:
        if (ctrl) {
            sr_output(loc(SR_PREF_HELP), true);
            return true;
        }
        return false;

    default:
        return false;
    }
}

void RunModal() {
    // Backup current values for cancel
    _backupPref = *GamePreferences;
    _backupMore = *GameMorePreferences;
    _backupWarn = *GameWarnings;

    _active = true;
    _wantClose = false;
    _confirmed = false;
    _currentTab = 0;
    _currentOption = 0;

    sr_debug_log("PrefsHandler::RunModal enter\n");
    announce_tab();

    // Modal message pump
    MSG msg;
    while (!_wantClose) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Sleep(10);
        }
    }

    // Drain leftover messages
    MSG drain;
    while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
    while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

    if (_confirmed) {
        // Sync game globals to AlphaIniPrefs before saving
        AlphaIniPrefs->preferences = *GamePreferences;
        AlphaIniPrefs->more_preferences = *GameMorePreferences;
        AlphaIniPrefs->announce = *GameWarnings;
        prefs_save(0);
        sr_debug_log("PrefsHandler::RunModal confirm\n");
        sr_output(loc(SR_PREF_SAVED), true);
    } else {
        // Restore from backup
        *GamePreferences = _backupPref;
        *GameMorePreferences = _backupMore;
        *GameWarnings = _backupWarn;
        sr_debug_log("PrefsHandler::RunModal cancel\n");
        sr_output(loc(SR_PREF_CANCELLED), true);
    }

    _active = false;

    // Refresh map display
    draw_map(1);
}

} // namespace PrefsHandler
