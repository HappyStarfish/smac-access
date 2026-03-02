/*
 * GameSettingsHandler - Accessible singleplayer settings editor.
 *
 * Ctrl+F10 in main menu opens a modal dialog with 4 categories:
 *   1. General (difficulty)
 *   2. Faction (player faction)
 *   3. Map (size, ocean, land, erosion, clouds, natives)
 *   4. Rules (15 toggleable flags)
 *
 * On save (Enter), writes to Alpha Centauri.ini via prefs_save().
 * On cancel (Escape), discards all changes.
 */

#include "game_settings_handler.h"
#include "main.h"
#include "engine.h"
#include "gui.h"
#include "modal_utils.h"
#include "screen_reader.h"
#include "localization.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

namespace GameSettingsHandler {

// --- Constants ---

static const int NUM_CATEGORIES = 4;
static const int CAT_GENERAL = 0;
static const int CAT_FACTION = 1;
static const int CAT_MAP = 2;
static const int CAT_RULES = 3;

static const int NUM_DIFFICULTIES = 6;
static const int NUM_FACTIONS = 14;
static const int NUM_MAP_SETTINGS = 6;
static const int NUM_RULES = 15;

// Faction filenames (SMACX order: 7 SMAC + 7 Alien Crossfire)
static const char* FactionFiles[NUM_FACTIONS] = {
    "GAIANS", "HIVE", "UNIV", "MORGAN", "SPARTANS", "BELIEVE", "PEACE",
    "CYBORG", "PIRATES", "DRONE", "ANGELS", "FUNGBOY", "CARETAKE", "USURPER"
};

// Map setting value ranges
// custom_world[0] = size (0-4), [1] = ocean (0-2), [2] = land (0-2),
// [3] = erosion (0-2), [4] = orbit (unused here), [5] = clouds (0-2), [6] = natives (0-2)
static const int MapMaxValues[] = { 4, 2, 2, 2, 2, 2 };
// Which custom_world index each map setting maps to
static const int MapWorldIndex[] = { 0, 1, 2, 3, 5, 6 };

// Rule flags in display order (5 victory + 10 game rules)
static const int RuleFlags[NUM_RULES] = {
    RULES_VICTORY_CONQUEST, RULES_VICTORY_ECONOMIC, RULES_VICTORY_DIPLOMATIC,
    RULES_VICTORY_TRANSCENDENCE, RULES_VICTORY_COOPERATIVE,
    RULES_DO_OR_DIE, RULES_LOOK_FIRST, RULES_TECH_STAGNATION,
    RULES_INTENSE_RIVALRY, RULES_TIME_WARP, RULES_NO_UNITY_SURVEY,
    RULES_BLIND_RESEARCH, RULES_NO_UNITY_SCATTERING, RULES_SPOILS_OF_WAR,
    RULES_BELL_CURVE
};

// Localization keys for rule names
static const SrStr RuleNames[NUM_RULES] = {
    SR_GSETTINGS_RULE_CONQUEST, SR_GSETTINGS_RULE_ECONOMIC,
    SR_GSETTINGS_RULE_DIPLOMATIC, SR_GSETTINGS_RULE_TRANSCENDENCE,
    SR_GSETTINGS_RULE_COOPERATIVE,
    SR_GSETTINGS_RULE_DO_OR_DIE, SR_GSETTINGS_RULE_LOOK_FIRST,
    SR_GSETTINGS_RULE_TECH_STAGNATION, SR_GSETTINGS_RULE_INTENSE_RIVALRY,
    SR_GSETTINGS_RULE_TIME_WARP, SR_GSETTINGS_RULE_NO_UNITY_SURVEY,
    SR_GSETTINGS_RULE_BLIND_RESEARCH, SR_GSETTINGS_RULE_NO_UNITY_SCATTER,
    SR_GSETTINGS_RULE_SPOILS_OF_WAR, SR_GSETTINGS_RULE_BELL_CURVE
};

// Localization keys for rule descriptions
static const SrStr RuleDescs[NUM_RULES] = {
    SR_GSETTINGS_RULE_DESC_CONQUEST, SR_GSETTINGS_RULE_DESC_ECONOMIC,
    SR_GSETTINGS_RULE_DESC_DIPLOMATIC, SR_GSETTINGS_RULE_DESC_TRANSCENDENCE,
    SR_GSETTINGS_RULE_DESC_COOPERATIVE,
    SR_GSETTINGS_RULE_DESC_DO_OR_DIE, SR_GSETTINGS_RULE_DESC_LOOK_FIRST,
    SR_GSETTINGS_RULE_DESC_TECH_STAGNATION, SR_GSETTINGS_RULE_DESC_INTENSE_RIVALRY,
    SR_GSETTINGS_RULE_DESC_TIME_WARP, SR_GSETTINGS_RULE_DESC_NO_UNITY_SURVEY,
    SR_GSETTINGS_RULE_DESC_BLIND_RESEARCH, SR_GSETTINGS_RULE_DESC_NO_UNITY_SCATTER,
    SR_GSETTINGS_RULE_DESC_SPOILS_OF_WAR, SR_GSETTINGS_RULE_DESC_BELL_CURVE
};

// Difficulty name localization keys (reuse from netsetup)
static const SrStr DifficultyNames[NUM_DIFFICULTIES] = {
    SR_NETSETUP_DIFF_CITIZEN, SR_NETSETUP_DIFF_SPECIALIST,
    SR_NETSETUP_DIFF_TALENT, SR_NETSETUP_DIFF_LIBRARIAN,
    SR_NETSETUP_DIFF_THINKER, SR_NETSETUP_DIFF_TRANSCEND
};

// --- State ---

static bool _active = false;
static bool _wantClose = false;
static bool _confirmed = false;
static int _currentCat = 0;
static int _currentItem = 0;

// Editable values
static int _difficulty = 0;
static int _factionIndex = 0;
static int _customWorld[7] = {};
static int _rules = 0;

// --- Helpers ---

/// Get number of items in the current category.
static int category_count() {
    switch (_currentCat) {
        case CAT_GENERAL: return 1;
        case CAT_FACTION: return 1;
        case CAT_MAP:     return NUM_MAP_SETTINGS;
        case CAT_RULES:   return NUM_RULES;
        default:          return 0;
    }
}

/// Get category name.
static const char* category_name() {
    static const SrStr names[] = {
        SR_GSETTINGS_CAT_GENERAL, SR_GSETTINGS_CAT_FACTION,
        SR_GSETTINGS_CAT_MAP, SR_GSETTINGS_CAT_RULES
    };
    if (_currentCat >= 0 && _currentCat < NUM_CATEGORIES) {
        return loc(names[_currentCat]);
    }
    return "";
}

/// Read faction name from the faction .txt file in game directory.
static bool read_faction_name(const char* filename, char* name, int namesize) {
    if (!filename || !filename[0]) return false;

    char path[MAX_PATH];
    char exepath[MAX_PATH];
    GetModuleFileNameA(NULL, exepath, MAX_PATH);
    char* last_slash = strrchr(exepath, '\\');
    if (last_slash) *(last_slash + 1) = '\0';

    snprintf(path, sizeof(path), "%s%s.txt", exepath, filename);

    FILE* f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    bool found_section = false;
    char section_tag[64];
    snprintf(section_tag, sizeof(section_tag), "#%s", filename);

    while (fgets(line, sizeof(line), f)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (_strnicmp(line, section_tag, strlen(section_tag)) == 0) {
            found_section = true;
            continue;
        }
        if (found_section && line[0] != ';' && line[0] != '\0') {
            char* comma = strchr(line, ',');
            if (comma) {
                int nlen = comma - line;
                if (nlen >= namesize) nlen = namesize - 1;
                strncpy(name, line, nlen);
                name[nlen] = '\0';
                while (nlen > 0 && name[nlen-1] == ' ') name[--nlen] = '\0';
            } else {
                strncpy(name, line, namesize - 1);
                name[namesize - 1] = '\0';
            }
            fclose(f);
            char utf8[256];
            sr_ansi_to_utf8(name, utf8, sizeof(utf8));
            strncpy(name, utf8, namesize - 1);
            name[namesize - 1] = '\0';
            return true;
        }
    }
    fclose(f);
    return false;
}

/// Read a named section from a faction .txt file.
/// section_name: e.g. "#BLURB" or "#DATALINKS1"
static bool read_faction_section(const char* filename, const char* section_name,
    char* buf, int bufsize)
{
    if (!filename || !filename[0]) return false;

    char path[MAX_PATH];
    char exepath[MAX_PATH];
    GetModuleFileNameA(NULL, exepath, MAX_PATH);
    char* last_slash = strrchr(exepath, '\\');
    if (last_slash) *(last_slash + 1) = '\0';

    snprintf(path, sizeof(path), "%s%s.txt", exepath, filename);

    FILE* f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    bool found = false;
    buf[0] = '\0';
    int pos = 0;

    while (fgets(line, sizeof(line), f)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (!found) {
            if (_strnicmp(line, section_name, strlen(section_name)) == 0) {
                found = true;
            }
            continue;
        }

        // End of section: next # marker or end of file
        if (line[0] == '#') break;
        // Skip empty lines and comments
        if (line[0] == ';') continue;
        if (line[0] == '\0') continue;

        // Append line to buffer
        if (pos > 0 && pos < bufsize - 2) {
            buf[pos++] = ' ';
        }
        int remaining = bufsize - pos - 1;
        if (remaining > 0) {
            int copy = (len < remaining) ? len : remaining;
            strncpy(buf + pos, line, copy);
            pos += copy;
        }
    }
    buf[pos] = '\0';
    fclose(f);

    if (found && pos > 0) {
        char utf8[2048];
        sr_ansi_to_utf8(buf, utf8, sizeof(utf8));
        strncpy(buf, utf8, bufsize - 1);
        buf[bufsize - 1] = '\0';
        return true;
    }
    return false;
}

/// Get the display name for the current faction.
static const char* current_faction_name() {
    static char name[128];
    if (read_faction_name(FactionFiles[_factionIndex], name, sizeof(name))) {
        return name;
    }
    return FactionFiles[_factionIndex];
}

/// Get difficulty name string.
static const char* difficulty_name(int diff) {
    if (diff >= 0 && diff < NUM_DIFFICULTIES) {
        return loc(DifficultyNames[diff]);
    }
    return "?";
}

/// Get map size name (reuse netsetup strings).
static const char* map_size_name(int val) {
    static const SrStr sizes[] = {
        SR_NETSETUP_SIZE_TINY, SR_NETSETUP_SIZE_SMALL,
        SR_NETSETUP_SIZE_STANDARD, SR_NETSETUP_SIZE_LARGE,
        SR_NETSETUP_SIZE_HUGE
    };
    if (val >= 0 && val <= 4) return loc(sizes[val]);
    return "?";
}

/// Get map setting value name for the given map item index.
static const char* map_value_name(int item, int val) {
    switch (item) {
        case 0: return map_size_name(val); // size
        case 1: { // ocean
            static const SrStr ocean[] = {
                SR_NETSETUP_OCEAN_30, SR_NETSETUP_OCEAN_50, SR_NETSETUP_OCEAN_70
            };
            if (val >= 0 && val <= 2) return loc(ocean[val]);
            break;
        }
        case 2: { // land
            static const SrStr land[] = {
                SR_GSETTINGS_LAND_SPARSE, SR_GSETTINGS_LAND_AVG, SR_GSETTINGS_LAND_DENSE
            };
            if (val >= 0 && val <= 2) return loc(land[val]);
            break;
        }
        case 3: { // erosion
            static const SrStr erosion[] = {
                SR_NETSETUP_EROSION_STRONG, SR_NETSETUP_EROSION_AVG, SR_NETSETUP_EROSION_WEAK
            };
            if (val >= 0 && val <= 2) return loc(erosion[val]);
            break;
        }
        case 4: { // clouds
            static const SrStr clouds[] = {
                SR_NETSETUP_CLOUD_SPARSE, SR_NETSETUP_CLOUD_AVG, SR_NETSETUP_CLOUD_DENSE
            };
            if (val >= 0 && val <= 2) return loc(clouds[val]);
            break;
        }
        case 5: { // natives
            static const SrStr natives[] = {
                SR_NETSETUP_NATIVE_RARE, SR_NETSETUP_NATIVE_AVG, SR_NETSETUP_NATIVE_ABUND
            };
            if (val >= 0 && val <= 2) return loc(natives[val]);
            break;
        }
    }
    return "?";
}

/// Get map setting label for the given map item index.
static const char* map_setting_name(int item) {
    static const SrStr names[] = {
        SR_GSETTINGS_MAP_SIZE, SR_GSETTINGS_OCEAN, SR_GSETTINGS_LAND,
        SR_GSETTINGS_EROSION, SR_GSETTINGS_CLOUDS, SR_GSETTINGS_NATIVES
    };
    if (item >= 0 && item < NUM_MAP_SETTINGS) return loc(names[item]);
    return "?";
}

/// Count how many rules are currently enabled.
static int count_active_rules() {
    int count = 0;
    for (int i = 0; i < NUM_RULES; i++) {
        if (_rules & RuleFlags[i]) count++;
    }
    return count;
}

// --- Announcements ---

static void announce_category() {
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_GSETTINGS_CAT_FMT),
        _currentCat + 1, NUM_CATEGORIES, category_name(), category_count());
    sr_output(buf, true);
}

static void announce_item() {
    char buf[512];
    int count = category_count();

    switch (_currentCat) {
        case CAT_GENERAL:
            snprintf(buf, sizeof(buf), loc(SR_GSETTINGS_ITEM_FMT),
                1, 1, loc(SR_GSETTINGS_DIFFICULTY), difficulty_name(_difficulty));
            break;
        case CAT_FACTION: {
            snprintf(buf, sizeof(buf), loc(SR_GSETTINGS_ITEM_FMT),
                1, 1, loc(SR_GSETTINGS_FACTION), current_faction_name());
            break;
        }
        case CAT_MAP: {
            int wi = MapWorldIndex[_currentItem];
            snprintf(buf, sizeof(buf), loc(SR_GSETTINGS_ITEM_FMT),
                _currentItem + 1, count,
                map_setting_name(_currentItem),
                map_value_name(_currentItem, _customWorld[wi]));
            break;
        }
        case CAT_RULES: {
            bool on = (_rules & RuleFlags[_currentItem]) != 0;
            snprintf(buf, sizeof(buf), loc(SR_GSETTINGS_TOGGLE_FMT),
                _currentItem + 1, count,
                loc(RuleNames[_currentItem]),
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
    snprintf(buf, sizeof(buf), loc(SR_GSETTINGS_SUMMARY_FMT),
        difficulty_name(_difficulty),
        current_faction_name(),
        map_size_name(_customWorld[0]),
        count_active_rules());
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
    switch (_currentCat) {
        case CAT_GENERAL:
            _difficulty = (_difficulty + dir + NUM_DIFFICULTIES) % NUM_DIFFICULTIES;
            break;
        case CAT_FACTION:
            _factionIndex = (_factionIndex + dir + NUM_FACTIONS) % NUM_FACTIONS;
            break;
        case CAT_MAP: {
            int wi = MapWorldIndex[_currentItem];
            int maxVal = MapMaxValues[_currentItem];
            _customWorld[wi] = (_customWorld[wi] + dir + maxVal + 1) % (maxVal + 1);
            break;
        }
        case CAT_RULES:
            // Toggle
            _rules ^= RuleFlags[_currentItem];
            break;
        default:
            return;
    }
    announce_item();
}

static void toggle_rule() {
    if (_currentCat == CAT_RULES) {
        _rules ^= RuleFlags[_currentItem];
        announce_item();
    }
}

static void read_description() {
    char buf[2048];

    if (_currentCat == CAT_FACTION) {
        // Read BLURB from faction file
        char section[16];
        snprintf(section, sizeof(section), "#BLURB");
        if (read_faction_section(FactionFiles[_factionIndex], section, buf, sizeof(buf))) {
            sr_output(buf, true);
        } else {
            sr_output(loc(SR_GSETTINGS_NO_DESC), true);
        }
    } else if (_currentCat == CAT_RULES) {
        sr_output(loc(RuleDescs[_currentItem]), true);
    } else {
        sr_output(loc(SR_GSETTINGS_NO_DESC), true);
    }
}

static void read_faction_info() {
    if (_currentCat != CAT_FACTION) return;

    char buf[2048];
    if (read_faction_section(FactionFiles[_factionIndex], "#DATALINKS1", buf, sizeof(buf))) {
        sr_output(buf, true);
    } else {
        sr_output(loc(SR_GSETTINGS_NO_DESC), true);
    }
}

/// Find which faction index matches a filename, or 0 if not found.
static int find_faction_index(const char* filename) {
    for (int i = 0; i < NUM_FACTIONS; i++) {
        if (_strnicmp(FactionFiles[i], filename, 23) == 0) {
            return i;
        }
    }
    return 0;
}

// --- Save ---

static void save_settings() {
    // Set difficulty
    DefaultPrefs->difficulty = _difficulty;

    // Set custom world map
    AlphaIniPrefs->customize = 2; // "Custom" map type
    DefaultPrefs->map_type = 2;
    for (int i = 0; i < 7; i++) {
        AlphaIniPrefs->custom_world[i] = _customWorld[i];
    }

    // Set rules (entire bitfield)
    AlphaIniPrefs->rules = _rules;

    // Save everything to INI
    prefs_save(0);

    // Save faction separately
    char buf[256];
    prefs_put2("Faction 1", FactionFiles[_factionIndex]);

    sr_debug_log("GameSettings: saved diff=%d faction=%s rules=0x%X\n",
        _difficulty, FactionFiles[_factionIndex], _rules);
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
            if (_currentCat == CAT_RULES) {
                // Enter on a rule = toggle
                toggle_rule();
            } else {
                // Enter elsewhere = save and close
                _confirmed = true;
                _wantClose = true;
            }
            break;

        case VK_SPACE:
            if (_currentCat == CAT_RULES) {
                toggle_rule();
            }
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
                sr_output(loc(SR_GSETTINGS_HELP), true);
            }
            break;

        case 'D':
            if (!ctrl && !shift) {
                read_description();
            }
            break;

        case 'I':
            if (!ctrl && !shift) {
                read_faction_info();
            }
            break;

        case 'S':
            if (!ctrl && !shift) {
                announce_summary();
            }
            break;
    }
}

void RunModal() {
    // Load current values from game state
    _difficulty = DefaultPrefs->difficulty;
    if (_difficulty < 0 || _difficulty >= NUM_DIFFICULTIES) _difficulty = 0;

    // Load faction: read "Faction 1" from INI
    char faction_buf[64] = {};
    GetPrivateProfileStringA("Alpha Centauri", "Faction 1", "GAIANS",
        faction_buf, sizeof(faction_buf), ".\\Alpha Centauri.ini");
    _factionIndex = find_faction_index(faction_buf);

    // Load custom world settings
    for (int i = 0; i < 7; i++) {
        _customWorld[i] = AlphaIniPrefs->custom_world[i];
    }
    // Clamp values
    if (_customWorld[0] < 0 || _customWorld[0] > 4) _customWorld[0] = 2;
    for (int i = 1; i < 7; i++) {
        if (_customWorld[i] < 0 || _customWorld[i] > 2) _customWorld[i] = 1;
    }

    // Load rules
    _rules = AlphaIniPrefs->rules;

    // Set state
    _active = true;
    _wantClose = false;
    _confirmed = false;
    _currentCat = 0;
    _currentItem = 0;

    sr_debug_log("GameSettingsHandler::RunModal enter\n");
    sr_output(loc(SR_GSETTINGS_OPEN), true);

    // Run modal pump
    sr_run_modal_pump(&_wantClose);

    // Apply or discard
    if (_confirmed) {
        save_settings();
        sr_output(loc(SR_GSETTINGS_SAVED), true);
        sr_debug_log("GameSettingsHandler::RunModal confirm\n");
    } else {
        sr_output(loc(SR_GSETTINGS_CANCELLED), true);
        sr_debug_log("GameSettingsHandler::RunModal cancel\n");
    }

    _active = false;
}

} // namespace GameSettingsHandler
