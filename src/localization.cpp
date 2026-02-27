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
    /* SR_FMT_BASE_OPEN_V2   */ "Base: %s, Population %d. Building %s, %d of %d, %s. Ctrl+F1: Help.",
    /* SR_FMT_WORKERS        */ "%d workers",
    /* SR_BASE_HELP          */ "Left/Right: Previous/Next base. Ctrl+Up/Down: Sections. Ctrl+Left/Right: Tabs. Ctrl+I: Repeat. Ctrl+H: Hurry. Ctrl+Shift+P: Change production. Ctrl+Q: Queue. Ctrl+D: Demolish. Ctrl+U: Garrison. Ctrl+W: Citizens. Ctrl+G: Governor. Ctrl+N: Nerve staple. F2: Rename. Ctrl+F1: Help. Escape: Close.",
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
    /* SR_HELP_READ         */ "Ctrl+R: Road to cursor, Ctrl+T: Tube to cursor",
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
    /* SR_UNIT_ABILITIES     */ " Abilities:",

    // Queue Management
    /* SR_QUEUE_OPEN                 */ "Queue: %d items. Up/Down navigate, Shift+Up/Down reorder, Delete remove, Insert add, Escape close.",
    /* SR_QUEUE_OPEN_ONE             */ "Queue: 1 item. %s. Insert to add, Escape close.",
    /* SR_QUEUE_ITEM                 */ "%d of %d: %s, %d minerals, %s.",
    /* SR_QUEUE_ITEM_CURRENT         */ "%d of %d: %s, %d of %d minerals, %s. Current production.",
    /* SR_QUEUE_MOVED                */ "%s moved to position %d.",
    /* SR_QUEUE_REMOVED              */ "Removed %s. %d items remaining.",
    /* SR_QUEUE_ADDED                */ "Added %s at position %d.",
    /* SR_QUEUE_FULL                 */ "Cannot: queue full, 10 items.",
    /* SR_QUEUE_CANNOT_MOVE_CURRENT  */ "Cannot move current production.",
    /* SR_QUEUE_CANNOT_REMOVE_CURRENT */ "Cannot remove current production. Use Ctrl+Shift+P to change.",
    /* SR_QUEUE_CLOSED               */ "Queue closed.",
    /* SR_QUEUE_EMPTY                */ "Queue empty.",

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
    // Popup Menus
    /* SR_MENU_MAIN         */ "Main Menu",
    /* SR_MENU_MAP_MENU     */ "Map Menu",
    /* SR_MENU_MULTIPLAYER  */ "Multiplayer",
    /* SR_MENU_SCENARIO_MENU*/ "Scenario",
    /* SR_MENU_THINKER      */ "Thinker Menu",
    /* SR_MENU_GAME_MENU    */ "Game Menu",
    /* SR_MENU_FILE_SELECT  */ "File Selection",
    /* SR_FILE_LOAD_GAME   */ "Load Game",
    /* SR_FILE_SAVE_GAME   */ "Save Game",
    /* SR_FILE_FOLDER      */ "Folder",
    /* SR_FILE_ITEM_FMT    */ "%d of %d: %s, %s",
    /* SR_FILE_PARENT_DIR  */ "Parent directory",
    /* SR_FILE_OVERWRITE_HINT */ "Enter to confirm, Escape to cancel.",

    /* SR_POPUP_LIST_FMT   */ "%d of %d: %s",
    /* SR_POPUP_CONTINUE   */ "Enter to continue.",

    // Social Engineering
    /* SR_SOCENG_TITLE                 */ "Social Engineering",
    /* SR_SOCENG_CATEGORY_FMT          */ "Category %d of 4: %s. Current: %s",
    /* SR_SOCENG_MODEL_FMT             */ "%d of %d: %s",
    /* SR_SOCENG_EFFECTS               */ "Effects: %s",
    /* SR_SOCENG_NO_EFFECT             */ "No effects",
    /* SR_SOCENG_UNAVAILABLE_TECH      */ "Not available, requires %s",
    /* SR_SOCENG_UNAVAILABLE_OPPOSITION */ "Not available, faction opposition",
    /* SR_SOCENG_SELECTED              */ "Selected: %s",
    /* SR_SOCENG_CURRENT               */ "Current selection",
    /* SR_SOCENG_PENDING               */ "Pending change to %s",
    /* SR_SOCENG_SUMMARY_FMT           */ "%s: %s",
    /* SR_SOCENG_UPHEAVAL_COST         */ "Upheaval cost: %d energy credits",
    /* SR_SOCENG_HELP                  */ "Up/Down: Categories. Left/Right: Models. G: Total effects. W: Energy allocation mode. I: Research info. S or Tab: Summary. Ctrl+I: Repeat. Enter: Confirm. Escape: Cancel.",
    /* SR_SOCENG_CLOSED                */ "Social Engineering closed.",

    // Preferences Handler
    /* SR_PREF_TITLE                   */ "Preferences",
    /* SR_PREF_TAB_FMT                 */ "Preferences, Tab: %s, %d options",
    /* SR_PREF_OPTION_FMT              */ "%d of %d: %s, %s",
    /* SR_PREF_TOGGLED_FMT             */ "%s, %s",
    /* SR_PREF_SAVED                   */ "Preferences saved.",
    /* SR_PREF_CANCELLED               */ "Preferences cancelled.",
    /* SR_PREF_HELP                    */ "Left/Right: Tabs. Up/Down: Options. Space: Toggle. S or Tab: Summary. Ctrl+I: Repeat. Ctrl+F1: Help. Enter: Save. Escape: Cancel.",
    /* SR_PREF_SUMMARY_FMT             */ "%s: %s",
    /* SR_PREF_ENABLED                 */ "enabled",
    /* SR_PREF_DISABLED                */ "disabled",

    // Preferences: Tab names
    /* SR_PREF_TAB_GENERAL             */ "General",
    /* SR_PREF_TAB_WARNINGS            */ "Warnings",
    /* SR_PREF_TAB_ADVANCED            */ "Advanced",
    /* SR_PREF_TAB_AUTOMATION          */ "Automation",
    /* SR_PREF_TAB_AUDIO_VISUAL        */ "Audio and Visual",
    /* SR_PREF_TAB_MAP_DISPLAY         */ "Map Display",

    // Preferences: General tab
    /* SR_PREF_PAUSE_END_TURN          */ "Pause at end of turn",
    /* SR_PREF_AUTOSAVE                */ "Autosave each turn",
    /* SR_PREF_QUICK_ENEMY             */ "Quick move enemy vehicles",
    /* SR_PREF_TUTORIAL                */ "Tutorial messages",
    /* SR_PREF_MOUSE_EDGE_SCROLL       */ "Mouse edge scroll",
    /* SR_PREF_AUTO_DESIGN             */ "Auto design vehicles",
    /* SR_PREF_QUICK_ALLY              */ "Quick move ally vehicles",
    /* SR_PREF_AUTO_PRUNE              */ "Auto prune obsolete vehicles",

    // Preferences: Warnings tab
    /* SR_PREF_WARN_FAC_BUILT          */ "Facility built",
    /* SR_PREF_WARN_NON_COMBAT_BUILT   */ "Non-combat unit built",
    /* SR_PREF_WARN_PROTOTYPE          */ "Prototype complete",
    /* SR_PREF_WARN_DRONE_RIOTS        */ "Drone riots",
    /* SR_PREF_WARN_DRONE_RIOTS_END    */ "Drone riots end",
    /* SR_PREF_WARN_GOLDEN_AGE         */ "Golden age",
    /* SR_PREF_WARN_GOLDEN_AGE_END     */ "Golden age end",
    /* SR_PREF_WARN_NUTRIENT_SHORTAGE  */ "Nutrient shortage",
    /* SR_PREF_WARN_BUILD_OUT_OF_DATE  */ "Build out of date",
    /* SR_PREF_WARN_COMBAT_BUILT       */ "Combat unit built",
    /* SR_PREF_WARN_POP_LIMIT          */ "Population limit reached",
    /* SR_PREF_WARN_DELAY_TRANSCEND    */ "Delay in transcendence",
    /* SR_PREF_WARN_BUILT_VIA_QUEUE    */ "Built via governor queue",
    /* SR_PREF_WARN_STARVATION         */ "Starvation",
    /* SR_PREF_WARN_MINERAL_SHORTAGE   */ "Mineral shortage",
    /* SR_PREF_WARN_ENERGY_SHORTAGE    */ "Energy shortage",
    /* SR_PREF_WARN_RANDOM_EVENT       */ "Random event",

    // Preferences: Advanced tab
    /* SR_PREF_FAST_BATTLE             */ "Fast battle resolution",
    /* SR_PREF_NO_CENTER_ORDERS        */ "Don't center on unit with orders",
    /* SR_PREF_PAUSE_AFTER_BATTLE      */ "Pause after battles",
    /* SR_PREF_ZOOM_NO_RECENTER        */ "Zoom base no recenter",
    /* SR_PREF_QUICK_MOVE_ORDERS       */ "Quick move vehicle orders",
    /* SR_PREF_QUICK_MOVE_ALL          */ "Quick move all vehicles",
    /* SR_PREF_RIGHT_CLICK_MENU        */ "Right-click popup menu",
    /* SR_PREF_DETAIL_RIGHT_CLICK      */ "Detail right-click menus",
    /* SR_PREF_CONFIRM_ODDS            */ "Confirm odds before attacking",
    /* SR_PREF_DETAIL_MAIN_MENUS       */ "Detail main menus",
    /* SR_PREF_RADIO_SINGLE_CLICK      */ "Radio button single click",

    // Preferences: Automation tab
    /* SR_PREF_FORMER_RAISE_LOWER      */ "Former: raise and lower terrain",
    /* SR_PREF_FORMER_PLANT_FOREST     */ "Former: plant forests",
    /* SR_PREF_FORMER_BUILD_ADV        */ "Former: build advanced",
    /* SR_PREF_FORMER_REMOVE_FUNGUS    */ "Former: remove fungus",
    /* SR_PREF_FORMER_BUILD_SENSORS    */ "Former: build sensors",
    /* SR_PREF_FORMER_BUILD_ROADS      */ "Former: build roads and tubes",
    /* SR_PREF_END_MOVE_PACT           */ "End move: spot pact vehicle",
    /* SR_PREF_END_MOVE_TREATY         */ "End move: spot treaty vehicle",
    /* SR_PREF_END_MOVE_TRUCE          */ "End move: spot truce vehicle",
    /* SR_PREF_END_MOVE_WAR            */ "End move: spot war vehicle",
    /* SR_PREF_END_MOVE_DIFF_TRIAD     */ "End move: different triad",
    /* SR_PREF_AIR_RETURN_HOME         */ "Air vehicle return home",
    /* SR_PREF_WAKE_ON_LAND            */ "Wake vehicle on land",
    /* SR_PREF_ALWAYS_INSPECT_MONOLITH */ "Always inspect monolith",

    // Preferences: Audio/Visual tab
    /* SR_PREF_SOUND_EFFECTS           */ "Sound effects",
    /* SR_PREF_BACKGROUND_MUSIC        */ "Background music",
    /* SR_PREF_MAP_ANIMATIONS          */ "Map animations",
    /* SR_PREF_SLIDING_WINDOWS         */ "Sliding windows",
    /* SR_PREF_PROJECT_MOVIES          */ "Secret project movies",
    /* SR_PREF_INTERLUDES              */ "Interludes",
    /* SR_PREF_VOICEOVER               */ "Voice over",
    /* SR_PREF_STOP_VOICE_CLOSE        */ "Stop voice on close",
    /* SR_PREF_SLIDING_SCROLLBARS      */ "Sliding scrollbars",
    /* SR_PREF_WHOLE_VEH_BLINKS        */ "Whole vehicle blinks",
    /* SR_PREF_MONUMENTS               */ "Monuments",

    // Preferences: Map Display tab
    /* SR_PREF_SHOW_GRID               */ "Show grid",
    /* SR_PREF_SHOW_BASE_GRID          */ "Show base grid",
    /* SR_PREF_FOG_OF_WAR              */ "Fog of war",
    /* SR_PREF_BASE_NAMES              */ "Base names",
    /* SR_PREF_PROD_WITH_NAMES         */ "Production with base names",
    /* SR_PREF_FLAT_TERRAIN            */ "Flat terrain",
    /* SR_PREF_GRID_OCEAN              */ "Grid ocean squares",
    /* SR_PREF_SHOW_GOTO_PATH          */ "Show goto path",

    // Specialist Management (Ctrl+W)
    /* SR_SPEC_OPEN           */ "Citizens: %s",
    /* SR_SPEC_CLOSE          */ "Citizens closed.",
    /* SR_SPEC_WORKER         */ "%d of %d: Worker. %s",
    /* SR_SPEC_SPECIALIST     */ "%d of %d: %s. %s",
    /* SR_SPEC_BONUS          */ "+%d Economy, +%d Psych, +%d Labs",
    /* SR_SPEC_TO_SPECIALIST  */ "Converted to %s.",
    /* SR_SPEC_TO_WORKER      */ "Converted to Worker.",
    /* SR_SPEC_TYPE_CHANGED   */ "Changed to %s.",
    /* SR_SPEC_CANNOT_MORE    */ "Cannot: base too small for more specialists.",
    /* SR_SPEC_CANNOT_LESS    */ "Cannot: no specialists to convert.",
    /* SR_SPEC_CANNOT_CENTER  */ "Cannot: base center always worked.",
    /* SR_SPEC_HELP           */ "Up/Down: Navigate citizens. Enter: Toggle worker/specialist. Left/Right: Change specialist type. S: Summary. Escape: Close.",
    /* SR_SPEC_SUMMARY        */ "%d workers, %d specialists.",
    /* SR_SPEC_CENTER         */ "Base center.",
    /* SR_SPEC_NO_OTHER_TYPE  */ "No other specialist types available.",
    /* SR_SPEC_WORKER_NO_TYPE */ "Workers have no type. Use Enter to convert.",

    // Facility Demolition (Ctrl+D)
    /* SR_DEMOLITION_OPEN    */ "Facility demolition. %d facilities. %d of %d: %s",
    /* SR_DEMOLITION_ITEM    */ "%d of %d: %s, maintenance %d",
    /* SR_DEMOLITION_DETAIL  */ "%s. %s. Cost %d, maintenance %d",
    /* SR_DEMOLITION_CONFIRM */ "Press Delete again to demolish %s",
    /* SR_DEMOLITION_DONE    */ "%s demolished",
    /* SR_DEMOLITION_BLOCKED */ "Cannot: already scrapped one facility this turn",
    /* SR_DEMOLITION_EMPTY   */ "No facilities to demolish",
    /* SR_DEMOLITION_CANCEL  */ "Demolition mode closed",

    // Targeting Mode & Go to Base
    /* SR_TARGETING_MODE     */ "Target selection. Arrows move cursor, Enter confirms, Escape cancels.",
    /* SR_TARGETING_CANCEL   */ "Cancelled.",
    /* SR_GO_TO_BASE         */ "Go to base: %d bases. Up/Down to browse, Enter to send, Escape to cancel.",
    /* SR_BASE_LIST_FMT      */ "%d of %d: %s (%d, %d), %d tiles away",
    /* SR_BASE_LIST_EMPTY    */ "No bases available.",
    /* SR_GOING_HOME         */ "Going to home base: %s",
    /* SR_GOING_TO_BASE      */ "Going to %s",
    /* SR_NO_UNIT_SELECTED   */ "No unit selected.",
    /* SR_HELP_GO_TO_BASE    */ "G: Go to base",
    /* SR_HELP_GO_HOME       */ "Shift+G: Go to home base",
    /* SR_GROUP_GO_TO_BASE   */ "Group go to base: %d units, %d bases. Up/Down to browse, Enter to send all, Escape to cancel.",
    /* SR_GROUP_GOING_TO_BASE */ "Sending %d units to %s",
    /* SR_HELP_GROUP_GOTO    */ "J: Group go to",
    /* SR_HELP_PATROL        */ "P: Patrol",
    /* SR_HELP_ARTILLERY     */ "F: Long range fire",
    /* SR_HELP_SCAN_FILTER   */ "Ctrl+PgUp/PgDn: Scanner filter",
    /* SR_HELP_SCAN_JUMP     */ "Ctrl+Left/Right: Jump to match",
    /* SR_AIRDROP_NO_ABILITY */ "Cannot airdrop: unit lacks Drop Pod ability or already dropped this turn.",
    /* SR_AIRDROP_BASE       */ "Airdrop: %d bases in range. Up/Down to browse, Enter to drop, Escape to cancel.",
    /* SR_AIRDROP_BASE_FMT   */ "%d of %d: %s (%d, %d), %d tiles away",
    /* SR_AIRDROP_CONFIRM    */ "Airdropped to %s",
    /* SR_AIRDROP_BLOCKED    */ "Cannot airdrop: no valid targets in range.",

    /* SR_AIRDROP_CURSOR     */ "C: Drop at cursor (%d, %d), %d tiles away",
    /* SR_AIRDROP_CURSOR_CONFIRM */ "Airdropped to (%d, %d)",
    /* SR_AIRDROP_CURSOR_INVALID */ "Cannot drop at cursor: invalid target.",

    // Diplomacy
    /* SR_DIPLO_OPEN         */ "Diplomacy with %s",
    /* SR_DIPLO_CLOSED       */ "Diplomacy ended.",
    /* SR_DIPLO_HELP         */ "S or Tab: Relationship summary. Ctrl+F1: Help. Popup options navigate with arrows.",
    /* SR_DIPLO_SUMMARY      */ "%s. %s. %s",
    /* SR_DIPLO_STATUS_PACT  */ "Pact",
    /* SR_DIPLO_STATUS_TREATY */ "Treaty of Friendship",
    /* SR_DIPLO_STATUS_TRUCE */ "Truce",
    /* SR_DIPLO_STATUS_VENDETTA */ "Vendetta",
    /* SR_DIPLO_STATUS_NONE  */ "No formal agreement",
    /* SR_DIPLO_PATIENCE     */ "Patience: %s",
    /* SR_DIPLO_PATIENCE_THIN */ "wearing thin",
    /* SR_DIPLO_PATIENCE_WEARING */ "moderate",
    /* SR_DIPLO_PATIENCE_OK  */ "patient",
    /* SR_DIPLO_SURRENDERED  */ "Has surrendered",
    /* SR_DIPLO_INFILTRATOR  */ "We have infiltrator",
    /* SR_DIPLO_NETMSG       */ "%s",
    /* SR_DIPLO_COMMLINK_OPEN */ "Commlink: %d factions. Up/Down to browse, Enter to contact, Escape to cancel.",
    /* SR_DIPLO_COMMLINK_ITEM */ "%d of %d: %s, %s, %d votes",
    /* SR_DIPLO_COMMLINK_EMPTY */ "No factions with commlink.",
    /* SR_DIPLO_COMMLINK_CONTACT */ "Contacting %s",
    // Cursor / Numpad
    /* SR_CURSOR_TO_UNIT    */ "Cursor at unit.",

    // Turn Info
    /* SR_NEW_TURN          */ "Turn %d, Year %d M.Y.",
    /* SR_UNIT_SKIPPED      */ "Skipped. Next unit: %s",
    /* SR_UNIT_DAMAGED      */ "Damaged: %d of %d HP",

    // Input dialogs
    /* SR_INPUT_NUMBER       */ "Number input. Default: %d. Type digits, Enter to confirm, Escape for default.",
    /* SR_INPUT_NUMBER_EMPTY */ "Empty",
    /* SR_INPUT_NUMBER_DONE  */ "Entered %d.",

    // Design Workshop
    /* SR_DESIGN_PROTO_LIST    */ "Unit designs: %d prototypes.",
    /* SR_DESIGN_PROTO_FMT     */ "%d of %d: %s, Attack %d Defense %d Speed %d, %d minerals",
    /* SR_DESIGN_CATEGORY      */ "%s: %s",
    /* SR_DESIGN_CAT_CHASSIS   */ "Chassis",
    /* SR_DESIGN_CAT_WEAPON    */ "Weapon",
    /* SR_DESIGN_CAT_ARMOR     */ "Armor",
    /* SR_DESIGN_CAT_REACTOR   */ "Reactor",
    /* SR_DESIGN_CHASSIS_FMT   */ "%s, Speed %d, %s",
    /* SR_DESIGN_WEAPON_FMT    */ "%s, Attack %d",
    /* SR_DESIGN_ARMOR_FMT     */ "%s, Defense %d",
    /* SR_DESIGN_REACTOR_FMT   */ "%s, Level %d",
    /* SR_DESIGN_ABILITY_FMT   */ "Ability %d: %s",
    /* SR_DESIGN_ABILITY_NONE  */ "None",
    /* SR_DESIGN_COST          */ "Total cost: %d minerals",
    /* SR_DESIGN_SAVED         */ "Design saved: %s",
    /* SR_DESIGN_CANCELLED     */ "Design cancelled.",
    /* SR_DESIGN_NEW           */ "New unit design.",
    /* SR_DESIGN_RETIRED       */ "%s retired.",
    /* SR_DESIGN_RETIRE_CONFIRM */ "Press Delete again to retire %s.",
    /* SR_DESIGN_HELP          */ "Up/Down: browse. Enter: edit. N: new. Delete: retire. Escape: close.",
    /* SR_DESIGN_EDIT_HELP     */ "Left/Right: category. Up/Down: option. Enter: save. S: summary. Escape: cancel.",
    /* SR_DESIGN_TRIAD_LAND    */ "Land",
    /* SR_DESIGN_TRIAD_SEA     */ "Sea",
    /* SR_DESIGN_TRIAD_AIR     */ "Air",
    /* SR_DESIGN_EQUIPMENT     */ "Equipment: %s",

    // Terraform Status
    /* SR_TERRAFORM_ORDER      */ "Building %s, %d turns",
    /* SR_TERRAFORM_STATUS     */ "Building %s, %d of %d turns",
    /* SR_TERRAFORM_COMPLETE   */ "%s completed",

    // Nerve Staple (Ctrl+N)
    /* SR_NERVE_STAPLE_CONFIRM */ "Press Ctrl+N again to nerve staple %s.",
    /* SR_NERVE_STAPLE_DONE    */ "Nerve stapled %s. %d turns.",
    /* SR_NERVE_STAPLE_CANNOT  */ "Cannot: nerve staple not allowed.",

    // Base Rename (F2)
    /* SR_RENAME_OPEN          */ "Rename base: %s. Type new name, Enter to confirm, Escape to cancel.",
    /* SR_RENAME_CHAR_FMT      */ "%c",
    /* SR_RENAME_DONE          */ "Base renamed to %s.",
    /* SR_RENAME_CANCEL        */ "Rename cancelled.",

    // Base Open (extended announcement)
    /* SR_FMT_BASE_OPEN_NAME       */ "Base: %s, Population %d.",
    /* SR_FMT_BASE_OPEN_RESOURCES  */ "Nutrients %+d, %s. Minerals %+d. Energy %+d.",
    /* SR_FMT_BASE_OPEN_MOOD       */ "Talents %d, Drones %d.",
    /* SR_FMT_BASE_OPEN_PROD       */ "Building %s, %d of %d, %s.",

    // Governor Configuration (Ctrl+G)
    /* SR_GOV_TITLE            */ "Governor configuration. %d options.",
    /* SR_GOV_OPTION_FMT       */ "%d of %d: %s, %s",
    /* SR_GOV_ON               */ "on",
    /* SR_GOV_OFF              */ "off",
    /* SR_GOV_HELP             */ "Up/Down: Options. Space: Toggle. Left/Right: Change priority. S: Summary. Enter: Save. Escape: Cancel.",
    /* SR_GOV_SUMMARY_FMT      */ "Governor: %d of %d enabled.",
    /* SR_GOV_SAVED            */ "Governor settings saved.",
    /* SR_GOV_CANCELLED        */ "Governor settings cancelled.",
    /* SR_GOV_PRIORITY_FMT     */ "Priority: %s. Left/Right to change.",
    /* SR_GOV_PRIORITY_NONE    */ "None",
    /* SR_GOV_PRIORITY_EXPLORE */ "Explore",
    /* SR_GOV_PRIORITY_DISCOVER*/ "Discover",
    /* SR_GOV_PRIORITY_BUILD   */ "Build",
    /* SR_GOV_PRIORITY_CONQUER */ "Conquer",

    // Social Engineering (extended)
    /* SR_SOCENG_TOTAL_EFFECTS   */ "Total effects: %s",
    /* SR_SOCENG_NO_TOTAL_EFFECT */ "No total effects",
    /* SR_SOCENG_ALLOC_FMT       */ "Energy allocation: Economy %d%%, Psych %d%%, Labs %d%%",
    /* SR_SOCENG_ALLOC_MODE      */ "Energy allocation mode. Up/Down: select slider. Left/Right: adjust.",
    /* SR_SOCENG_ALLOC_SLIDER    */ "%s: %d%%",
    /* SR_SOCENG_INFO_FMT        */ "Researching %s. %d of %d labs, %d turns. Energy: %d credits, %+d per turn",
    /* SR_SOCENG_ALLOC_ECON      */ "Economy",
    /* SR_SOCENG_ALLOC_PSYCH     */ "Psych",
    /* SR_SOCENG_ALLOC_LABS      */ "Labs",

    // Garrison List (Ctrl+U)
    /* SR_GARRISON_OPEN          */ "Garrison: %d units. Up/Down: navigate, D: details, Enter: activate, B: home base, H: hurry, Escape: close.",
    /* SR_GARRISON_ITEM          */ "%d of %d: %s, %d/%d HP, %s",
    /* SR_GARRISON_DETAIL        */ "%s. %s %d, %s %d, %s, %d moves. Home: %s. %s",
    /* SR_GARRISON_ACTIVATE      */ "Activated: %s",
    /* SR_GARRISON_HOME_SET      */ "Home base set: %s",
    /* SR_GARRISON_EMPTY         */ "No units stationed.",
    /* SR_GARRISON_CLOSE         */ "Garrison list closed.",

    // Message Log (Ctrl+M)
    /* SR_MSG_OPEN               */ "Message log. %d messages.",
    /* SR_MSG_ITEM               */ "%d of %d: %s",
    /* SR_MSG_ITEM_LOC           */ "%d of %d: %s (%d, %d)",
    /* SR_MSG_EMPTY              */ "No messages.",
    /* SR_MSG_CLOSED             */ "Message log closed.",
    /* SR_MSG_NO_LOCATION        */ "No location for this message.",
    /* SR_MSG_SUMMARY            */ "%d messages. Turn %d.",
    /* SR_MSG_HELP               */ "Up/Down: Browse. Enter: Jump to location. S: Summary. Escape: Close.",
    /* SR_MSG_NOTIFICATION       */ "Message: %s",

    // Time Controls (Shift+T)
    /* SR_TC_OPEN                */ "Time Controls, %d presets. Current: %s. Up/Down to browse, Enter to select, Escape to cancel.",
    /* SR_TC_ITEM                */ "%d of %d: %s, %d seconds per turn, %d per base, %d per unit",
    /* SR_TC_SET                 */ "Time Controls set to %s",
    /* SR_TC_CANCELLED           */ "Time Controls cancelled",

    // Chat (Ctrl+C)
    /* SR_CHAT_OPEN              */ "Chat",

    // Base Screen: Resources/Economy/Status V3
    /* SR_FMT_RESOURCES_V3       */ "Resources: Nutrients %+d (%d of %d, %s), consuming %d. Minerals %+d, support %d. Energy %+d, inefficiency %d.",
    /* SR_FMT_STARVATION         */ "Starvation!",
    /* SR_FMT_LOW_MINERALS       */ "Low minerals.",
    /* SR_FMT_ECONOMY_V3         */ "Economy: %d economy, %d psych, %d labs. Commerce %d. %s.",
    /* SR_FMT_COMMERCE           */ "Commerce: %d energy from trade.",
    /* SR_FMT_INEFFICIENCY       */ " Inefficiency: %d energy lost.",
    /* SR_FMT_NO_HQ              */ " No headquarters!",
    /* SR_FMT_HIGH_SUPPORT       */ " High support: %d minerals.",
    /* SR_FMT_UNDEFENDED         */ " Undefended!",
    /* SR_FMT_POLICE_UNITS       */ " %d police units.",

    // Supported Units (Ctrl+Shift+S)
    /* SR_SUPPORT_OPEN           */ "Supported units: %d units, %d mineral support. Up/Down: navigate, D: details, Enter: activate, Escape: close.",
    /* SR_SUPPORT_ITEM           */ "%d of %d: %s, at (%d, %d), %d of %d HP",
    /* SR_SUPPORT_DETAIL         */ "%s. %s %d, %s %d, %s, %d moves. Home: %s. At (%d, %d). %s",
    /* SR_SUPPORT_EMPTY          */ "No supported units.",
    /* SR_SUPPORT_CLOSE          */ "Support list closed.",
    /* SR_SUPPORT_ACTIVATE       */ "Activated: %s",

    // Psych Detail (Ctrl+Shift+Y)
    /* SR_PSYCH_DETAIL           */ "Psych detail: %d police units. Psych output %d. Talents %d, Drones %d. %s",

    // Base Help (updated)
    /* SR_BASE_HELP_V2           */ "Left/Right: Previous/Next base. Ctrl+Up/Down: Sections. Ctrl+Left/Right: Tabs. Ctrl+I: Repeat. Ctrl+H: Hurry. Ctrl+Shift+P: Change production. Ctrl+Q: Queue. Ctrl+D: Demolish. Ctrl+U: Garrison. Ctrl+Shift+S: Supported units. Ctrl+W: Citizens. Ctrl+T: Tile assignment. Ctrl+G: Governor. Ctrl+N: Nerve staple. Ctrl+Shift+Y: Psych detail. F2: Rename. Ctrl+F1: Help. Escape: Close.",

    // Tile Assignment (Ctrl+T)
    /* SR_TILE_ASSIGN_OPEN       */ "Tile assignment: %d tiles, %d worked. Up/Down: browse, Space: toggle, S: summary, Escape: close.",
    /* SR_TILE_ASSIGN_WORKED     */ "%d of %d: Worked. (%d, %d) %s. %d food, %d minerals, %d energy.",
    /* SR_TILE_ASSIGN_AVAILABLE  */ "%d of %d: Available. (%d, %d) %s. %d food, %d minerals, %d energy.",
    /* SR_TILE_ASSIGN_CENTER     */ "%d of %d: Base center, always worked. (%d, %d) %s. %d food, %d minerals, %d energy.",
    /* SR_TILE_ASSIGN_UNAVAIL    */ "%d of %d: Unavailable, %s. (%d, %d) %s.",
    /* SR_TILE_ASSIGN_ASSIGNED   */ "Worker assigned. %d food, %d minerals, %d energy.",
    /* SR_TILE_ASSIGN_REMOVED    */ "Worker removed. Converted to %s.",
    /* SR_TILE_ASSIGN_CANNOT_CENTER */ "Cannot: base center is always worked.",
    /* SR_TILE_ASSIGN_CANNOT_UNAVAIL */ "Cannot: tile unavailable.",
    /* SR_TILE_ASSIGN_CANNOT_NO_FREE */ "Cannot: no free citizens. Remove a worker or use Ctrl+W first.",
    /* SR_TILE_ASSIGN_SUMMARY    */ "Tiles: %d worked of %d total, %d specialists. Total: %d food, %d minerals, %d energy.",
    /* SR_TILE_ASSIGN_CLOSE      */ "Tile assignment closed.",
    /* SR_TILE_UNAVAIL_FOREIGN   */ "foreign territory",
    /* SR_TILE_UNAVAIL_OTHER_BASE */ "worked by another base",
    /* SR_TILE_UNAVAIL_VEHICLE   */ "blocked by vehicle",
    /* SR_TILE_UNAVAIL_UNEXPLORED */ "unexplored",

    // Patrol (P key)
    /* SR_PATROL_CONFIRM  */ "Patrolling to (%d, %d).",
    /* SR_PATROL_CANNOT   */ "Cannot patrol.",
    /* SR_PATROL_PROMPT   */ "Patrol to (%d, %d)? Enter to confirm, Escape to cancel.",

    // Road/Tube to cursor (Ctrl+R / Ctrl+T)
    /* SR_ROAD_TO_CONFIRM */ "Building road to (%d, %d).",
    /* SR_ROAD_TO_PROMPT  */ "Build road to (%d, %d)? Enter to confirm, Escape to cancel.",
    /* SR_TUBE_TO_CONFIRM */ "Building mag tube to (%d, %d).",
    /* SR_TUBE_TO_PROMPT  */ "Build mag tube to (%d, %d)? Enter to confirm, Escape to cancel.",
    /* SR_ROAD_TO_CANNOT  */ "Cannot build: no former selected.",

    // Artillery (F key)
    /* SR_ARTY_CANNOT     */ "Cannot fire artillery.",
    /* SR_ARTY_NO_TARGETS */ "No targets within range (%d tiles).",
    /* SR_ARTY_OPEN       */ "Artillery: %d targets in range (%d tiles). Up/Down to browse, C for cursor, Enter to fire, Escape to cancel.",
    /* SR_ARTY_TARGET_FMT */ "%d of %d: %s (%d, %d), %d tiles away",
    /* SR_ARTY_CURSOR     */ "C: Fire at cursor (%d, %d), %d tiles away",
    /* SR_ARTY_CURSOR_OOR */ "C: Cursor (%d, %d), %d tiles — out of range (%d max)",
    /* SR_ARTY_FIRED      */ "Firing at %s.",
    /* SR_ARTY_HELP       */ "Up/Down: browse targets. C: cursor position. Enter: fire. Escape: cancel.",

    // World Map Help (additional)
    /* SR_HELP_CURSOR_TO_UNIT */ "Ctrl+Space: Jump cursor to unit",

    // Faction Status (Ctrl+F3)
    /* SR_STATUS_HEADER       */ "Turn %d, Year %d M.Y.",
    /* SR_STATUS_CREDITS      */ "%d energy credits.",
    /* SR_STATUS_ALLOC        */ "Econ %d%%, Psych %d%%, Labs %d%%.",
    /* SR_STATUS_INCOME       */ "Commerce: %d. Gross: %d. Maintenance: %d. Net: %d.",
    /* SR_STATUS_RESEARCH     */ "Researching %s, %d of %d (%d%%).",
    /* SR_STATUS_RESEARCH_NONE */ "No research in progress.",
    /* SR_STATUS_BASES        */ "%d bases, %d population.",
    /* SR_STATUS_HELP         */ "Ctrl+F3: Faction status.",

    // Resource Bonuses
    /* SR_BONUS_NUTRIENT      */ "Nutrient bonus",
    /* SR_BONUS_MINERAL       */ "Mineral bonus",
    /* SR_BONUS_ENERGY        */ "Energy bonus",

    // Landmarks
    /* SR_LANDMARK_CRATER     */ "Crater",
    /* SR_LANDMARK_VOLCANO    */ "Volcano",
    /* SR_LANDMARK_JUNGLE     */ "Jungle",
    /* SR_LANDMARK_URANIUM    */ "Uranium flats",
    /* SR_LANDMARK_SARGASSO   */ "Sargasso Sea",
    /* SR_LANDMARK_RUINS      */ "Ruins",
    /* SR_LANDMARK_DUNES      */ "Dunes",
    /* SR_LANDMARK_FRESH      */ "Freshwater Sea",
    /* SR_LANDMARK_MESA       */ "Mesa",
    /* SR_LANDMARK_CANYON     */ "Canyon",
    /* SR_LANDMARK_GEOTHERMAL */ "Geothermal",
    /* SR_LANDMARK_RIDGE      */ "Ridge",
    /* SR_LANDMARK_BOREHOLE   */ "Great Borehole",
    /* SR_LANDMARK_NEXUS      */ "Manifold Nexus",
    /* SR_LANDMARK_UNITY      */ "Unity wreckage",
    /* SR_LANDMARK_FOSSIL     */ "Fossil Ridge",

    // Move/View Mode
    /* SR_MODE_MOVE           */ "Move mode",
    /* SR_MODE_VIEW           */ "View mode",

    // Multiplayer Setup (NetWin)
    /* SR_NETSETUP_OPEN       */ "Multiplayer Setup. Game text will be read aloud.",
    /* SR_NETSETUP_HELP       */ "Multiplayer lobby. Arrow keys, Enter, Escape.",

    // Unit Action Menu (Shift+F10)
    /* SR_ACT_MENU_OPEN       */ "Unit actions, %d items. Up/Down, Enter, Escape.",
    /* SR_ACT_MENU_ITEM       */ "%d of %d: %s",
    /* SR_ACT_MENU_CANCEL     */ "Cancelled.",
    /* SR_ACT_MENU_EMPTY      */ "No actions available.",
    /* SR_ACT_SKIP            */ "Skip turn, Space",
    /* SR_ACT_HOLD            */ "Hold, H",
    /* SR_ACT_EXPLORE         */ "Explore, X",
    /* SR_ACT_GOTO_BASE       */ "Go to base, G",
    /* SR_ACT_PATROL          */ "Patrol, P",
    /* SR_ACT_BUILD_ROAD      */ "Build road, R",
    /* SR_ACT_BUILD_MAGTUBE   */ "Build mag tube, R",
    /* SR_ACT_BUILD_FARM      */ "Build farm, F",
    /* SR_ACT_BUILD_MINE      */ "Build mine, M",
    /* SR_ACT_BUILD_SOLAR     */ "Build solar collector, S",
    /* SR_ACT_BUILD_FOREST    */ "Plant forest, Shift+F",
    /* SR_ACT_BUILD_SENSOR    */ "Build sensor, O",
    /* SR_ACT_BUILD_CONDENSER */ "Build condenser, N",
    /* SR_ACT_BUILD_BOREHOLE  */ "Build borehole, Shift+N",
    /* SR_ACT_BUILD_AIRBASE   */ "Build airbase, Period",
    /* SR_ACT_BUILD_BUNKER    */ "Build bunker, Shift+Period",
    /* SR_ACT_REMOVE_FUNGUS   */ "Remove fungus, F",
    /* SR_ACT_AUTOMATE        */ "Automate, Shift+A",
    /* SR_ACT_BUILD_BASE      */ "Build base, B",
    /* SR_ACT_LONG_RANGE      */ "Long range fire, F",
    /* SR_ACT_CONVOY          */ "Convoy resources, O",
    /* SR_ACT_UNLOAD          */ "Unload transport, U",
    /* SR_ACT_AIRDROP         */ "Airdrop, I",
    /* SR_ACT_OPEN_BASE       */ "Open base, Enter",
    /* SR_ACT_MENU_HELP       */ "Shift+F10: Action menu",
    /* SR_ACT_DONE_HOLD       */ "Holding.",
    /* SR_ACT_DONE_EXPLORE    */ "Auto-exploring.",
    /* SR_ACT_DONE_UNLOAD     */ "Unloading.",

    // Automation (Shift+A)
    /* SR_AUTO_OPEN           */ "Automate: %d options. Up, Down, Enter, Escape.",
    /* SR_AUTO_ITEM           */ "%d of %d: %s",
    /* SR_AUTO_CONFIRM        */ "Automated: %s",
    /* SR_AUTO_CANCEL         */ "Cancelled",
    /* SR_AUTO_NOT_FORMER     */ "Cannot: not a former",
    /* SR_AUTO_FULL           */ "Full Terraforming",
    /* SR_AUTO_ROAD           */ "Auto Roads",
    /* SR_AUTO_MAGTUBE        */ "Auto Magtubes",
    /* SR_AUTO_IMPROVE_BASE   */ "Auto Improve Base",
    /* SR_AUTO_FARM_SOLAR     */ "Farm+Solar+Road",
    /* SR_AUTO_FARM_MINE      */ "Farm+Mine+Road",
    /* SR_AUTO_FUNGUS         */ "Auto Fungus Removal",
    /* SR_AUTO_SENSOR         */ "Auto Sensors",
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
    "unit_abilities",
    "queue_open", "queue_open_one", "queue_item", "queue_item_current",
    "queue_moved", "queue_removed", "queue_added", "queue_full",
    "queue_cannot_move_current", "queue_cannot_remove_current",
    "queue_closed", "queue_empty",
    "menu_game", "menu_hq", "menu_network", "menu_map", "menu_action",
    "menu_terraform", "menu_scenario", "menu_editmap", "menu_help",
    "menu_closed", "menu_entry_fmt", "menu_item_fmt", "menu_item_nohk",
    "menu_nav_fmt",
    "menu_main", "menu_map_menu", "menu_multiplayer",
    "menu_scenario_menu", "menu_thinker", "menu_game_menu",
    "menu_file_select",
    "file_load_game", "file_save_game", "file_folder", "file_item_fmt", "file_parent_dir",
    "file_overwrite_hint",
    "popup_list_fmt", "popup_continue",
    "soceng_title", "soceng_category_fmt", "soceng_model_fmt",
    "soceng_effects", "soceng_no_effect", "soceng_unavailable_tech",
    "soceng_unavailable_opposition", "soceng_selected", "soceng_current",
    "soceng_pending", "soceng_summary_fmt", "soceng_upheaval_cost",
    "soceng_help", "soceng_closed",
    // Preferences Handler
    "pref_title", "pref_tab_fmt", "pref_option_fmt", "pref_toggled_fmt",
    "pref_saved", "pref_cancelled", "pref_help", "pref_summary_fmt",
    "pref_enabled", "pref_disabled",
    // Preferences: Tab names
    "pref_tab_general", "pref_tab_warnings", "pref_tab_advanced",
    "pref_tab_automation", "pref_tab_audio_visual", "pref_tab_map_display",
    // Preferences: General tab
    "pref_pause_end_turn", "pref_autosave", "pref_quick_enemy",
    "pref_tutorial", "pref_mouse_edge_scroll", "pref_auto_design",
    "pref_quick_ally", "pref_auto_prune",
    // Preferences: Warnings tab
    "pref_warn_fac_built", "pref_warn_non_combat_built", "pref_warn_prototype",
    "pref_warn_drone_riots", "pref_warn_drone_riots_end", "pref_warn_golden_age",
    "pref_warn_golden_age_end", "pref_warn_nutrient_shortage",
    "pref_warn_build_out_of_date", "pref_warn_combat_built",
    "pref_warn_pop_limit", "pref_warn_delay_transcend",
    "pref_warn_built_via_queue", "pref_warn_starvation",
    "pref_warn_mineral_shortage", "pref_warn_energy_shortage",
    "pref_warn_random_event",
    // Preferences: Advanced tab
    "pref_fast_battle", "pref_no_center_orders", "pref_pause_after_battle",
    "pref_zoom_no_recenter", "pref_quick_move_orders", "pref_quick_move_all",
    "pref_right_click_menu", "pref_detail_right_click", "pref_confirm_odds",
    "pref_detail_main_menus", "pref_radio_single_click",
    // Preferences: Automation tab
    "pref_former_raise_lower", "pref_former_plant_forest", "pref_former_build_adv",
    "pref_former_remove_fungus", "pref_former_build_sensors",
    "pref_former_build_roads", "pref_end_move_pact", "pref_end_move_treaty",
    "pref_end_move_truce", "pref_end_move_war", "pref_end_move_diff_triad",
    "pref_air_return_home", "pref_wake_on_land", "pref_always_inspect_monolith",
    // Preferences: Audio/Visual tab
    "pref_sound_effects", "pref_background_music", "pref_map_animations",
    "pref_sliding_windows", "pref_project_movies", "pref_interludes",
    "pref_voiceover", "pref_stop_voice_close", "pref_sliding_scrollbars",
    "pref_whole_veh_blinks", "pref_monuments",
    // Preferences: Map Display tab
    "pref_show_grid", "pref_show_base_grid", "pref_fog_of_war",
    "pref_base_names", "pref_prod_with_names", "pref_flat_terrain",
    "pref_grid_ocean", "pref_show_goto_path",
    // Specialist Management
    "spec_open", "spec_close", "spec_worker", "spec_specialist",
    "spec_bonus", "spec_to_specialist", "spec_to_worker", "spec_type_changed",
    "spec_cannot_more", "spec_cannot_less", "spec_cannot_center",
    "spec_help", "spec_summary", "spec_center",
    "spec_no_other_type", "spec_worker_no_type",
    // Facility Demolition
    "demolition_open", "demolition_item", "demolition_detail",
    "demolition_confirm", "demolition_done", "demolition_blocked",
    "demolition_empty", "demolition_cancel",
    // Targeting Mode & Go to Base
    "targeting_mode", "targeting_cancel", "go_to_base", "base_list_fmt",
    "base_list_empty", "going_home", "going_to_base", "no_unit_selected",
    "group_go_to_base", "group_going_to_base",
    "help_go_to_base", "help_go_home", "help_group_goto", "help_patrol",
    "help_artillery", "help_scan_filter", "help_scan_jump",
    "airdrop_no_ability", "airdrop_base", "airdrop_base_fmt",
    "airdrop_confirm", "airdrop_blocked",
    "airdrop_cursor", "airdrop_cursor_confirm", "airdrop_cursor_invalid",
    // Diplomacy
    "diplo_open", "diplo_closed", "diplo_help", "diplo_summary",
    "diplo_status_pact", "diplo_status_treaty", "diplo_status_truce",
    "diplo_status_vendetta", "diplo_status_none",
    "diplo_patience", "diplo_patience_thin", "diplo_patience_wearing",
    "diplo_patience_ok", "diplo_surrendered", "diplo_infiltrator",
    "diplo_netmsg",
    "diplo_commlink_open", "diplo_commlink_item",
    "diplo_commlink_empty", "diplo_commlink_contact",
    // Cursor / Numpad
    "cursor_to_unit",
    // Turn Info
    "new_turn",
    "unit_skipped",
    "unit_damaged",
    "input_number", "input_number_empty", "input_number_done",
    // Design Workshop
    "design_proto_list", "design_proto_fmt", "design_category",
    "design_cat_chassis", "design_cat_weapon", "design_cat_armor",
    "design_cat_reactor",
    "design_chassis_fmt", "design_weapon_fmt", "design_armor_fmt",
    "design_reactor_fmt", "design_ability_fmt", "design_ability_none",
    "design_cost", "design_saved", "design_cancelled",
    "design_new", "design_retired", "design_retire_confirm",
    "design_help", "design_edit_help",
    "design_triad_land", "design_triad_sea", "design_triad_air",
    "design_equipment",
    // Terraform Status
    "terraform_order", "terraform_status", "terraform_complete",
    // Nerve Staple
    "nerve_staple_confirm", "nerve_staple_done", "nerve_staple_cannot",
    // Base Rename
    "rename_open", "rename_char_fmt", "rename_done", "rename_cancel",
    // Base Open (extended announcement)
    "fmt_base_open_name", "fmt_base_open_resources",
    "fmt_base_open_mood", "fmt_base_open_prod",
    // Governor Configuration
    "gov_title", "gov_option_fmt", "gov_on", "gov_off",
    "gov_help", "gov_summary_fmt", "gov_saved", "gov_cancelled",
    "gov_priority_fmt", "gov_priority_none", "gov_priority_explore",
    "gov_priority_discover", "gov_priority_build", "gov_priority_conquer",
    // Social Engineering (extended)
    "soceng_total_effects", "soceng_no_total_effect",
    "soceng_alloc_fmt", "soceng_alloc_mode", "soceng_alloc_slider",
    "soceng_info_fmt", "soceng_alloc_econ", "soceng_alloc_psych",
    "soceng_alloc_labs",
    // Garrison List
    "garrison_open", "garrison_item", "garrison_detail",
    "garrison_activate", "garrison_home_set", "garrison_empty",
    "garrison_close",
    // Message Log
    "msg_open", "msg_item", "msg_item_loc", "msg_empty",
    "msg_closed", "msg_no_location", "msg_summary", "msg_help",
    "msg_notification",
    // Time Controls
    "tc_open", "tc_item", "tc_set", "tc_cancelled",
    // Chat
    "chat_open",
    // Base Screen: Resources/Economy/Status V3
    "fmt_resources_v3", "fmt_starvation", "fmt_low_minerals",
    "fmt_economy_v3", "fmt_commerce",
    "fmt_inefficiency", "fmt_no_hq", "fmt_high_support",
    "fmt_undefended", "fmt_police_units",
    // Supported Units
    "support_open", "support_item", "support_detail",
    "support_empty", "support_close", "support_activate",
    // Psych Detail
    "psych_detail",
    // Base Help V2
    "base_help_v2",
    // Tile Assignment
    "tile_assign_open", "tile_assign_worked", "tile_assign_available",
    "tile_assign_center", "tile_assign_unavail",
    "tile_assign_assigned", "tile_assign_removed",
    "tile_assign_cannot_center", "tile_assign_cannot_unavail",
    "tile_assign_cannot_no_free", "tile_assign_summary",
    "tile_assign_close",
    "tile_unavail_foreign", "tile_unavail_other_base",
    "tile_unavail_vehicle", "tile_unavail_unexplored",
    // Patrol
    "patrol_confirm", "patrol_cannot", "patrol_prompt",
    // Road/Tube to cursor
    "road_to_confirm", "road_to_prompt",
    "tube_to_confirm", "tube_to_prompt",
    "road_to_cannot",
    // Artillery
    "arty_cannot", "arty_no_targets", "arty_open", "arty_target_fmt",
    "arty_cursor", "arty_cursor_oor", "arty_fired", "arty_help",
    // World Map Help (additional)
    "help_cursor_to_unit",
    // Faction Status
    "status_header", "status_credits", "status_alloc", "status_income",
    "status_research", "status_research_none", "status_bases", "status_help",
    // Resource Bonuses
    "bonus_nutrient", "bonus_mineral", "bonus_energy",
    // Landmarks
    "landmark_crater", "landmark_volcano", "landmark_jungle",
    "landmark_uranium", "landmark_sargasso", "landmark_ruins",
    "landmark_dunes", "landmark_fresh", "landmark_mesa",
    "landmark_canyon", "landmark_geothermal", "landmark_ridge",
    "landmark_borehole", "landmark_nexus", "landmark_unity",
    "landmark_fossil",
    // Move/View Mode
    "mode_move", "mode_view",
    // Multiplayer Setup (NetWin)
    "netsetup_open", "netsetup_help",
    // Unit Action Menu (Shift+F10)
    "act_menu_open", "act_menu_item", "act_menu_cancel", "act_menu_empty",
    "act_skip", "act_hold", "act_explore", "act_goto_base", "act_patrol",
    "act_build_road", "act_build_magtube", "act_build_farm", "act_build_mine",
    "act_build_solar", "act_build_forest", "act_build_sensor",
    "act_build_condenser", "act_build_borehole", "act_build_airbase",
    "act_build_bunker", "act_remove_fungus", "act_automate",
    "act_build_base", "act_long_range", "act_convoy", "act_unload",
    "act_airdrop", "act_open_base", "act_menu_help",
    "act_done_hold", "act_done_explore", "act_done_unload",
    // Automation (Shift+A)
    "auto_open", "auto_item", "auto_confirm", "auto_cancel",
    "auto_not_former",
    "auto_full", "auto_road", "auto_magtube", "auto_improve_base",
    "auto_farm_solar", "auto_farm_mine", "auto_fungus", "auto_sensor",
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

// Compile-time check: enum, defaults, and keys arrays must all have SR_COUNT entries.
// A mismatch here means a new string was added to one but not the others.
static_assert(sizeof(sr_defaults) / sizeof(sr_defaults[0]) == SR_COUNT,
    "sr_defaults array size mismatch with SR_COUNT — did you forget to add a default?");
static_assert(sizeof(sr_keys) / sizeof(sr_keys[0]) == SR_COUNT,
    "sr_keys array size mismatch with SR_COUNT — did you forget to add a key?");

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

/*
Auto-detect game language by checking alphax.txt for German content.
The German language patch translates comments and labels in this file.
Returns "de" if German detected, "en" otherwise.
*/
static const char* loc_detect_language() {
    FILE* f = fopen("alphax.txt", "r");
    if (!f) {
        debug("loc_detect: alphax.txt not found, defaulting to en\n");
        return "en";
    }
    char line[512];
    int lines_read = 0;
    bool found_german = false;
    // Check first 30 lines for German indicators
    while (fgets(line, sizeof(line), f) && lines_read < 30) {
        lines_read++;
        // German patch header: "Benutzerkonfigurierbare Regeln"
        if (strstr(line, "Benutzerkonfig") != NULL) { found_german = true; break; }
        // German patch: "Grundregeln" or "Fortbewegung"
        if (strstr(line, "GRUNDREGELN") != NULL) { found_german = true; break; }
        if (strstr(line, "Fortbewegung") != NULL) { found_german = true; break; }
        // German patch: "Sicherungskopie"
        if (strstr(line, "Sicherungskopie") != NULL) { found_german = true; break; }
    }
    fclose(f);
    if (found_german) {
        debug("loc_detect: German game text detected in alphax.txt\n");
        return "de";
    }
    debug("loc_detect: No German detected, defaulting to en\n");
    return "en";
}

void loc_init() {
    // Read language setting from thinker.ini
    char lang[32] = "auto";
    char val[32];
    if (GetPrivateProfileStringA("thinker", "sr_language", "auto",
            val, sizeof(val), ".\\thinker.ini")) {
        // Trim and validate
        char* trimmed = loc_trim(val);
        if (trimmed[0] && strlen(trimmed) < sizeof(lang)) {
            strncpy(lang, trimmed, sizeof(lang) - 1);
            lang[sizeof(lang) - 1] = '\0';
        }
    }

    // Auto-detect game language if set to "auto" or empty
    if (strcmp(lang, "auto") == 0 || lang[0] == '\0') {
        const char* detected = loc_detect_language();
        strncpy(lang, detected, sizeof(lang) - 1);
        lang[sizeof(lang) - 1] = '\0';
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
