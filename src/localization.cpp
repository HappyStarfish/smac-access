/*
 * Localization system for screen reader strings.
 * Loads translations from sr_lang/<language>.txt (UTF-8).
 * Falls back to built-in English defaults if file or key is missing.
 */

#include "main.h"
#include "localization.h"
#include <stdio.h>
#include <string.h>

// Built-in English defaults (used when no language file is loaded)
static const char* sr_defaults[SR_COUNT] = {
    // General
    /* SR_MOD_LOADED       */ "Thinker Accessibility Mod loaded.",
    /* SR_DEBUG_ON          */ "Debug logging on.",
    /* SR_DEBUG_OFF         */ "Debug logging off.",
    /* SR_NO_TEXT           */ "No text captured.",

    // World Map
    /* SR_MAP_EDGE          */ "Map edge.",
    /* SR_WORLD_MAP         */ "World Map",
    /* SR_DESIGN_WORKSHOP   */ "Design Workshop",
    /* SR_YOUR_TURN         */ "Your Turn. %s at (%d, %d)",
    /* SR_UNIT_MOVES_TO     */ "%s moves to (%d, %d)",
    /* SR_TAB_FMT           */ "Tab: %s",
    /* SR_TAB_RESOURCES     */ "Resources",
    /* SR_TAB_CITIZENS      */ "Citizens",
    /* SR_TAB_PRODUCTION    */ "Production",
    /* SR_TERRAIN_OCEAN     */ "Ocean",
    /* SR_TERRAIN_SHELF     */ "Shelf",
    /* SR_TERRAIN_ROCKY     */ "Rocky",
    /* SR_TERRAIN_ROLLING   */ "Rolling",
    /* SR_TERRAIN_FLAT      */ "Flat",
    /* SR_FEATURE_FUNGUS    */ "Fungus",
    /* SR_FEATURE_FOREST    */ "Forest",
    /* SR_FEATURE_RIVER     */ "River",
    /* SR_FEATURE_FARM      */ "Farm",
    /* SR_FEATURE_MINE      */ "Mine",
    /* SR_FEATURE_SOLAR     */ "Solar",
    /* SR_FEATURE_ROAD      */ "Road",
    /* SR_FEATURE_CONDENSER */ "Condenser",
    /* SR_FEATURE_BOREHOLE  */ "Borehole",
    /* SR_FEATURE_BUNKER    */ "Bunker",
    /* SR_FEATURE_AIRBASE   */ "Airbase",
    /* SR_FEATURE_SENSOR    */ "Sensor",
    /* SR_FEATURE_SUPPLY_POD*/ "Supply Pod",
    /* SR_FEATURE_MONOLITH  */ "Monolith",
    /* SR_FEATURE_MAGTUBE   */ "Magtube",
    /* SR_FEATURE_SOIL_ENRICHER */ "Soil Enricher",
    /* SR_FEATURE_ECH_MIRROR */ "Echelon Mirror",
    /* SR_TERRAIN_TRENCH    */ "Deep Ocean",
    /* SR_TERRAIN_ARID      */ "Arid",
    /* SR_TERRAIN_MOIST     */ "Moist",
    /* SR_TERRAIN_RAINY     */ "Rainy",
    /* SR_TERRAIN_HIGH      */ "Elevated",
    /* SR_TILE_YIELDS       */ "%d food, %d minerals, %d energy",
    /* SR_TILE_OWNER        */ "Territory: %s",
    /* SR_TILE_IN_RADIUS    */ "In base radius",
    /* SR_TILE_UNOWNED      */ "Unclaimed",
    /* SR_TILE_UNEXPLORED   */ "Unexplored",

    // Scanner
    /* SR_SCAN_ALL          */ "Scanner: All",
    /* SR_SCAN_OWN_BASES    */ "Scanner: Own Bases",
    /* SR_SCAN_ENEMY_BASES  */ "Scanner: Enemy Bases",
    /* SR_SCAN_ENEMY_UNITS  */ "Scanner: Enemy Units",
    /* SR_SCAN_OWN_UNITS    */ "Scanner: Own Units",
    /* SR_SCAN_OWN_FORMERS  */ "Scanner: Own Formers",
    /* SR_SCAN_FUNGUS       */ "Scanner: Fungus",
    /* SR_SCAN_PODS         */ "Scanner: Pods and Monoliths",
    /* SR_SCAN_IMPROVEMENTS */ "Scanner: Improvements",
    /* SR_SCAN_NATURE       */ "Scanner: Terrain and Nature",
    /* SR_SCAN_NOT_FOUND    */ "Scanner: Nothing found",

    /* SR_BASE_AT           */ ". Base: %s",
    /* SR_UNIT_AT           */ ". Unit: %s",
    /* SR_MORE_UNITS        */ " (+%d more)",

    // Base Screen
    /* SR_BASE_SCREEN       */ "Base Screen",
    /* SR_SEC_OVERVIEW      */ "Overview",
    /* SR_SEC_RESOURCES     */ "Resources",
    /* SR_SEC_PRODUCTION    */ "Production",
    /* SR_SEC_ECONOMY       */ "Economy",
    /* SR_SEC_FACILITIES    */ "Facilities",
    /* SR_SEC_STATUS        */ "Status",
    /* SR_FMT_OVERVIEW      */ "Overview: %s, Population %d. Talents %d, Drones %d, Specialists %d.",
    /* SR_FMT_RESOURCES     */ "Resources: Nutrients %d surplus (%d intake), Minerals %d surplus (%d intake), Energy %d surplus (%d intake).",
    /* SR_FMT_PRODUCTION    */ "Production: %s, %d of %d minerals.",
    /* SR_FMT_QUEUE         */ " Queue:",
    /* SR_FMT_HURRY_COST    */ " Hurry cost: %d energy.",
    /* SR_FMT_ECONOMY       */ "Economy: %d economy, %d psych, %d labs.",
    /* SR_FMT_FACILITIES    */ "Facilities:",
    /* SR_FMT_FACILITIES_NONE */ "Facilities: None.",
    /* SR_FMT_STATUS        */ "Status:",
    /* SR_FMT_STATUS_NORMAL */ "Status: Normal.",
    /* SR_FMT_DRONE_RIOTS   */ " Drone riots!",
    /* SR_FMT_GOLDEN_AGE    */ " Golden age.",
    /* SR_FMT_ECO_DAMAGE    */ " Eco damage %d%%.",
    /* SR_FMT_GOVERNOR      */ " Governor active.",
    /* SR_FMT_NERVE_STAPLE  */ " Nerve staple %d turns.",
    /* SR_FMT_BASE_OPEN     */ "Base: %s, Population %d. Building %s, %d of %d.",

    // Base Screen (enhanced)
    /* SR_SEC_UNITS          */ "Units",
    /* SR_FMT_OVERVIEW_V2    */ "Overview: %s at (%d, %d), Population %d. %s. Talents %d, Drones %d.",
    /* SR_FMT_RESOURCES_V2   */ "Resources: Nutrients +%d (%d of %d, %s). Minerals +%d. Energy +%d.",
    /* SR_FMT_PRODUCTION_V2  */ "Production: %s, %d of %d minerals, %s.",
    /* SR_FMT_ECONOMY_V2     */ "Economy: %d economy, %d psych, %d labs. %s.",
    /* SR_FMT_FACILITIES_V2  */ "Facilities: %s.",
    /* SR_FMT_UNITS          */ "Units: %d stationed.",
    /* SR_FMT_UNITS_NONE     */ "Units: None stationed.",
    /* SR_FMT_BASE_OPEN_V2   */ "Base: %s, Population %d. Building %s, %d of %d, %s. Ctrl+Up/Down: Sections.",
    /* SR_FMT_WORKERS        */ "%d workers",
    /* SR_BASE_HELP          */ "Ctrl+Up/Down: Sections. Ctrl+Left/Right: Tabs. Ctrl+I: Repeat. Ctrl+H: Hurry. Ctrl+Shift+P: Change production. Escape: Close.",
    /* SR_FMT_TURNS          */ "%d turns",
    /* SR_FMT_GROWTH_NEVER   */ "no growth",
    /* SR_FMT_FACTION_CREDITS */ "%d energy credits",
    /* SR_FMT_FACILITIES_COUNT */ "%d built, %d maintenance",

    // Movement & Navigation
    /* SR_MOVEMENT_POINTS   */ "%d of %d moves",
    /* SR_CANNOT_MOVE       */ "Cannot move there.",

    // Context Help (Ctrl+F1)
    /* SR_HELP_HEADER       */ "Commands:",
    /* SR_HELP_MOVE         */ "Shift+Arrows: Move",
    /* SR_HELP_EXPLORE      */ "Arrows: Explore",
    /* SR_HELP_SKIP         */ "Space: Skip unit",
    /* SR_HELP_HOLD         */ "H: Hold",
    /* SR_HELP_READ         */ "Ctrl+R: Read screen",
    /* SR_HELP_GOTO         */ "Shift+Space: Go to cursor",
    /* SR_HELP_BUILD_BASE   */ "B: Build base",
    /* SR_HELP_BUILD_ROAD   */ "R: Build road",
    /* SR_HELP_BUILD_MAGTUBE*/ "R: Build mag tube",
    /* SR_HELP_BUILD_FARM   */ "F: Build farm",
    /* SR_HELP_BUILD_MINE   */ "M: Build mine",
    /* SR_HELP_BUILD_SOLAR  */ "S: Build solar collector",
    /* SR_HELP_BUILD_FOREST */ "Shift+F: Plant forest",
    /* SR_HELP_BUILD_SENSOR */ "O: Build sensor",
    /* SR_HELP_BUILD_CONDENSER */ "N: Build condenser",
    /* SR_HELP_BUILD_BOREHOLE  */ "Shift+N: Build borehole",
    /* SR_HELP_BUILD_AIRBASE   */ ".: Build airbase",
    /* SR_HELP_BUILD_BUNKER    */ "Shift+.: Build bunker",
    /* SR_HELP_REMOVE_FUNGUS   */ "F: Remove fungus",
    /* SR_HELP_AUTOMATE    */ "Shift+A: Automate",
    /* SR_HELP_CONVOY      */ "O: Convoy resources",
    /* SR_HELP_UNLOAD      */ "U: Unload transport",
    /* SR_HELP_OPEN_BASE   */ "Enter: Open base",
    /* SR_HELP_MONOLITH    */ "Monolith here: enter to upgrade",
    /* SR_HELP_SUPPLY_POD  */ "Supply Pod here: move onto it",
    /* SR_HELP_ATTACK      */ "Shift+Arrows: Attack adjacent",
    /* SR_HELP_PROBE       */ "Shift+Arrows: Probe adjacent",
    /* SR_HELP_AIRDROP     */ "I: Airdrop",

    // Hurry feedback
    /* SR_HURRY_OK         */ "Hurried %s. %d credits spent, %d remaining.",
    /* SR_HURRY_NO_CREDITS */ "Cannot hurry: need %d credits, have %d.",
    /* SR_HURRY_CANNOT     */ "Cannot hurry production.",

    // Production Picker
    /* SR_PROD_PICKER_OPEN   */ "Change production: %d items available. Up/Down to browse, Enter to select, Escape to cancel.",
    /* SR_PROD_PICKER_ITEM   */ "%d of %d: %s, %d minerals, %s",
    /* SR_PROD_PICKER_SELECT */ "Now building %s.",
    /* SR_PROD_PICKER_CANCEL */ "Production unchanged.",
    /* SR_PROD_PICKER_EMPTY  */ "No items available to build.",
    /* SR_PROD_PICKER_CURRENT */ "Currently building: %s, %d of %d minerals, %s.",
    /* SR_PROD_PICKER_CURRENT_NONE */ "Currently building: nothing.",
    /* SR_PROD_DETAIL_FAC    */ "%s. %s. Cost: %d minerals, Maintenance: %d per turn.",
    /* SR_PROD_DETAIL_PROJECT */ "%s. %s. Cost: %d minerals. Secret Project.",
    /* SR_PROD_DETAIL_UNIT   */ "%s. %s, %s %d, %s %d, %s, %d moves, %d HP.",

    // Menu Bar (world map)
    /* SR_MENU_GAME        */ "Game: Ctrl+S Save, Ctrl+L Load, Ctrl+Q Quit",
    /* SR_MENU_HQ          */ "HQ: E Social Engineering, U Design Workshop, F1 Datalinks",
    /* SR_MENU_NETWORK     */ "Network: Ctrl+C Chat, Ctrl+Enter End Turn",
    /* SR_MENU_MAP         */ "Map: F2 Zoom, Ctrl+Shift+M Center",
    /* SR_MENU_ACTION      */ "Action: Space Skip, H Hold, Shift+A Automate",
    /* SR_MENU_TERRAFORM   */ "Terraform: F Farm, M Mine, S Solar, R Road",
    /* SR_MENU_SCENARIO    */ "Scenario",
    /* SR_MENU_EDITMAP     */ "Edit Map",
    /* SR_MENU_HELP        */ "Help: F1 Datalinks, Shift+F1 Commands",
    /* SR_MENU_CLOSED      */ "Menu closed.",
    /* SR_MENU_ENTRY_FMT   */ "%s, %d entries",
    /* SR_MENU_ITEM_FMT    */ "%s, %s",
    /* SR_MENU_ITEM_NOHK   */ "%s",
    /* SR_MENU_NAV_FMT     */ "%d of %d: %s",
};

// Key names matching the enum order (for file parsing)
static const char* sr_keys[SR_COUNT] = {
    "mod_loaded", "debug_on", "debug_off", "no_text",
    "map_edge", "world_map", "design_workshop", "your_turn",
    "unit_moves_to", "tab_fmt", "tab_resources", "tab_citizens",
    "tab_production", "terrain_ocean", "terrain_shelf", "terrain_rocky",
    "terrain_rolling", "terrain_flat", "feature_fungus", "feature_forest",
    "feature_river", "feature_farm", "feature_mine", "feature_solar",
    "feature_road", "feature_condenser", "feature_borehole", "feature_bunker",
    "feature_airbase", "feature_sensor", "feature_supply_pod", "feature_monolith",
    "feature_magtube", "feature_soil_enricher", "feature_ech_mirror",
    "terrain_trench", "terrain_arid", "terrain_moist", "terrain_rainy",
    "terrain_high", "tile_yields", "tile_owner", "tile_in_radius",
    "tile_unowned", "tile_unexplored",
    "scan_all", "scan_own_bases", "scan_enemy_bases", "scan_enemy_units",
    "scan_own_units", "scan_own_formers", "scan_fungus", "scan_pods",
    "scan_improvements", "scan_nature", "scan_not_found",
    "base_at", "unit_at", "more_units",
    "base_screen", "sec_overview", "sec_resources", "sec_production",
    "sec_economy", "sec_facilities", "sec_status",
    "fmt_overview", "fmt_resources", "fmt_production", "fmt_queue",
    "fmt_hurry_cost", "fmt_economy", "fmt_facilities", "fmt_facilities_none",
    "fmt_status", "fmt_status_normal", "fmt_drone_riots", "fmt_golden_age",
    "fmt_eco_damage", "fmt_governor", "fmt_nerve_staple", "fmt_base_open",
    "sec_units", "fmt_overview_v2", "fmt_resources_v2", "fmt_production_v2",
    "fmt_economy_v2", "fmt_facilities_v2", "fmt_units", "fmt_units_none",
    "fmt_base_open_v2", "fmt_workers", "base_help", "fmt_turns",
    "fmt_growth_never", "fmt_faction_credits", "fmt_facilities_count",
    "movement_points", "cannot_move",
    "help_header", "help_move", "help_explore", "help_skip", "help_hold",
    "help_read", "help_goto", "help_build_base", "help_build_road",
    "help_build_magtube", "help_build_farm", "help_build_mine",
    "help_build_solar", "help_build_forest", "help_build_sensor",
    "help_build_condenser", "help_build_borehole", "help_build_airbase",
    "help_build_bunker", "help_remove_fungus", "help_automate",
    "help_convoy", "help_unload", "help_open_base", "help_monolith",
    "help_supply_pod", "help_attack", "help_probe", "help_airdrop",
    "hurry_ok", "hurry_no_credits", "hurry_cannot",
    "prod_picker_open", "prod_picker_item", "prod_picker_select",
    "prod_picker_cancel", "prod_picker_empty",
    "prod_picker_current", "prod_picker_current_none",
    "prod_detail_fac", "prod_detail_project", "prod_detail_unit",
    "menu_game", "menu_hq", "menu_network", "menu_map", "menu_action",
    "menu_terraform", "menu_scenario", "menu_editmap", "menu_help",
    "menu_closed", "menu_entry_fmt", "menu_item_fmt", "menu_item_nohk",
    "menu_nav_fmt",
};

// Loaded strings (dynamically allocated, or NULL = use default)
static char* sr_loaded[SR_COUNT] = {};

// Find enum index by key name, returns -1 if not found
static int loc_find_key(const char* key) {
    for (int i = 0; i < SR_COUNT; i++) {
        if (strcmp(sr_keys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

// Strip leading/trailing whitespace in-place, return pointer to trimmed start
static char* loc_trim(char* s) {
    while (*s == ' ' || *s == '\t') s++;
    int len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t'
           || s[len-1] == '\r' || s[len-1] == '\n')) {
        s[--len] = '\0';
    }
    return s;
}

// Strip UTF-8 BOM if present at start of buffer
static char* loc_skip_bom(char* s) {
    if ((unsigned char)s[0] == 0xEF
        && (unsigned char)s[1] == 0xBB
        && (unsigned char)s[2] == 0xBF) {
        return s + 3;
    }
    return s;
}

/*
Load a language file (sr_lang/<lang>.txt) and populate sr_loaded[].
File format: KEY=Value (one per line), # for comments, blank lines ignored.
*/
static void loc_load_file(const char* lang) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "sr_lang/%s.txt", lang);

    FILE* f = fopen(path, "rb");
    if (!f) {
        debug("loc_load_file: %s not found, using defaults\n", path);
        return;
    }

    char line[1024];
    int loaded = 0;
    bool first_line = true;

    while (fgets(line, sizeof(line), f)) {
        char* s = line;
        // Strip BOM on first line
        if (first_line) {
            s = loc_skip_bom(s);
            first_line = false;
        }
        s = loc_trim(s);

        // Skip empty lines and comments
        if (s[0] == '\0' || s[0] == '#') continue;

        // Find '=' separator
        char* eq = strchr(s, '=');
        if (!eq) continue;

        *eq = '\0';
        char* key = loc_trim(s);
        char* val = loc_trim(eq + 1);

        int idx = loc_find_key(key);
        if (idx < 0) {
            debug("loc_load_file: unknown key '%s' in %s\n", key, path);
            continue;
        }

        // Free previous value if any
        if (sr_loaded[idx]) {
            free(sr_loaded[idx]);
        }
        sr_loaded[idx] = strdup(val);
        loaded++;
    }

    fclose(f);
    debug("loc_load_file: loaded %d strings from %s\n", loaded, path);
}

void loc_init() {
    // Read language setting from thinker.ini
    char lang[32] = "en";
    char val[32];
    if (GetPrivateProfileStringA("thinker", "sr_language", "en",
            val, sizeof(val), ".\\thinker.ini")) {
        // Trim and validate
        char* trimmed = loc_trim(val);
        if (trimmed[0] && strlen(trimmed) < sizeof(lang)) {
            strncpy(lang, trimmed, sizeof(lang) - 1);
            lang[sizeof(lang) - 1] = '\0';
        }
    }

    debug("loc_init: language=%s\n", lang);
    loc_load_file(lang);
}

const char* loc(SrStr id) {
    if (id < 0 || id >= SR_COUNT) {
        return "???";
    }
    if (sr_loaded[id]) {
        return sr_loaded[id];
    }
    return sr_defaults[id] ? sr_defaults[id] : "???";
}
