/*
 * Thinker Menu accessibility handler (Alt+T).
 * Replaces the game's popup-based Thinker menu with a keyboard-navigable,
 * screen-reader accessible modal loop. Four modes:
 *   1. Main menu — navigate items, Enter to select
 *   2. Statistics — read-only, any key closes
 *   3. Mod Options — checkbox list, Space to toggle, Enter to save
 *   4. Game Rules — checkbox list, Space to toggle, Enter to apply
 */

#include "thinker_menu_handler.h"
#include "engine.h"
#include "engine_enums.h"
#include "gui.h"
#include "game.h"
#include "modal_utils.h"
#include "screen_reader.h"
#include "localization.h"

namespace ThinkerMenuHandler {

// --- Config option descriptors ---

struct ConfigOption {
    SrStr nameKey;
    int* confField;
    const char* iniKey;
};

static const ConfigOption config_opts[] = {
    { SR_TMENU_OPT_NEW_WORLD,     &conf.new_world_builder,     "new_world_builder"     },
    { SR_TMENU_OPT_CONTINENTS,    &conf.world_continents,      "world_continents"      },
    { SR_TMENU_OPT_LANDMARKS,     &conf.modified_landmarks,    "modified_landmarks"    },
    { SR_TMENU_OPT_POLAR_CAPS,    &conf.world_polar_caps,      "world_polar_caps"      },
    { SR_TMENU_OPT_MIRROR_X,      &conf.world_mirror_x,        "world_mirror_x"        },
    { SR_TMENU_OPT_MIRROR_Y,      &conf.world_mirror_y,        "world_mirror_y"        },
    { SR_TMENU_OPT_AUTO_BASES,    &conf.manage_player_bases,   "manage_player_bases"   },
    { SR_TMENU_OPT_AUTO_UNITS,    &conf.manage_player_units,   "manage_player_units"   },
    { SR_TMENU_OPT_FORMER_WARN,   &conf.warn_on_former_replace,"warn_on_former_replace"},
    { SR_TMENU_OPT_BASE_INFO,     &conf.render_base_info,      "render_base_info"      },
    { SR_TMENU_OPT_TREATY_POPUP,  &conf.foreign_treaty_popup,  "foreign_treaty_popup"  },
    { SR_TMENU_OPT_AUTO_MINIMISE, &conf.auto_minimise,         "auto_minimise"         },
};
static const int NUM_CONFIG_OPTS = sizeof(config_opts) / sizeof(config_opts[0]);

// --- Game rules descriptors (reuse loc keys from GameSettingsHandler) ---

struct RuleOption {
    SrStr nameKey;
    uint32_t flag;
    bool isMoreRules; // true = GameMoreRules, false = GameRules
};

static const RuleOption rule_opts[] = {
    { SR_GSETTINGS_RULE_CONQUEST,       RULES_VICTORY_CONQUEST,      false },
    { SR_GSETTINGS_RULE_ECONOMIC,       RULES_VICTORY_ECONOMIC,      false },
    { SR_GSETTINGS_RULE_DIPLOMATIC,     RULES_VICTORY_DIPLOMATIC,    false },
    { SR_GSETTINGS_RULE_TRANSCENDENCE,  RULES_VICTORY_TRANSCENDENCE, false },
    { SR_GSETTINGS_RULE_COOPERATIVE,    RULES_VICTORY_COOPERATIVE,   false },
    { SR_GSETTINGS_RULE_DO_OR_DIE,      RULES_DO_OR_DIE,             false },
    { SR_GSETTINGS_RULE_LOOK_FIRST,     RULES_LOOK_FIRST,            false },
    { SR_GSETTINGS_RULE_TECH_STAGNATION,RULES_TECH_STAGNATION,       false },
    { SR_GSETTINGS_RULE_INTENSE_RIVALRY,RULES_INTENSE_RIVALRY,        false },
    { SR_GSETTINGS_RULE_TIME_WARP,      RULES_TIME_WARP,             false },
    { SR_GSETTINGS_RULE_NO_UNITY_SURVEY,RULES_NO_UNITY_SURVEY,       false },
    { SR_GSETTINGS_RULE_BLIND_RESEARCH, RULES_BLIND_RESEARCH,        false },
    { SR_GSETTINGS_RULE_NO_UNITY_SCATTER,RULES_NO_UNITY_SCATTERING,  false },
    { SR_GSETTINGS_RULE_SPOILS_OF_WAR,  RULES_SPOILS_OF_WAR,         false },
    { SR_GSETTINGS_RULE_BELL_CURVE,     RULES_BELL_CURVE,            false },
    { SR_TMENU_RULE_NO_COUNCIL,         MRULES_NO_PLANETARY_COUNCIL, true  },
    { SR_TMENU_RULE_NO_SOCIAL_ENG,      MRULES_NO_SOCIAL_ENGINEERING,true  },
};
static const int NUM_RULE_OPTS = sizeof(rule_opts) / sizeof(rule_opts[0]);

// --- Mode enum ---

enum MenuMode {
    MODE_MAIN,
    MODE_STATS,
    MODE_OPTIONS,
    MODE_RULES,
};

// --- State ---

static bool _active = false;
static bool _wantClose = false;
static MenuMode _mode = MODE_MAIN;
static int _currentIndex = 0;
static bool _confirmed = false;

// Backup of config values for cancel in options mode
static int _backup[NUM_CONFIG_OPTS];

// Backup of game rules for cancel in rules mode
static int _rulesBackup = 0;
static int _moreRulesBackup = 0;

// Main menu items (dynamic based on game state)
enum MainItem {
    ITEM_STATS = 0,
    ITEM_OPTIONS,
    ITEM_RULES,     // only in MAINMENU (GameHalted)
    ITEM_HOMEPAGE,
    ITEM_CLOSE,
};

static int _menuItems[8];
static int _menuCount = 0;

// --- Helpers ---

static void build_menu_items() {
    _menuCount = 0;
    if (!*GameHalted && !*PbemActive && !*MultiplayerActive) {
        _menuItems[_menuCount++] = ITEM_STATS;
    }
    _menuItems[_menuCount++] = ITEM_OPTIONS;
    if (*GameHalted) {
        _menuItems[_menuCount++] = ITEM_RULES;
    }
    _menuItems[_menuCount++] = ITEM_HOMEPAGE;
    _menuItems[_menuCount++] = ITEM_CLOSE;
}

static const char* menu_item_name(int item) {
    switch (item) {
    case ITEM_STATS:    return loc(SR_TMENU_STATS);
    case ITEM_OPTIONS:  return loc(SR_TMENU_OPTIONS);
    case ITEM_RULES:    return loc(SR_TMENU_RULES);
    case ITEM_HOMEPAGE: return loc(SR_TMENU_HOMEPAGE);
    case ITEM_CLOSE:    return loc(SR_TMENU_CLOSE);
    default:            return "";
    }
}

static void announce_menu_item() {
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_TMENU_ITEM_FMT),
        _currentIndex + 1, _menuCount,
        menu_item_name(_menuItems[_currentIndex]));
    sr_output(buf, false);
}

static void announce_version() {
    char buf[512];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_TMENU_VERSION_FMT),
        MOD_VERSION, MOD_DATE);

    if (!*GameHalted) {
        uint64_t seconds = ThinkerVars->game_time_spent / 1000;
        int hours = (int)(seconds / 3600);
        int minutes = (int)((seconds / 60) % 60);
        pos += snprintf(buf + pos, sizeof(buf) - pos, ". ");
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_TMENU_GAMETIME_FMT),
            hours, minutes);
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos, ". %s", loc(SR_TMENU_TITLE));
    sr_output(buf, false);
}

static void announce_config_option() {
    char buf[512];
    const ConfigOption& opt = config_opts[_currentIndex];
    snprintf(buf, sizeof(buf), loc(SR_TMENU_OPT_FMT),
        _currentIndex + 1, NUM_CONFIG_OPTS,
        loc(opt.nameKey),
        *opt.confField ? loc(SR_TMENU_OPT_ON) : loc(SR_TMENU_OPT_OFF));
    sr_output(buf, true);
}

static void announce_config_summary() {
    char buf[2048];
    int pos = 0;
    for (int i = 0; i < NUM_CONFIG_OPTS; i++) {
        const ConfigOption& opt = config_opts[i];
        if (i > 0) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s: %s",
            loc(opt.nameKey),
            *opt.confField ? loc(SR_TMENU_OPT_ON) : loc(SR_TMENU_OPT_OFF));
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, ".");
    sr_output(buf, true);
}

static void announce_stats() {
    int total_pop = 0, total_minerals = 0, total_energy = 0;
    int faction_bases = 0, faction_pop = 0, faction_units = 0;
    int faction_minerals = 0, faction_energy = 0;

    for (int i = 0; i < *BaseCount; ++i) {
        BASE* b = &Bases[i];
        int mindiv = (has_project(FAC_SPACE_ELEVATOR, b->faction_id)
             && (b->item() == -FAC_ORBITAL_DEFENSE_POD
             || b->item() == -FAC_NESSUS_MINING_STATION
             || b->item() == -FAC_ORBITAL_POWER_TRANS
             || b->item() == -FAC_SKY_HYDRO_LAB) ? 2 : 1);
        if (b->faction_id == MapWin->cOwner) {
            faction_bases++;
            faction_pop += b->pop_size;
            faction_minerals += b->mineral_intake_2 / mindiv;
            faction_energy += b->energy_intake_2;
        }
        total_pop += b->pop_size;
        total_minerals += b->mineral_intake_2 / mindiv;
        total_energy += b->energy_intake_2;
    }
    for (int i = 0; i < *VehCount; i++) {
        VEH* v = &Vehs[i];
        if (v->faction_id == MapWin->cOwner) {
            faction_units++;
        }
    }

    char buf[1024];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s ", loc(SR_TMENU_STAT_HEADER));
    pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_TMENU_STAT_WORLD),
        *BaseCount, *VehCount, total_pop, total_minerals, total_energy);
    pos += snprintf(buf + pos, sizeof(buf) - pos, " ");
    pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_TMENU_STAT_FACTION),
        faction_bases, faction_units, faction_pop, faction_minerals, faction_energy);
    sr_output(buf, true);
}

static bool is_rule_enabled(const RuleOption& r) {
    if (r.isMoreRules) return !!(*GameMoreRules & r.flag);
    return !!(*GameRules & r.flag);
}

static void toggle_rule(const RuleOption& r) {
    if (r.isMoreRules) {
        *GameMoreRules ^= r.flag;
    } else {
        *GameRules ^= r.flag;
    }
}

static void announce_rule_option() {
    char buf[512];
    const RuleOption& r = rule_opts[_currentIndex];
    snprintf(buf, sizeof(buf), loc(SR_TMENU_OPT_FMT),
        _currentIndex + 1, NUM_RULE_OPTS,
        loc(r.nameKey),
        is_rule_enabled(r) ? loc(SR_TMENU_OPT_ON) : loc(SR_TMENU_OPT_OFF));
    sr_output(buf, true);
}

static void announce_rules_summary() {
    char buf[2048];
    int pos = 0;
    for (int i = 0; i < NUM_RULE_OPTS; i++) {
        const RuleOption& r = rule_opts[i];
        if (i > 0) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s: %s",
            loc(r.nameKey),
            is_rule_enabled(r) ? loc(SR_TMENU_OPT_ON) : loc(SR_TMENU_OPT_OFF));
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, ".");
    sr_output(buf, true);
}

static void save_config_backup() {
    for (int i = 0; i < NUM_CONFIG_OPTS; i++) {
        _backup[i] = *config_opts[i].confField;
    }
}

static void restore_config_backup() {
    for (int i = 0; i < NUM_CONFIG_OPTS; i++) {
        *config_opts[i].confField = _backup[i];
    }
}

static void save_config_to_ini() {
    for (int i = 0; i < NUM_CONFIG_OPTS; i++) {
        WritePrivateProfileStringA(ModAppName, config_opts[i].iniKey,
            (*config_opts[i].confField ? "1" : "0"), ModIniFile);
    }
}

// --- Mode transitions ---

static void enter_stats_mode() {
    _mode = MODE_STATS;
    sr_debug_log("ThinkerMenuHandler: enter stats mode\n");
    announce_stats();
}

static void enter_options_mode() {
    _mode = MODE_OPTIONS;
    _currentIndex = 0;
    _confirmed = false;
    save_config_backup();
    sr_debug_log("ThinkerMenuHandler: enter options mode\n");

    char buf[512];
    snprintf(buf, sizeof(buf), "%s. ", loc(SR_TMENU_OPTIONS));
    sr_output(buf, false);
    announce_config_option();
}

static void enter_rules_mode() {
    _mode = MODE_RULES;
    _currentIndex = 0;
    _rulesBackup = *GameRules;
    _moreRulesBackup = *GameMoreRules;
    sr_debug_log("ThinkerMenuHandler: enter rules mode\n");

    char buf[512];
    snprintf(buf, sizeof(buf), "%s. ", loc(SR_TMENU_RULES));
    sr_output(buf, false);
    announce_rule_option();
}

static void return_to_main() {
    _mode = MODE_MAIN;
    _currentIndex = 0;
    announce_version();
    announce_menu_item();
}

// --- Public API ---

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return false;

    bool ctrl = ctrl_key_down();

    // Stats mode: any key returns to main menu
    if (_mode == MODE_STATS) {
        if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE) {
            return_to_main();
            return true;
        }
        // Ctrl+F1: help
        if (wParam == VK_F1 && ctrl) {
            sr_output(loc(SR_TMENU_HELP), true);
            return true;
        }
        return true; // consume all keys in stats mode
    }

    // Options mode
    if (_mode == MODE_OPTIONS) {
        switch (wParam) {
        case VK_UP:
            if (ctrl) return false;
            _currentIndex = (_currentIndex + NUM_CONFIG_OPTS - 1) % NUM_CONFIG_OPTS;
            announce_config_option();
            return true;

        case VK_DOWN:
            if (ctrl) return false;
            _currentIndex = (_currentIndex + 1) % NUM_CONFIG_OPTS;
            announce_config_option();
            return true;

        case VK_SPACE: {
            ConfigOption const& opt = config_opts[_currentIndex];
            *opt.confField = !*opt.confField;
            announce_config_option();
            return true;
        }

        case VK_RETURN:
            save_config_to_ini();
            sr_output(loc(SR_TMENU_OPT_SAVED), true);
            sr_debug_log("ThinkerMenuHandler: options saved\n");
            draw_map(1);
            if (Win_is_visible(BaseWin)) {
                BaseWin_on_redraw(BaseWin);
            }
            return_to_main();
            return true;

        case VK_ESCAPE:
            restore_config_backup();
            sr_output(loc(SR_TMENU_OPT_CANCELLED), true);
            sr_debug_log("ThinkerMenuHandler: options cancelled\n");
            return_to_main();
            return true;

        case 'S':
            if (!ctrl) {
                announce_config_summary();
                return true;
            }
            return false;

        case VK_F1:
            if (ctrl) {
                sr_output(loc(SR_TMENU_OPT_HELP), true);
                return true;
            }
            return false;

        default:
            return true; // consume other keys
        }
    }

    // Rules mode
    if (_mode == MODE_RULES) {
        switch (wParam) {
        case VK_UP:
            if (ctrl) return false;
            _currentIndex = (_currentIndex + NUM_RULE_OPTS - 1) % NUM_RULE_OPTS;
            announce_rule_option();
            return true;

        case VK_DOWN:
            if (ctrl) return false;
            _currentIndex = (_currentIndex + 1) % NUM_RULE_OPTS;
            announce_rule_option();
            return true;

        case VK_SPACE:
            toggle_rule(rule_opts[_currentIndex]);
            announce_rule_option();
            return true;

        case VK_RETURN:
            // Apply: update Thinker's custom_game_rules tracking
            sr_output(loc(SR_TMENU_RULES_SAVED), true);
            sr_debug_log("ThinkerMenuHandler: rules saved\n");
            return_to_main();
            return true;

        case VK_ESCAPE:
            *GameRules = _rulesBackup;
            *GameMoreRules = _moreRulesBackup;
            sr_output(loc(SR_TMENU_RULES_CANCELLED), true);
            sr_debug_log("ThinkerMenuHandler: rules cancelled\n");
            return_to_main();
            return true;

        case 'S':
            if (!ctrl) {
                announce_rules_summary();
                return true;
            }
            return false;

        case VK_F1:
            if (ctrl) {
                sr_output(loc(SR_TMENU_OPT_HELP), true);
                return true;
            }
            return false;

        default:
            return true;
        }
    }

    // Main menu mode
    switch (wParam) {
    case VK_UP:
        if (ctrl) return false;
        _currentIndex = (_currentIndex + _menuCount - 1) % _menuCount;
        announce_menu_item();
        return true;

    case VK_DOWN:
        if (ctrl) return false;
        _currentIndex = (_currentIndex + 1) % _menuCount;
        announce_menu_item();
        return true;

    case VK_SPACE:
    case VK_RETURN: {
        int item = _menuItems[_currentIndex];
        switch (item) {
        case ITEM_STATS:
            enter_stats_mode();
            return true;
        case ITEM_OPTIONS:
            enter_options_mode();
            return true;
        case ITEM_RULES:
            enter_rules_mode();
            return true;
        case ITEM_HOMEPAGE:
            ShellExecute(NULL, "open", "https://github.com/induktio/thinker",
                NULL, NULL, SW_SHOWNORMAL);
            return true;
        case ITEM_CLOSE:
            _wantClose = true;
            _confirmed = false;
            return true;
        }
        return true;
    }

    case VK_ESCAPE:
        _wantClose = true;
        _confirmed = false;
        return true;

    case VK_F1:
        if (ctrl) {
            sr_output(loc(SR_TMENU_HELP), true);
            return true;
        }
        return false;

    default:
        return true; // consume other keys in menu
    }
}

void RunModal() {
    _active = true;
    _wantClose = false;
    _confirmed = false;
    _mode = MODE_MAIN;
    _currentIndex = 0;

    build_menu_items();

    sr_debug_log("ThinkerMenuHandler::RunModal enter\n");
    announce_version();
    announce_menu_item();

    sr_run_modal_pump(&_wantClose);

    _active = false;

    draw_map(1);
    sr_debug_log("ThinkerMenuHandler::RunModal exit\n");
}

} // namespace ThinkerMenuHandler
