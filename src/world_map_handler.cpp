
#include "world_map_handler.h"
#include "engine.h"
#include "gui.h"
#include "screen_reader.h"
#include "localization.h"
#include "design_handler.h"
#include "social_handler.h"
#include "prefs_handler.h"
#include "specialist_handler.h"
#include "veh.h"
#include "move.h"

#include <algorithm>
#include <vector>

namespace WorldMapHandler {

// --- Static state ---

// Virtual exploration cursor (independent of game cursor)
static int sr_cursor_x = -1;
static int sr_cursor_y = -1;

// Targeting mode state (for J, P, F, Ctrl+R road-to, Ctrl+T tube-to)
static bool sr_targeting_active = false;
static DWORD sr_targeting_time = 0;

// Map position tracking with debounce
static int sr_map_x = -999;
static int sr_map_y = -999;
static DWORD sr_map_change_time = 0;
static bool sr_map_pending = false;

// World map transition detection
static bool sr_prev_on_world_map = false;

// Unit change detection
static int sr_prev_unit = -1;

// Turn change detection
static int sr_prev_turn = -1;

// Skip detection: set when Space is pressed, cleared on unit change
static bool sr_unit_skipped = false;

// Terraform order tracking
static uint8_t sr_prev_order = 0;       // previous order of current unit
static int sr_prev_order_unit = -1;     // which unit sr_prev_order belongs to

// Trigger 4: worldmap poll timer
static DWORD sr_worldmap_poll_time = 0;

// Scanner filter state
static int sr_scan_filter = 0;
static const int SR_SCAN_FILTER_COUNT = 10;

static const SrStr sr_scan_filter_names[SR_SCAN_FILTER_COUNT] = {
    SR_SCAN_ALL, SR_SCAN_OWN_BASES, SR_SCAN_ENEMY_BASES,
    SR_SCAN_ENEMY_UNITS, SR_SCAN_OWN_UNITS, SR_SCAN_OWN_FORMERS,
    SR_SCAN_FUNGUS, SR_SCAN_PODS, SR_SCAN_IMPROVEMENTS, SR_SCAN_NATURE,
};


// --- Helper functions ---

/// Get the localized terraform name for a vehicle's current order.
/// Returns NULL if the vehicle is not performing a terraform action.
static const char* get_terraform_name(VEH* veh) {
    if (veh->order < ORDER_FARM || veh->order >= ORDER_MOVE_TO) {
        return NULL;
    }
    int idx = veh->order - ORDER_FARM;
    MAP* sq = mapsq(veh->x, veh->y);
    if (!sq) return NULL;
    const char* raw = is_ocean(sq)
        ? Terraform[idx].name_sea
        : Terraform[idx].name;
    if (!raw || !raw[0]) return NULL;
    return sr_game_str(raw);
}

/// Announce tile info at given coordinates via screen reader.
/// Reusable for both MAP-MOVE tracking and virtual exploration cursor.
static void sr_announce_tile(int x, int y) {
    if (!sr_is_available()) return;
    if (*MapAreaX <= 0 || *MapAreaY <= 0) return;
    if (x < 0 || x >= *MapAreaX || y < 0 || y >= *MapAreaY) {
        sr_output(loc(SR_MAP_EDGE), true);
        return;
    }
    MAP* sq = mapsq(x, y);
    if (!sq) return;

    int faction = *CurrentPlayerFaction;
    char buf[512];
    int pos = 0;

    // Coordinates
    pos += snprintf(buf + pos, sizeof(buf) - pos, "(%d, %d) ", x, y);

    // Unexplored check
    if (!sq->is_visible(faction)) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TILE_UNEXPLORED));
        sr_debug_log("TILE-ANNOUNCE (%d,%d): %s", x, y, buf);
        sr_output(buf, true);
        return;
    }

    // Terrain type
    int alt = sq->alt_level();
    bool is_land = (alt >= ALT_SHORE_LINE);
    if (alt == ALT_OCEAN_TRENCH) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_TRENCH));
    } else if (alt <= ALT_OCEAN) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_OCEAN));
    } else if (alt == ALT_OCEAN_SHELF) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_SHELF));
    } else {
        if (sq->is_rocky())
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_ROCKY));
        else if (sq->is_rolling())
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_ROLLING));
        else
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_FLAT));
    }

    // Moisture (land only)
    if (is_land) {
        if (sq->is_rainy())
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_TERRAIN_RAINY));
        else if (sq->is_moist())
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_TERRAIN_MOIST));
        else if (sq->is_arid())
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_TERRAIN_ARID));
    }

    // Altitude (land only, elevated = 2+ above sea level)
    if (is_land && alt >= ALT_TWO_ABOVE_SEA) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_TERRAIN_HIGH));
    }

    // Features
    if (sq->is_fungus())
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_FUNGUS));
    if (sq->items & BIT_FOREST)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_FOREST));
    if (sq->items & BIT_RIVER)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_RIVER));
    if (sq->items & BIT_FARM)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_FARM));
    if (sq->items & BIT_SOIL_ENRICHER)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_SOIL_ENRICHER));
    if (sq->items & BIT_MINE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_MINE));
    if (sq->items & BIT_SOLAR)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_SOLAR));
    if (sq->items & BIT_CONDENSER)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_CONDENSER));
    if (sq->items & BIT_ECH_MIRROR)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_ECH_MIRROR));
    if (sq->items & BIT_THERMAL_BORE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_BOREHOLE));
    if (sq->items & BIT_ROAD)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_ROAD));
    if (sq->items & BIT_MAGTUBE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_MAGTUBE));
    if (sq->items & BIT_BUNKER)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_BUNKER));
    if (sq->items & BIT_AIRBASE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_AIRBASE));
    if (sq->items & BIT_SENSOR)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_SENSOR));
    if (sq->items & BIT_SUPPLY_POD)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_SUPPLY_POD));
    if (sq->items & BIT_MONOLITH)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_MONOLITH));

    // Resource bonus
    int bonus = mod_bonus_at(x, y);
    if (bonus == 1)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_BONUS_NUTRIENT));
    else if (bonus == 2)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_BONUS_MINERAL));
    else if (bonus == 3)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_BONUS_ENERGY));

    // Landmarks
    uint32_t lm = sq->landmarks & 0xFFFF;
    if (lm) {
        static const struct { uint32_t bit; SrStr str; } lm_table[] = {
            { LM_CRATER, SR_LANDMARK_CRATER },
            { LM_VOLCANO, SR_LANDMARK_VOLCANO },
            { LM_JUNGLE, SR_LANDMARK_JUNGLE },
            { LM_URANIUM, SR_LANDMARK_URANIUM },
            { LM_SARGASSO, SR_LANDMARK_SARGASSO },
            { LM_RUINS, SR_LANDMARK_RUINS },
            { LM_DUNES, SR_LANDMARK_DUNES },
            { LM_FRESH, SR_LANDMARK_FRESH },
            { LM_MESA, SR_LANDMARK_MESA },
            { LM_CANYON, SR_LANDMARK_CANYON },
            { LM_GEOTHERMAL, SR_LANDMARK_GEOTHERMAL },
            { LM_RIDGE, SR_LANDMARK_RIDGE },
            { LM_BOREHOLE, SR_LANDMARK_BOREHOLE },
            { LM_NEXUS, SR_LANDMARK_NEXUS },
            { LM_UNITY, SR_LANDMARK_UNITY },
            { LM_FOSSIL, SR_LANDMARK_FOSSIL },
        };
        for (int i = 0; i < 16; i++) {
            if (lm & lm_table[i].bit) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(lm_table[i].str));
            }
        }
    }

    // Resource yields
    int food = mod_crop_yield(faction, -1, x, y, 0);
    int mins = mod_mine_yield(faction, -1, x, y, 0);
    int energy = mod_energy_yield(faction, -1, x, y, 0);
    pos += snprintf(buf + pos, sizeof(buf) - pos, ". ");
    pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_TILE_YIELDS), food, mins, energy);

    // Ownership
    if (sq->owner >= 0 && sq->owner < MaxPlayerNum) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ". ");
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_TILE_OWNER),
            MFactions[sq->owner].noun_faction);
    } else {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ". %s", loc(SR_TILE_UNOWNED));
    }

    // City radius
    if (sq->items & BIT_BASE_RADIUS) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ". %s", loc(SR_TILE_IN_RADIUS));
    }

    // Base on tile
    int base_id = base_at(x, y);
    if (base_id >= 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_BASE_AT), sr_game_str(Bases[base_id].name));
    }

    // Units on this tile
    if (Vehs && *VehCount > 0) {
        int unit_count = 0;
        for (int i = 0; i < *VehCount && unit_count < 3; i++) {
            if (Vehs[i].x == x && Vehs[i].y == y) {
                if (unit_count == 0) {
                    pos += snprintf(buf + pos, sizeof(buf) - pos,
                        loc(SR_UNIT_AT), sr_game_str(Vehs[i].name()));
                } else {
                    pos += snprintf(buf + pos, sizeof(buf) - pos,
                        ", %s", sr_game_str(Vehs[i].name()));
                }
                unit_count++;
            }
        }
        if (unit_count > 0) {
            // Count remaining
            int remaining = 0;
            for (int i = 0; i < *VehCount; i++) {
                if (Vehs[i].x == x && Vehs[i].y == y) remaining++;
            }
            if (remaining > unit_count) {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                    loc(SR_MORE_UNITS), remaining - unit_count);
            }
        }
    }

    sr_debug_log("TILE-ANNOUNCE (%d,%d): %s", x, y, buf);
    sr_output(buf, true);
}

/// Check if tile (x,y) matches the current scanner filter for given faction.
static bool sr_scan_matches(int x, int y, int filter, int faction) {
    MAP* sq = mapsq(x, y);
    if (!sq || !sq->is_visible(faction)) return false;

    switch (filter) {
    case 0: // All non-empty
        return (sq->items & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE | BIT_FARM | BIT_MINE
            | BIT_SOLAR | BIT_ROAD | BIT_MAGTUBE | BIT_FOREST | BIT_RIVER | BIT_FUNGUS
            | BIT_CONDENSER | BIT_ECH_MIRROR | BIT_SOIL_ENRICHER | BIT_THERMAL_BORE
            | BIT_BUNKER | BIT_AIRBASE | BIT_SENSOR | BIT_SUPPLY_POD | BIT_MONOLITH)) != 0;
    case 1: // Own bases
        return (sq->items & BIT_BASE_IN_TILE) && sq->owner == faction;
    case 2: // Enemy bases
        return (sq->items & BIT_BASE_IN_TILE) && sq->owner != faction && sq->owner >= 0;
    case 3: // Enemy units
        if (Vehs && *VehCount > 0) {
            for (int i = 0; i < *VehCount; i++) {
                if (Vehs[i].x == x && Vehs[i].y == y && Vehs[i].faction_id != faction)
                    return true;
            }
        }
        return false;
    case 4: // Own units
        if (Vehs && *VehCount > 0) {
            for (int i = 0; i < *VehCount; i++) {
                if (Vehs[i].x == x && Vehs[i].y == y && Vehs[i].faction_id == faction)
                    return true;
            }
        }
        return false;
    case 5: // Own formers
        if (Vehs && *VehCount > 0) {
            for (int i = 0; i < *VehCount; i++) {
                if (Vehs[i].x == x && Vehs[i].y == y && Vehs[i].faction_id == faction
                    && Vehs[i].is_former())
                    return true;
            }
        }
        return false;
    case 6: // Fungus
        return sq->is_fungus();
    case 7: // Supply pods / Monoliths
        return (sq->items & (BIT_SUPPLY_POD | BIT_MONOLITH)) != 0;
    case 8: // Improvements
        return (sq->items & (BIT_FARM | BIT_MINE | BIT_SOLAR | BIT_ROAD | BIT_MAGTUBE
            | BIT_SENSOR | BIT_BUNKER | BIT_AIRBASE | BIT_CONDENSER | BIT_ECH_MIRROR
            | BIT_SOIL_ENRICHER | BIT_THERMAL_BORE)) != 0;
    case 9: // Terrain & Nature
        return (sq->items & (BIT_RIVER | BIT_FOREST | BIT_FUNGUS)) != 0
            || sq->is_fungus();
    }
    return false;
}

/// Advance to next valid tile coordinate (row by row, left to right).
/// Returns false if wrapped back to start position.
static bool sr_scan_next_tile(int* x, int* y, int start_x, int start_y) {
    *x += 2;
    if (*x >= *MapAreaX) {
        *y += 1;
        if (*y >= *MapAreaY) {
            *y = 0;
        }
        *x = (*y % 2 == 0) ? 0 : 1;
    }
    return !(*x == start_x && *y == start_y);
}

/// Go to previous valid tile coordinate (row by row, right to left).
/// Returns false if wrapped back to start position.
static bool sr_scan_prev_tile(int* x, int* y, int start_x, int start_y) {
    *x -= 2;
    if (*x < 0) {
        *y -= 1;
        if (*y < 0) {
            *y = *MapAreaY - 1;
        }
        // Largest valid x in row: x+y must be even, x < MapAreaX
        *x = *MapAreaX - 1;
        if ((*x + *y) % 2 != 0) *x -= 1;
    }
    return !(*x == start_x && *y == start_y);
}

/// Initialize cursor to current unit position if not set.
static void init_cursor_if_needed() {
    if (sr_cursor_x < 0 || sr_cursor_y < 0) {
        if (MapWin->iUnit >= 0 && Vehs && *VehCount > 0
            && MapWin->iUnit < *VehCount) {
            sr_cursor_x = Vehs[MapWin->iUnit].x;
            sr_cursor_y = Vehs[MapWin->iUnit].y;
        } else {
            sr_cursor_x = MapWin->iTileX;
            sr_cursor_y = MapWin->iTileY;
        }
    }
}


// --- Public API ---

int GetCursorX() { return sr_cursor_x; }
int GetCursorY() { return sr_cursor_y; }
bool IsTargetingActive() { return sr_targeting_active; }

void SetCursor(int x, int y) {
    sr_cursor_x = x;
    sr_cursor_y = y;
    if (MapWin) {
        MapWin_set_center(MapWin, x, y, 1);
    }
    sr_announce_tile(x, y);
}

void SetCursorToUnit() {
    if (MapWin && MapWin->iUnit >= 0 && Vehs && *VehCount > 0
        && MapWin->iUnit < *VehCount) {
        sr_cursor_x = Vehs[MapWin->iUnit].x;
        sr_cursor_y = Vehs[MapWin->iUnit].y;
    }
}


void OnTimer(DWORD now, bool on_world_map, GameWinState cur_win, int cur_popup,
             char* sr_announced, int sr_announced_size) {
    (void)cur_win;
    (void)cur_popup;

    // Announce world map transition (spoken)
    if (on_world_map && !sr_prev_on_world_map) {
        sr_debug_log("ANNOUNCE-SCREEN: World Map");
        sr_output(loc(SR_WORLD_MAP), true);
        sr_announced[0] = '\0';
        // Initialize exploration cursor to current unit position
        if (MapWin->iUnit >= 0 && Vehs && *VehCount > 0
            && MapWin->iUnit < *VehCount) {
            sr_cursor_x = Vehs[MapWin->iUnit].x;
            sr_cursor_y = Vehs[MapWin->iUnit].y;
        } else {
            sr_cursor_x = MapWin->iTileX;
            sr_cursor_y = MapWin->iTileY;
        }
        // Initialize map tracker to current position so no initial
        // tile announcement fires — only announce on actual movement
        sr_map_x = MapWin->iTileX;
        sr_map_y = MapWin->iTileY;
        sr_map_pending = false;
        sr_debug_log("CURSOR-INIT (%d,%d)", sr_cursor_x, sr_cursor_y);
    }
    sr_prev_on_world_map = on_world_map;

    // Map position tracking with 150ms debounce
    if (on_world_map && *MapAreaX > 0 && *MapAreaY > 0) {
        int mx = MapWin->iTileX;
        int my = MapWin->iTileY;
        // Skip if coordinates look uninitialized/garbage
        if (mx < 0 || mx >= *MapAreaX || my < 0 || my >= *MapAreaY) {
            sr_map_x = -999;
            sr_map_y = -999;
        } else if (mx != sr_map_x || my != sr_map_y) {
            sr_map_x = mx;
            sr_map_y = my;
            sr_map_change_time = now;
            sr_map_pending = true;
        }
        if (sr_map_pending && (now - sr_map_change_time) > 150) {
            sr_map_pending = false;
            sr_announce_tile(sr_map_x, sr_map_y);
        }
    } else {
        sr_map_x = -999;
        sr_map_y = -999;
    }

    // Targeting mode auto-reset
    if (sr_targeting_active) {
        if (!on_world_map) {
            sr_targeting_active = false;
            sr_debug_log("TARGETING: auto-reset (left world map)");
        } else if (now - sr_targeting_time > 30000) {
            sr_targeting_active = false;
            sr_debug_log("TARGETING: auto-reset (timeout)");
        }
    }

    // Turn change detection: announce new turn with year (queued)
    if (on_world_map) {
        int cur_turn = *CurrentTurn;
        if (cur_turn != sr_prev_turn) {
            sr_prev_turn = cur_turn;
            char turn_buf[128];
            snprintf(turn_buf, sizeof(turn_buf),
                loc(SR_NEW_TURN), cur_turn, *CurrentMissionYear);
            sr_debug_log("NEW-TURN: %s", turn_buf);
            sr_output(turn_buf, false);
        }
    }

    // Player turn / unit selection detection
    if (on_world_map && MapWin && Vehs && *VehCount > 0) {
        int cur_unit = MapWin->iUnit;
        if (cur_unit != sr_prev_unit) {
            sr_debug_log("UNIT-CHANGE: prev=%d cur=%d VehCount=%d skipped=%d",
                sr_prev_unit, cur_unit, *VehCount, (int)sr_unit_skipped);
            if (cur_unit >= 0 && cur_unit < *VehCount) {
                VEH* veh = &Vehs[cur_unit];
                sr_debug_log("UNIT-CHECK: faction=%d owner=%d unit_id=%d",
                    (int)veh->faction_id, (int)MapWin->cOwner,
                    (int)veh->unit_id);
                if (veh->faction_id == MapWin->cOwner
                    && veh->unit_id >= 0) {
                    // Update cursor to new unit position
                    sr_cursor_x = veh->x;
                    sr_cursor_y = veh->y;

                    if (sr_unit_skipped) {
                        // Skip feedback: "Skipped. Next unit: [name]"
                        char skip_buf[256];
                        snprintf(skip_buf, sizeof(skip_buf),
                            loc(SR_UNIT_SKIPPED), sr_game_str(veh->name()));
                        sr_debug_log("UNIT-SKIP: %s", skip_buf);
                        sr_output(skip_buf, true);
                        // Queue position and movement info
                        int total_speed = veh_speed(cur_unit, 0);
                        int remaining = max(0, total_speed - (int)veh->moves_spent);
                        int disp_rem = remaining / Rules->move_rate_roads;
                        int disp_tot = total_speed / Rules->move_rate_roads;
                        char pos_buf[128];
                        snprintf(pos_buf, sizeof(pos_buf), "(%d, %d). ",
                            veh->x, veh->y);
                        char move_buf[64];
                        snprintf(move_buf, sizeof(move_buf),
                            loc(SR_MOVEMENT_POINTS), disp_rem, disp_tot);
                        char detail[192];
                        snprintf(detail, sizeof(detail), "%s%s", pos_buf, move_buf);
                        sr_output(detail, false);
                        // Terraform status for formers
                        const char* tf_name = get_terraform_name(veh);
                        if (tf_name) {
                            int rate = Terraform[veh->order - ORDER_FARM].rate;
                            int done = rate - (int)veh->movement_turns;
                            char tf_buf[128];
                            snprintf(tf_buf, sizeof(tf_buf),
                                loc(SR_TERRAFORM_STATUS), tf_name, done, rate);
                            sr_output(tf_buf, false);
                        }
                    } else {
                        // Normal turn announcement
                        int total_speed = veh_speed(cur_unit, 0);
                        int remaining = max(0, total_speed - (int)veh->moves_spent);
                        int disp_rem = remaining / Rules->move_rate_roads;
                        int disp_tot = total_speed / Rules->move_rate_roads;
                        char buf[256];
                        char move_buf[64];
                        snprintf(move_buf, sizeof(move_buf),
                            loc(SR_MOVEMENT_POINTS), disp_rem, disp_tot);
                        snprintf(buf, sizeof(buf), "%s. %s",
                            loc(SR_YOUR_TURN), move_buf);
                        char full[320];
                        snprintf(full, sizeof(full), buf,
                            sr_game_str(veh->name()), veh->x, veh->y);
                        sr_debug_log("ANNOUNCE-TURN: %s", full);
                        sr_output(full, false);
                        // Terraform status for formers
                        const char* tf_name = get_terraform_name(veh);
                        if (tf_name) {
                            int rate = Terraform[veh->order - ORDER_FARM].rate;
                            int done = rate - (int)veh->movement_turns;
                            char tf_buf[128];
                            snprintf(tf_buf, sizeof(tf_buf),
                                loc(SR_TERRAFORM_STATUS), tf_name, done, rate);
                            sr_output(tf_buf, false);
                        }
                    }
                    // Damage announcement (only when damaged)
                    if (veh->damage_taken > 0) {
                        int cur_hp = veh->cur_hitpoints();
                        int max_hp = veh->max_hitpoints();
                        char dmg_buf[64];
                        snprintf(dmg_buf, sizeof(dmg_buf),
                            loc(SR_UNIT_DAMAGED), cur_hp, max_hp);
                        sr_output(dmg_buf, false);
                    }
                }
            }
            // Check if the PREVIOUS unit got a terraform order before we switched
            if (sr_prev_order_unit >= 0 && sr_prev_order_unit < *VehCount
                && sr_prev_order_unit != cur_unit) {
                VEH* prev_veh = &Vehs[sr_prev_order_unit];
                uint8_t prev_cur_order = prev_veh->order;
                bool was_tf = (sr_prev_order >= ORDER_FARM && sr_prev_order < ORDER_MOVE_TO);
                bool now_tf = (prev_cur_order >= ORDER_FARM && prev_cur_order < ORDER_MOVE_TO);
                if (!was_tf && now_tf) {
                    const char* tf_name = get_terraform_name(prev_veh);
                    if (tf_name) {
                        int rate = Terraform[prev_cur_order - ORDER_FARM].rate;
                        char tf_buf[128];
                        snprintf(tf_buf, sizeof(tf_buf),
                            loc(SR_TERRAFORM_ORDER), tf_name, rate);
                        sr_debug_log("TERRAFORM-ORDER-ON-SWITCH: %s", tf_buf);
                        sr_output(tf_buf, true);
                    }
                }
            }
            // Reset terraform tracking on unit change
            sr_prev_order = (cur_unit >= 0 && cur_unit < *VehCount)
                ? Vehs[cur_unit].order : 0;
            sr_prev_order_unit = cur_unit;
            sr_prev_unit = cur_unit;
            sr_unit_skipped = false;
        }
        // Terraform order change detection (same unit, order changed)
        if (cur_unit >= 0 && cur_unit < *VehCount
            && cur_unit == sr_prev_order_unit) {
            VEH* veh = &Vehs[cur_unit];
            uint8_t cur_order = veh->order;
            bool prev_is_tf = (sr_prev_order >= ORDER_FARM && sr_prev_order < ORDER_MOVE_TO);
            bool cur_is_tf = (cur_order >= ORDER_FARM && cur_order < ORDER_MOVE_TO);

            if (!prev_is_tf && cur_is_tf) {
                // Terraform order just given
                const char* tf_name = get_terraform_name(veh);
                if (tf_name) {
                    int rate = Terraform[cur_order - ORDER_FARM].rate;
                    char tf_buf[128];
                    snprintf(tf_buf, sizeof(tf_buf),
                        loc(SR_TERRAFORM_ORDER), tf_name, rate);
                    sr_debug_log("TERRAFORM-ORDER: %s", tf_buf);
                    sr_output(tf_buf, true);
                }
            } else if (prev_is_tf && !cur_is_tf) {
                // Terraform just completed
                int prev_idx = sr_prev_order - ORDER_FARM;
                MAP* sq = mapsq(veh->x, veh->y);
                const char* raw = (sq && is_ocean(sq))
                    ? Terraform[prev_idx].name_sea
                    : Terraform[prev_idx].name;
                if (raw && raw[0]) {
                    char tf_buf[128];
                    snprintf(tf_buf, sizeof(tf_buf),
                        loc(SR_TERRAFORM_COMPLETE), sr_game_str(raw));
                    sr_debug_log("TERRAFORM-DONE: %s", tf_buf);
                    sr_output(tf_buf, true);
                }
            }
            sr_prev_order = cur_order;
        }
    } else {
        sr_prev_unit = -1;
        sr_prev_order_unit = -1;
    }

    // Trigger 4: World map important message announce
    bool sr_modal_active = SocialEngHandler::IsActive()
        || PrefsHandler::IsActive()
        || SpecialistHandler::IsActive();
    if (on_world_map && !sr_modal_active) {
        if (sr_worldmap_poll_time == 0) sr_worldmap_poll_time = now;
        // Suppress all worldmap announcements while tour window is open
        if (Win_is_visible(TutWin)) {
            sr_worldmap_poll_time = now;
        // Suppress worldmap announcements for 3s after tutorial popup
        } else if (sr_tutorial_announce_time > 0
            && (now - sr_tutorial_announce_time) < 3000) {
            sr_worldmap_poll_time = now;
        } else if ((now - sr_worldmap_poll_time) >= 500) {
            sr_worldmap_poll_time = now;
            int count = sr_item_count();
            if (count > 0) {
                char buf[2048];
                int pos = 0;
                bool has_new = false;
                for (int i = 0; i < count; i++) {
                    const char* it = sr_item_get(i);
                    if (!it || strlen(it) < 3) continue;
                    // Whitelist: only individually important messages
                    bool important = false;
                    // English patterns
                    if (strncmp(it, "ABOUT ", 6) == 0) important = true;
                    if (strstr(it, "need new orders") != NULL) important = true;
                    if (strstr(it, "Press ENTER") != NULL) important = true;
                    // German patterns
                    if (strncmp(it, "\xc3\x9c" "BER ", 6) == 0) important = true; // "ÜBER "
                    if (strstr(it, "neue Befehle") != NULL) important = true;
                    if (strstr(it, "Eingabe dr") != NULL) important = true; // "Eingabe drücken"
                    if (!important) continue;
                    if (strstr(sr_announced, it) != NULL) continue;
                    has_new = true;
                    int len = strlen(it);
                    if (pos + len + 3 < (int)sizeof(buf)) {
                        if (pos > 0) {
                            buf[pos++] = '.';
                            buf[pos++] = ' ';
                        }
                        memcpy(buf + pos, it, len);
                        pos += len;
                    }
                }
                buf[pos] = '\0';
                if (has_new) {
                    // Append to sr_announced
                    int al = strlen(sr_announced);
                    int bl = strlen(buf);
                    if (al + bl + 3 < sr_announced_size) {
                        if (al > 0) {
                            sr_announced[al++] = '.';
                            sr_announced[al++] = ' ';
                        }
                        memcpy(sr_announced + al, buf, bl + 1);
                    }
                    sr_debug_log("ANNOUNCE-WORLDMAP: %s", buf);
                    sr_output(buf, true);
                }
            }
        }
    } else {
        sr_worldmap_poll_time = 0;
    }
}


bool HandleKey(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!sr_is_available() || !MapWin) return false;

    // When a popup dialog is active (commlink, diplomacy, etc.),
    // let all keys pass through to the game for popup navigation
    if (sr_popup_is_active()) return false;

    GameWinState cur_win = current_window();
    bool on_world_map = (cur_win == GW_World
        || (cur_win == GW_None && *PopupDialogState >= 2));
    if (!on_world_map) return false;
    if (msg != WM_KEYDOWN) return false;

    // Scanner: Ctrl+Left/Right = jump, Ctrl+PgUp/PgDn = filter
    if (ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && *MapAreaX > 0 && *MapAreaY > 0
        && (wParam == VK_LEFT || wParam == VK_RIGHT
            || wParam == VK_PRIOR || wParam == VK_NEXT)) {
        int faction = *CurrentPlayerFaction;

        if (wParam == VK_NEXT) {
            sr_scan_filter = (sr_scan_filter + 1) % SR_SCAN_FILTER_COUNT;
            sr_output(loc(sr_scan_filter_names[sr_scan_filter]), true);
            return true;
        }
        if (wParam == VK_PRIOR) {
            sr_scan_filter = (sr_scan_filter + SR_SCAN_FILTER_COUNT - 1) % SR_SCAN_FILTER_COUNT;
            sr_output(loc(sr_scan_filter_names[sr_scan_filter]), true);
            return true;
        }

        init_cursor_if_needed();

        int sx = sr_cursor_x;
        int sy = sr_cursor_y;
        int cx = sx;
        int cy = sy;
        bool found = false;

        if (wParam == VK_RIGHT) {
            while (sr_scan_next_tile(&cx, &cy, sx, sy)) {
                if (sr_scan_matches(cx, cy, sr_scan_filter, faction)) {
                    found = true;
                    break;
                }
            }
        } else {
            while (sr_scan_prev_tile(&cx, &cy, sx, sy)) {
                if (sr_scan_matches(cx, cy, sr_scan_filter, faction)) {
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            sr_cursor_x = cx;
            sr_cursor_y = cy;
            sr_announce_tile(sr_cursor_x, sr_cursor_y);
        } else {
            sr_output(loc(SR_SCAN_NOT_FOUND), true);
        }
        return true;
    }

    // Arrow keys: exploration + Shift+movement
    // When targeting mode or a popup dialog is active, let arrows pass to the game
    if (!sr_targeting_active && !sr_popup_is_active()
        && *MapAreaX > 0 && *MapAreaY > 0
        && (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT
            || wParam == VK_HOME || wParam == VK_PRIOR || wParam == VK_END || wParam == VK_NEXT)
        && !ctrl_key_down()) {
        // Direction offsets for SMAC diamond grid
        static const int dir_dx[] = { 1, 2, 1, 0, -1, -2, -1, 0 };
        static const int dir_dy[] = { -1, 0, 1, 2, 1, 0, -1, -2 };
        int dir = -1;
        if (wParam == VK_UP)          dir = 7; // N
        else if (wParam == VK_DOWN)   dir = 3; // S
        else if (wParam == VK_LEFT)   dir = 5; // W
        else if (wParam == VK_RIGHT)  dir = 1; // E
        else if (wParam == VK_HOME)   dir = 6; // NW
        else if (wParam == VK_PRIOR)  dir = 0; // NE (PgUp)
        else if (wParam == VK_END)    dir = 4; // SW
        else if (wParam == VK_NEXT)   dir = 2; // SE (PgDn)

        if (shift_key_down()) {
            // Shift+key = move unit to adjacent tile
            int veh_id = MapWin->iUnit;
            if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
                VEH* veh = &Vehs[veh_id];
                if (veh->faction_id == MapWin->cOwner) {
                    int tx = wrap(veh->x + dir_dx[dir]);
                    int ty = veh->y + dir_dy[dir];
                    if (ty >= 0 && ty < *MapAreaY) {
                        int old_x = veh->x;
                        int old_y = veh->y;
                        sr_debug_log("UNIT-MOVE %s (%d,%d)->(%d,%d) dir=%d",
                            sr_game_str(veh->name()), old_x, old_y, tx, ty, dir);
                        set_move_to(veh_id, tx, ty);
                        action(veh_id);
                        if (veh_id < *VehCount
                            && (veh->x != old_x || veh->y != old_y)) {
                            sr_cursor_x = veh->x;
                            sr_cursor_y = veh->y;
                            sr_announce_tile(sr_cursor_x, sr_cursor_y);
                            int total_speed = veh_speed(veh_id, 0);
                            int remaining = max(0, total_speed - (int)veh->moves_spent);
                            int disp_rem = remaining / Rules->move_rate_roads;
                            int disp_tot = total_speed / Rules->move_rate_roads;
                            char move_buf[64];
                            snprintf(move_buf, sizeof(move_buf),
                                loc(SR_MOVEMENT_POINTS), disp_rem, disp_tot);
                            sr_output(move_buf, false);
                        } else {
                            sr_output(loc(SR_CANNOT_MOVE), true);
                        }
                    } else {
                        sr_output(loc(SR_MAP_EDGE), true);
                    }
                }
            }
            return true;
        } else {
            // No shift = explore: move virtual cursor
            init_cursor_if_needed();
            int nx = sr_cursor_x + dir_dx[dir];
            int ny = sr_cursor_y + dir_dy[dir];
            nx = wrap(nx);
            if (ny >= 0 && ny < *MapAreaY) {
                sr_cursor_x = nx;
                sr_cursor_y = ny;
                sr_announce_tile(sr_cursor_x, sr_cursor_y);
            } else {
                sr_output(loc(SR_MAP_EDGE), true);
            }
        }
        return true;
    }

    // Space without modifier = skip unit (let game handle, but set flag for announcement)
    if (wParam == VK_SPACE && !ctrl_key_down() && !shift_key_down() && !alt_key_down()) {
        sr_unit_skipped = true;
        sr_debug_log("SPACE-SKIP: flag set");
        return false; // let game handle the skip
    }

    // TODO: V = cycle through units in stack
    // Problem: WinProc call recurses into ModWinProc, so V never reaches the game.
    // Solution: use return false approach (like Space/skip) and detect unit change
    // in OnTimer poll. Needs: track previous unit, announce on change when V was pressed.
    // See debug: V-KEY-RECEIVED confirms WM_KEYDOWN arrives, but iUnit doesn't change.

    // Ctrl+Space = jump exploration cursor back to selected unit
    if (wParam == VK_SPACE && ctrl_key_down() && !shift_key_down() && !alt_key_down()) {
        int veh_id = MapWin->iUnit;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            VEH* veh = &Vehs[veh_id];
            sr_cursor_x = veh->x;
            sr_cursor_y = veh->y;
            sr_output(loc(SR_CURSOR_TO_UNIT), true);
            sr_announce_tile(sr_cursor_x, sr_cursor_y);
            sr_debug_log("CURSOR-TO-UNIT (%d,%d)", sr_cursor_x, sr_cursor_y);
        } else {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
        }
        return true;
    }

    // Numpad movement: let game handle it, then announce result
    if (!ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && !sr_targeting_active
        && (wParam == VK_NUMPAD1 || wParam == VK_NUMPAD2 || wParam == VK_NUMPAD3
            || wParam == VK_NUMPAD4 || wParam == VK_NUMPAD6
            || wParam == VK_NUMPAD7 || wParam == VK_NUMPAD8 || wParam == VK_NUMPAD9)) {
        int veh_id = MapWin->iUnit;
        int old_x = -1, old_y = -1;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            old_x = Vehs[veh_id].x;
            old_y = Vehs[veh_id].y;
        }
        // Pass to game for actual movement
        WinProc(hwnd, msg, wParam, lParam);
        // Re-read unit state (veh_id may have changed after action)
        veh_id = MapWin->iUnit;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            VEH* veh = &Vehs[veh_id];
            if (veh->x != old_x || veh->y != old_y) {
                sr_cursor_x = veh->x;
                sr_cursor_y = veh->y;
                sr_announce_tile(sr_cursor_x, sr_cursor_y);
                int total_speed = veh_speed(veh_id, 0);
                int remaining = max(0, total_speed - (int)veh->moves_spent);
                int disp_rem = remaining / Rules->move_rate_roads;
                int disp_tot = total_speed / Rules->move_rate_roads;
                char move_buf[64];
                snprintf(move_buf, sizeof(move_buf),
                    loc(SR_MOVEMENT_POINTS), disp_rem, disp_tot);
                sr_output(move_buf, false);
            } else {
                sr_output(loc(SR_CANNOT_MOVE), true);
            }
        }
        return true;
    }

    // Shift+Space = send unit to cursor position (Go To)
    if (wParam == VK_SPACE && shift_key_down()
        && sr_cursor_x >= 0 && sr_cursor_y >= 0) {
        int veh_id = MapWin->iUnit;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            VEH* veh = &Vehs[veh_id];
            if (veh->faction_id == MapWin->cOwner) {
                set_move_to(veh_id, sr_cursor_x, sr_cursor_y);
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_UNIT_MOVES_TO),
                    sr_game_str(veh->name()), sr_cursor_x, sr_cursor_y);
                sr_debug_log("GO-TO: %s", buf);
                sr_output(buf, true);
                mod_veh_skip(veh_id);
            }
        }
        return true;
    }

    // Shift+A = automate former (accessible modal)
    if (wParam == 'A' && shift_key_down() && !ctrl_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        int veh_id = MapWin->iUnit;
        if (veh_id < 0 || !Vehs || *VehCount <= 0 || veh_id >= *VehCount) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }
        VEH* veh = &Vehs[veh_id];
        if (!veh->is_former()) {
            // Non-formers: pass through to game for default auto behavior
            sr_output(loc(SR_AUTO_NOT_FORMER), true);
            return false;
        }

        // Automation options matching VehOrderAutoType 0-7
        static const SrStr auto_labels[] = {
            SR_AUTO_FULL, SR_AUTO_ROAD, SR_AUTO_MAGTUBE,
            SR_AUTO_IMPROVE_BASE, SR_AUTO_FARM_SOLAR, SR_AUTO_FARM_MINE,
            SR_AUTO_FUNGUS, SR_AUTO_SENSOR,
        };
        int total = 8;
        int index = 0;
        bool want_close = false;
        bool confirmed = false;

        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_AUTO_OPEN), total);
        sr_output(buf, true);
        snprintf(buf, sizeof(buf), loc(SR_AUTO_ITEM),
            1, total, loc(auto_labels[0]));
        sr_output(buf, false);

        MSG modal_msg;
        while (!want_close) {
            if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
                if (modal_msg.message == WM_QUIT) {
                    PostQuitMessage((int)modal_msg.wParam);
                    break;
                }
                if (modal_msg.message == WM_KEYDOWN) {
                    WPARAM k = modal_msg.wParam;
                    if (k == VK_ESCAPE) {
                        want_close = true;
                    } else if (k == VK_RETURN) {
                        want_close = true;
                        confirmed = true;
                    } else if (k == VK_UP) {
                        index = (index - 1 + total) % total;
                        snprintf(buf, sizeof(buf), loc(SR_AUTO_ITEM),
                            index + 1, total, loc(auto_labels[index]));
                        sr_output(buf, true);
                    } else if (k == VK_DOWN) {
                        index = (index + 1) % total;
                        snprintf(buf, sizeof(buf), loc(SR_AUTO_ITEM),
                            index + 1, total, loc(auto_labels[index]));
                        sr_output(buf, true);
                    }
                }
            } else {
                Sleep(10);
            }
        }

        // Drain leftover key messages
        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

        if (confirmed) {
            veh->order_auto_type = (uint8_t)index;
            snprintf(buf, sizeof(buf), loc(SR_AUTO_CONFIRM),
                loc(auto_labels[index]));
            sr_debug_log("AUTOMATE: veh %d type %d (%s)",
                veh_id, index, loc(auto_labels[index]));
            sr_output(buf, true);
            mod_veh_skip(veh_id);
        } else {
            sr_output(loc(SR_AUTO_CANCEL), true);
        }
        return true;
    }

    // Escape during targeting → cancel
    if (wParam == VK_ESCAPE && sr_targeting_active) {
        sr_targeting_active = false;
        sr_output(loc(SR_TARGETING_CANCEL), true);
        sr_debug_log("TARGETING: cancelled via Escape");
        WinProc(hwnd, msg, wParam, lParam);
        return true;
    }

    // Enter during targeting → confirm
    if (wParam == VK_RETURN && sr_targeting_active) {
        sr_targeting_active = false;
        sr_debug_log("TARGETING: confirmed via Enter");
        WinProc(hwnd, msg, wParam, lParam);
        return true;
    }

    // J → accessible group go-to base list
    if (wParam == 'J' && !ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        int veh_id = MapWin->iUnit;
        if (veh_id < 0 || !Vehs || *VehCount <= 0 || veh_id >= *VehCount) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }
        VEH* veh = &Vehs[veh_id];
        int faction = veh->faction_id;
        int tile_x = veh->x;
        int tile_y = veh->y;

        // Count units at the current tile belonging to same faction
        std::vector<int> unit_ids;
        int iter_id = veh_at(tile_x, tile_y);
        while (iter_id >= 0 && iter_id < *VehCount) {
            if (Vehs[iter_id].faction_id == faction) {
                unit_ids.push_back(iter_id);
            }
            iter_id = Vehs[iter_id].next_veh_id_stack;
        }
        if (unit_ids.empty()) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }

        // Build sorted base list
        struct BaseEntry { int id; int dist; };
        std::vector<BaseEntry> bases;
        for (int i = 0; i < *BaseCount; i++) {
            if (Bases[i].faction_id == faction) {
                int d = map_range(tile_x, tile_y, Bases[i].x, Bases[i].y);
                bases.push_back({i, d});
            }
        }
        if (bases.empty()) {
            sr_output(loc(SR_BASE_LIST_EMPTY), true);
            return true;
        }
        std::sort(bases.begin(), bases.end(),
            [](const BaseEntry& a, const BaseEntry& b) { return a.dist < b.dist; });

        int total = (int)bases.size();
        int unit_count = (int)unit_ids.size();
        int index = 0;
        bool want_close = false;
        bool confirmed = false;

        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_GROUP_GO_TO_BASE), unit_count, total);
        sr_output(buf, true);

        {
            BASE* b = &Bases[bases[0].id];
            snprintf(buf, sizeof(buf), loc(SR_BASE_LIST_FMT),
                1, total, sr_game_str(b->name), b->x, b->y, bases[0].dist);
            sr_output(buf, false);
        }

        MSG modal_msg;
        while (!want_close) {
            if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
                if (modal_msg.message == WM_QUIT) {
                    PostQuitMessage((int)modal_msg.wParam);
                    break;
                }
                if (modal_msg.message == WM_KEYDOWN) {
                    WPARAM k = modal_msg.wParam;
                    if (k == VK_ESCAPE) {
                        want_close = true;
                    } else if (k == VK_RETURN) {
                        want_close = true;
                        confirmed = true;
                    } else if (k == VK_UP) {
                        index = (index - 1 + total) % total;
                        BASE* b = &Bases[bases[index].id];
                        snprintf(buf, sizeof(buf), loc(SR_BASE_LIST_FMT),
                            index + 1, total, sr_game_str(b->name), b->x, b->y, bases[index].dist);
                        sr_output(buf, true);
                    } else if (k == VK_DOWN) {
                        index = (index + 1) % total;
                        BASE* b = &Bases[bases[index].id];
                        snprintf(buf, sizeof(buf), loc(SR_BASE_LIST_FMT),
                            index + 1, total, sr_game_str(b->name), b->x, b->y, bases[index].dist);
                        sr_output(buf, true);
                    }
                }
            } else {
                Sleep(10);
            }
        }

        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

        if (confirmed && index >= 0 && index < total) {
            BASE* b = &Bases[bases[index].id];
            // Send all units at the tile to the selected base
            for (int i = 0; i < (int)unit_ids.size(); i++) {
                set_move_to(unit_ids[i], b->x, b->y);
            }
            snprintf(buf, sizeof(buf), loc(SR_GROUP_GOING_TO_BASE),
                unit_count, sr_game_str(b->name));
            sr_output(buf, true);
            sr_debug_log("GROUP-GO-TO: %d units to %s (%d,%d)",
                unit_count, sr_game_str(b->name), b->x, b->y);
            mod_veh_skip(veh_id);
        } else {
            sr_output(loc(SR_TARGETING_CANCEL), true);
        }
        return true;
    }

    // I → accessible airdrop base selection
    if (wParam == 'I' && !ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        int veh_id = MapWin->iUnit;
        if (veh_id < 0 || !Vehs || *VehCount <= 0 || veh_id >= *VehCount) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }
        VEH* veh = &Vehs[veh_id];
        MAP* cur_sq = mapsq(veh->x, veh->y);
        if (!cur_sq || !can_airdrop(veh_id, cur_sq)) {
            sr_output(loc(SR_AIRDROP_NO_ABILITY), true);
            return true;
        }

        int faction = veh->faction_id;
        bool combat = veh->is_combat_unit();
        int max_range = drop_range(faction);

        // Build list of valid airdrop target bases (own + all others in range)
        struct DropTarget { int base_id; int dist; };
        std::vector<DropTarget> targets;
        for (int i = 0; i < *BaseCount; i++) {
            BASE* b = &Bases[i];
            int d = map_range(veh->x, veh->y, b->x, b->y);
            if (d > max_range) continue;
            MAP* sq = mapsq(b->x, b->y);
            if (!sq || is_ocean(sq)) continue;
            if (!allow_airdrop(b->x, b->y, faction, combat, sq)) continue;
            targets.push_back({i, d});
        }
        if (targets.empty()) {
            sr_output(loc(SR_AIRDROP_BLOCKED), true);
            return true;
        }
        std::sort(targets.begin(), targets.end(),
            [](const DropTarget& a, const DropTarget& b) { return a.dist < b.dist; });

        int total = (int)targets.size();
        int index = 0;
        bool want_close = false;
        bool confirmed = false;
        bool cursor_mode = false;

        // Check if cursor position is a valid drop target
        int cx = GetCursorX();
        int cy = GetCursorY();
        int cursor_dist = map_range(veh->x, veh->y, cx, cy);
        bool cursor_valid = false;
        if (cx >= 0 && cy >= 0 && cursor_dist <= max_range) {
            MAP* csq = mapsq(cx, cy);
            if (csq && !is_ocean(csq) && allow_airdrop(cx, cy, faction, combat, csq)) {
                cursor_valid = true;
            }
        }

        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_AIRDROP_BASE), total);
        sr_output(buf, true);

        {
            BASE* b = &Bases[targets[0].base_id];
            snprintf(buf, sizeof(buf), loc(SR_AIRDROP_BASE_FMT),
                1, total, sr_game_str(b->name), b->x, b->y, targets[0].dist);
            sr_output(buf, false);
        }

        MSG modal_msg;
        while (!want_close) {
            if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
                if (modal_msg.message == WM_QUIT) {
                    PostQuitMessage((int)modal_msg.wParam);
                    break;
                }
                if (modal_msg.message == WM_KEYDOWN) {
                    WPARAM k = modal_msg.wParam;
                    if (k == VK_ESCAPE) {
                        want_close = true;
                    } else if (k == VK_RETURN) {
                        want_close = true;
                        confirmed = true;
                    } else if (k == VK_UP || k == VK_DOWN) {
                        cursor_mode = false;
                        if (k == VK_UP) {
                            index = (index - 1 + total) % total;
                        } else {
                            index = (index + 1) % total;
                        }
                        BASE* b = &Bases[targets[index].base_id];
                        snprintf(buf, sizeof(buf), loc(SR_AIRDROP_BASE_FMT),
                            index + 1, total, sr_game_str(b->name), b->x, b->y, targets[index].dist);
                        sr_output(buf, true);
                    } else if (k == 'C') {
                        // C = select cursor position as drop target
                        if (cursor_valid) {
                            cursor_mode = true;
                            snprintf(buf, sizeof(buf), loc(SR_AIRDROP_CURSOR),
                                cx, cy, cursor_dist);
                            sr_output(buf, true);
                        } else {
                            sr_output(loc(SR_AIRDROP_CURSOR_INVALID), true);
                        }
                    }
                }
            } else {
                Sleep(10);
            }
        }

        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

        if (confirmed && cursor_mode && cursor_valid) {
            action_airdrop(veh_id, cx, cy, 3);
            snprintf(buf, sizeof(buf), loc(SR_AIRDROP_CURSOR_CONFIRM), cx, cy);
            sr_output(buf, true);
            sr_debug_log("AIRDROP: %s to cursor (%d,%d)",
                sr_game_str(veh->name()), cx, cy);
            mod_veh_skip(veh_id);
        } else if (confirmed && !cursor_mode && index >= 0 && index < total) {
            BASE* b = &Bases[targets[index].base_id];
            action_airdrop(veh_id, b->x, b->y, 3);
            snprintf(buf, sizeof(buf), loc(SR_AIRDROP_CONFIRM), sr_game_str(b->name));
            sr_output(buf, true);
            sr_debug_log("AIRDROP: %s to %s (%d,%d)",
                sr_game_str(veh->name()), sr_game_str(b->name), b->x, b->y);
            mod_veh_skip(veh_id);
        } else {
            sr_output(loc(SR_TARGETING_CANCEL), true);
        }
        return true;
    }

    // F (Long range fire) → accessible artillery modal
    if (wParam == 'F' && !ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        int veh_id = MapWin->iUnit;
        if (veh_id < 0 || !Vehs || *VehCount <= 0 || veh_id >= *VehCount) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }
        VEH* veh = &Vehs[veh_id];
        if (!can_arty(veh->unit_id, true)) {
            sr_output(loc(SR_ARTY_CANNOT), true);
            return true;
        }
        if (!veh_ready(veh_id)) {
            sr_output(loc(SR_ARTY_CANNOT), true);
            return true;
        }

        int faction = veh->faction_id;
        int max_range = arty_range(veh->unit_id);

        // Build target list: enemy units and bases within range
        struct ArtyTarget {
            int x, y, dist;
            bool is_base;
            int id; // veh_id or base_id
        };
        std::vector<ArtyTarget> targets;

        // Scan enemy vehicles
        for (int i = 0; i < *VehCount; i++) {
            VEH* v = &Vehs[i];
            if (v->faction_id == faction || has_pact(faction, v->faction_id)) continue;
            int d = map_range(veh->x, veh->y, v->x, v->y);
            if (d < 1 || d > max_range) continue;
            // Only add top-of-stack unit at each location
            if (stack_fix(veh_at(v->x, v->y)) != i) continue;
            targets.push_back({v->x, v->y, d, false, i});
        }

        // Scan enemy bases
        for (int i = 0; i < *BaseCount; i++) {
            BASE* b = &Bases[i];
            if (b->faction_id == faction || has_pact(faction, b->faction_id)) continue;
            int d = map_range(veh->x, veh->y, b->x, b->y);
            if (d < 1 || d > max_range) continue;
            // Check if there's already a unit target at this location
            bool has_unit = false;
            for (size_t j = 0; j < targets.size(); j++) {
                if (targets[j].x == b->x && targets[j].y == b->y) {
                    has_unit = true;
                    break;
                }
            }
            if (!has_unit) {
                targets.push_back({b->x, b->y, d, true, i});
            }
        }

        if (targets.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), loc(SR_ARTY_NO_TARGETS), max_range);
            sr_output(buf, true);
            return true;
        }

        std::sort(targets.begin(), targets.end(),
            [](const ArtyTarget& a, const ArtyTarget& b) { return a.dist < b.dist; });

        int total = (int)targets.size();
        int index = 0;
        bool want_close = false;
        bool confirmed = false;
        bool cursor_mode = false;
        char buf[512];

        // Build target name for announcement
        auto target_name = [&](const ArtyTarget& t) -> const char* {
            static char namebuf[128];
            if (t.is_base) {
                BASE* b = &Bases[t.id];
                snprintf(namebuf, sizeof(namebuf), "%s (%s)",
                    sr_game_str(b->name),
                    sr_game_str(MFactions[b->faction_id].noun_faction));
            } else {
                VEH* v = &Vehs[t.id];
                snprintf(namebuf, sizeof(namebuf), "%s (%s)",
                    sr_game_str(v->name()),
                    sr_game_str(MFactions[v->faction_id].noun_faction));
            }
            return namebuf;
        };

        // Announce open
        snprintf(buf, sizeof(buf), loc(SR_ARTY_OPEN), total, max_range);
        sr_output(buf, true);

        // Announce first target
        snprintf(buf, sizeof(buf), loc(SR_ARTY_TARGET_FMT),
            1, total, target_name(targets[0]),
            targets[0].x, targets[0].y, targets[0].dist);
        sr_output(buf, false);

        MSG modal_msg;
        while (!want_close) {
            if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
                if (modal_msg.message == WM_QUIT) {
                    PostQuitMessage((int)modal_msg.wParam);
                    break;
                }
                if (modal_msg.message == WM_KEYDOWN) {
                    WPARAM k = modal_msg.wParam;
                    if (k == VK_ESCAPE) {
                        want_close = true;
                    } else if (k == VK_RETURN) {
                        want_close = true;
                        confirmed = true;
                    } else if (k == VK_UP || k == VK_DOWN) {
                        cursor_mode = false;
                        if (k == VK_UP) {
                            index = (index - 1 + total) % total;
                        } else {
                            index = (index + 1) % total;
                        }
                        snprintf(buf, sizeof(buf), loc(SR_ARTY_TARGET_FMT),
                            index + 1, total, target_name(targets[index]),
                            targets[index].x, targets[index].y, targets[index].dist);
                        sr_output(buf, true);
                    } else if (k == 'C') {
                        cursor_mode = true;
                        int cx = GetCursorX();
                        int cy = GetCursorY();
                        int cd = map_range(veh->x, veh->y, cx, cy);
                        if (cd >= 1 && cd <= max_range) {
                            snprintf(buf, sizeof(buf), loc(SR_ARTY_CURSOR),
                                cx, cy, cd);
                        } else {
                            snprintf(buf, sizeof(buf), loc(SR_ARTY_CURSOR_OOR),
                                cx, cy, cd, max_range);
                        }
                        sr_output(buf, true);
                    } else if (k == VK_F1) {
                        sr_output(loc(SR_ARTY_HELP), true);
                    }
                }
            } else {
                Sleep(10);
            }
        }

        // Drain leftover messages
        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

        if (confirmed) {
            int tx, ty;
            const char* tname;
            if (cursor_mode) {
                tx = GetCursorX();
                ty = GetCursorY();
                int cd = map_range(veh->x, veh->y, tx, ty);
                if (cd < 1 || cd > max_range) {
                    sr_output(loc(SR_TARGETING_CANCEL), true);
                    return true;
                }
                char cname[64];
                snprintf(cname, sizeof(cname), "(%d, %d)", tx, ty);
                snprintf(buf, sizeof(buf), loc(SR_ARTY_FIRED), cname);
                sr_output(buf, true);
            } else {
                tx = targets[index].x;
                ty = targets[index].y;
                tname = target_name(targets[index]);
                snprintf(buf, sizeof(buf), loc(SR_ARTY_FIRED), tname);
                sr_output(buf, true);
            }
            sr_debug_log("ARTILLERY: %s fires at (%d,%d)",
                sr_game_str(veh->name()), tx, ty);
            mod_action_arty(veh_id, tx, ty);
        } else {
            sr_output(loc(SR_TARGETING_CANCEL), true);
        }
        return true;
    }

    // P (Patrol) → patrol to exploration cursor position (with confirmation)
    if (wParam == 'P' && !ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        int veh_id = MapWin->iUnit;
        if (veh_id < 0 || !Vehs || *VehCount <= 0 || veh_id >= *VehCount) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }
        VEH* veh = &Vehs[veh_id];
        if (veh->faction_id != MapWin->cOwner) {
            sr_output(loc(SR_PATROL_CANNOT), true);
            return true;
        }
        init_cursor_if_needed();
        int tx = sr_cursor_x;
        int ty = sr_cursor_y;
        if (tx < 0 || ty < 0 || (tx == veh->x && ty == veh->y)) {
            sr_output(loc(SR_PATROL_CANNOT), true);
            return true;
        }
        // Ask for confirmation
        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_PATROL_PROMPT), tx, ty);
        sr_output(buf, true);
        bool confirmed = false;
        MSG modal_msg;
        bool wait = true;
        while (wait) {
            if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
                if (modal_msg.message == WM_QUIT) {
                    PostQuitMessage((int)modal_msg.wParam);
                    break;
                }
                if (modal_msg.message == WM_KEYDOWN) {
                    if (modal_msg.wParam == VK_RETURN) {
                        confirmed = true;
                        wait = false;
                    } else if (modal_msg.wParam == VK_ESCAPE) {
                        wait = false;
                    }
                }
            } else {
                Sleep(10);
            }
        }
        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}
        if (confirmed) {
            action_patrol(veh_id, tx, ty);
            snprintf(buf, sizeof(buf), loc(SR_PATROL_CONFIRM), tx, ty);
            sr_output(buf, true);
            sr_debug_log("PATROL: %s to (%d,%d)", sr_game_str(veh->name()), tx, ty);
        } else {
            sr_output(loc(SR_TARGETING_CANCEL), true);
        }
        return true;
    }

    // Ctrl+R (Road to), Ctrl+T (Tube to) → build to cursor position (with confirmation)
    if (ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World
        && (wParam == 'R' || wParam == 'T')) {
        bool is_tube = (wParam == 'T');
        int veh_id = MapWin->iUnit;
        if (veh_id < 0 || !Vehs || *VehCount <= 0 || veh_id >= *VehCount) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }
        VEH* veh = &Vehs[veh_id];
        if (veh->faction_id != MapWin->cOwner || !veh->is_former()) {
            sr_output(loc(SR_ROAD_TO_CANNOT), true);
            return true;
        }
        init_cursor_if_needed();
        int tx = sr_cursor_x;
        int ty = sr_cursor_y;
        if (tx < 0 || ty < 0 || (tx == veh->x && ty == veh->y)) {
            sr_output(loc(SR_ROAD_TO_CANNOT), true);
            return true;
        }
        // Ask for confirmation
        char buf[256];
        snprintf(buf, sizeof(buf),
            loc(is_tube ? SR_TUBE_TO_PROMPT : SR_ROAD_TO_PROMPT), tx, ty);
        sr_output(buf, true);
        bool confirmed = false;
        MSG modal_msg;
        bool wait = true;
        while (wait) {
            if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
                if (modal_msg.message == WM_QUIT) {
                    PostQuitMessage((int)modal_msg.wParam);
                    break;
                }
                if (modal_msg.message == WM_KEYDOWN) {
                    if (modal_msg.wParam == VK_RETURN) {
                        confirmed = true;
                        wait = false;
                    } else if (modal_msg.wParam == VK_ESCAPE) {
                        wait = false;
                    }
                }
            } else {
                Sleep(10);
            }
        }
        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}
        if (confirmed) {
            if (is_tube) {
                veh->waypoint_x[0] = tx;
                veh->waypoint_y[0] = ty;
                veh->order = ORDER_MAGTUBE_TO;
                veh->status_icon = 'T';
            } else {
                set_road_to(veh_id, tx, ty);
            }
            snprintf(buf, sizeof(buf),
                loc(is_tube ? SR_TUBE_TO_CONFIRM : SR_ROAD_TO_CONFIRM), tx, ty);
            sr_output(buf, true);
            sr_debug_log("%s: %s to (%d,%d)",
                is_tube ? "TUBE_TO" : "ROAD_TO",
                sr_game_str(veh->name()), tx, ty);
            mod_veh_skip(veh_id);
        } else {
            sr_output(loc(SR_TARGETING_CANCEL), true);
        }
        return true;
    }

    // Shift+G → go to home base directly
    if (wParam == 'G' && shift_key_down() && !ctrl_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        int veh_id = MapWin->iUnit;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            VEH* veh = &Vehs[veh_id];
            int home_id = veh->home_base_id;
            if (home_id >= 0 && home_id < *BaseCount) {
                BASE* base = &Bases[home_id];
                set_move_to(veh_id, base->x, base->y);
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_GOING_HOME), sr_game_str(base->name));
                sr_output(buf, true);
                sr_debug_log("GO-HOME: %s to %s (%d,%d)",
                    sr_game_str(veh->name()), sr_game_str(base->name), base->x, base->y);
                mod_veh_skip(veh_id);
            } else {
                sr_output(loc(SR_BASE_LIST_EMPTY), true);
            }
        } else {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
        }
        return true;
    }

    // G → accessible base list for "Go to"
    if (wParam == 'G' && !ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        int veh_id = MapWin->iUnit;
        if (veh_id < 0 || !Vehs || *VehCount <= 0 || veh_id >= *VehCount) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }
        VEH* veh = &Vehs[veh_id];
        int faction = veh->faction_id;

        struct BaseEntry { int id; int dist; };
        std::vector<BaseEntry> bases;
        for (int i = 0; i < *BaseCount; i++) {
            if (Bases[i].faction_id == faction) {
                int d = map_range(veh->x, veh->y, Bases[i].x, Bases[i].y);
                bases.push_back({i, d});
            }
        }
        if (bases.empty()) {
            sr_output(loc(SR_BASE_LIST_EMPTY), true);
            return true;
        }
        std::sort(bases.begin(), bases.end(),
            [](const BaseEntry& a, const BaseEntry& b) { return a.dist < b.dist; });

        int total = (int)bases.size();
        int index = 0;
        bool want_close = false;
        bool confirmed = false;

        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_GO_TO_BASE), total);
        sr_output(buf, true);

        {
            BASE* b = &Bases[bases[0].id];
            snprintf(buf, sizeof(buf), loc(SR_BASE_LIST_FMT),
                1, total, sr_game_str(b->name), b->x, b->y, bases[0].dist);
            sr_output(buf, false);
        }

        MSG modal_msg;
        while (!want_close) {
            if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
                if (modal_msg.message == WM_QUIT) {
                    PostQuitMessage((int)modal_msg.wParam);
                    break;
                }
                if (modal_msg.message == WM_KEYDOWN) {
                    WPARAM k = modal_msg.wParam;
                    if (k == VK_ESCAPE) {
                        want_close = true;
                    } else if (k == VK_RETURN) {
                        want_close = true;
                        confirmed = true;
                    } else if (k == VK_UP) {
                        index = (index - 1 + total) % total;
                        BASE* b = &Bases[bases[index].id];
                        snprintf(buf, sizeof(buf), loc(SR_BASE_LIST_FMT),
                            index + 1, total, sr_game_str(b->name), b->x, b->y, bases[index].dist);
                        sr_output(buf, true);
                    } else if (k == VK_DOWN) {
                        index = (index + 1) % total;
                        BASE* b = &Bases[bases[index].id];
                        snprintf(buf, sizeof(buf), loc(SR_BASE_LIST_FMT),
                            index + 1, total, sr_game_str(b->name), b->x, b->y, bases[index].dist);
                        sr_output(buf, true);
                    }
                }
            } else {
                Sleep(10);
            }
        }

        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

        if (confirmed && index >= 0 && index < total) {
            BASE* b = &Bases[bases[index].id];
            set_move_to(veh_id, b->x, b->y);
            snprintf(buf, sizeof(buf), loc(SR_GOING_TO_BASE), sr_game_str(b->name));
            sr_output(buf, true);
            sr_debug_log("GO-TO-BASE: %s to %s (%d,%d)",
                sr_game_str(veh->name()), sr_game_str(b->name), b->x, b->y);
            mod_veh_skip(veh_id);
        } else {
            sr_output(loc(SR_TARGETING_CANCEL), true);
        }
        return true;
    }

    // V → let game toggle Move/View mode, then announce
    if (wParam == 'V' && !ctrl_key_down() && !shift_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        WinProc(hwnd, msg, wParam, lParam);
        if (MapWin->fUnitNotViewMode)
            sr_output(loc(SR_MODE_MOVE), true);
        else
            sr_output(loc(SR_MODE_VIEW), true);
        return true;
    }

    // E → open Social Engineering handler
    if (wParam == 'E' && !ctrl_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        SocialEngHandler::RunModal();
        return true;
    }

    // Ctrl+P → open Preferences handler
    if (wParam == 'P' && ctrl_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        PrefsHandler::RunModal();
        return true;
    }

    // Shift+D → open Design Workshop handler
    if (wParam == 'D' && shift_key_down() && !ctrl_key_down() && !alt_key_down()
        && !*GameHalted && cur_win == GW_World) {
        DesignHandler::RunModal();
        return true;
    }

    // Escape → read quit dialog text before game opens it
    if (wParam == VK_ESCAPE && !*GameHalted && cur_win == GW_World) {
        char buf[512];
        if (sr_read_popup_text("Script", "REALLYQUIT", buf, sizeof(buf))) {
            sr_output(buf, false);
        }
        WinProc(hwnd, msg, wParam, lParam);
        return true;
    }

    // Ctrl+F1 = context-sensitive help for current unit/terrain
    if (wParam == VK_F1 && ctrl_key_down() && !shift_key_down()
        && cur_win == GW_World) {
        char buf[1024];
        int pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_HELP_HEADER));

        int veh_id = MapWin->iUnit;
        VEH* veh = NULL;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            veh = &Vehs[veh_id];
        }

        // Unit-specific commands
        if (veh && veh->faction_id == MapWin->cOwner) {
            MAP* tile = mapsq(veh->x, veh->y);
            uint32_t items = tile ? tile->items : 0;

            if (veh->is_former()) {
                if (tile && tile->is_fungus()) {
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_REMOVE_FUNGUS));
                } else {
                    if (!(items & BIT_ROAD)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_ROAD));
                    } else if (!(items & BIT_MAGTUBE)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_MAGTUBE));
                    }
                    if (!(items & BIT_FARM)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_FARM));
                    }
                    if (!(items & BIT_MINE)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_MINE));
                    }
                    if (!(items & BIT_SOLAR)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_SOLAR));
                    }
                    if (!(items & BIT_FOREST)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_FOREST));
                    }
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_SENSOR));
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_CONDENSER));
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_BOREHOLE));
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_AIRBASE));
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_BUNKER));
                }
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_AUTOMATE));
            }
            if (veh->is_colony()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_BASE));
            }
            if (veh->is_combat_unit()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_ATTACK));
            }
            if (veh->is_probe()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_PROBE));
            }
            if (veh->is_supply()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_CONVOY));
            }
            if (veh->is_transport()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_UNLOAD));
            }
            if (veh->triad() != TRIAD_SEA && veh->triad() != TRIAD_AIR) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_AIRDROP));
            }

            if (tile && tile->is_base()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_OPEN_BASE));
            }
            if (items & BIT_MONOLITH) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_MONOLITH));
            }
            if (items & BIT_SUPPLY_POD) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_SUPPLY_POD));
            }
        }

        // Always-shown commands
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_MOVE));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_EXPLORE));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_SKIP));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_HOLD));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_GOTO));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_GO_TO_BASE));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_GO_HOME));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_GROUP_GOTO));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_PATROL));
        if (veh && veh->is_combat_unit()) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_ARTILLERY));
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_READ));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_CURSOR_TO_UNIT));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_SCAN_FILTER));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_SCAN_JUMP));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_STATUS_HELP));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_ACT_MENU_HELP));

        sr_debug_log("CONTEXT-HELP: %s", buf);
        sr_output(buf, true);
        return true;
    }

    // Shift+F10 = unit action menu
    if (wParam == VK_F10 && shift_key_down() && !ctrl_key_down()
        && cur_win == GW_World && !*GameHalted) {
        int veh_id = MapWin->iUnit;
        VEH* veh = NULL;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            veh = &Vehs[veh_id];
        }
        if (!veh || veh->faction_id != MapWin->cOwner) {
            sr_output(loc(SR_NO_UNIT_SELECTED), true);
            return true;
        }

        struct UnitAction {
            SrStr label;
            WPARAM vk;
            bool need_shift;
            bool need_ctrl;
            bool use_accessible;  // true=PostMessage (our handler), false=WinProc (game)
            SrStr confirm;        // SR_COUNT = no extra confirmation (feedback from other system)
            bool skip_after;      // call mod_veh_skip after WinProc (advance to next unit)
        };
        std::vector<UnitAction> actions;

        MAP* tile = mapsq(veh->x, veh->y);
        uint32_t items = tile ? tile->items : 0;

        // Always available
        // Skip: sr_unit_skipped flag gives "Skipped. Next unit:" via OnTimer
        actions.push_back({SR_ACT_SKIP, VK_SPACE, false, false, false, SR_COUNT, false});
        actions.push_back({SR_ACT_HOLD, 'H', false, false, false, SR_ACT_DONE_HOLD, true});
        actions.push_back({SR_ACT_EXPLORE, 'X', false, false, false, SR_ACT_DONE_EXPLORE, true});
        // G, P: our modals handle skip themselves
        actions.push_back({SR_ACT_GOTO_BASE, 'G', false, false, true, SR_COUNT, false});
        actions.push_back({SR_ACT_PATROL, 'P', false, false, true, SR_COUNT, false});

        // Former actions — terraform polling gives feedback, skip to next unit
        if (veh->is_former()) {
            if (tile && tile->is_fungus()) {
                actions.push_back({SR_ACT_REMOVE_FUNGUS, 'F', false, false, false, SR_COUNT, true});
            } else {
                if (!(items & BIT_ROAD)) {
                    actions.push_back({SR_ACT_BUILD_ROAD, 'R', false, false, false, SR_COUNT, true});
                } else if (!(items & BIT_MAGTUBE)) {
                    actions.push_back({SR_ACT_BUILD_MAGTUBE, 'R', false, false, false, SR_COUNT, true});
                }
                if (!(items & BIT_FARM)) {
                    actions.push_back({SR_ACT_BUILD_FARM, 'F', false, false, false, SR_COUNT, true});
                }
                if (!(items & BIT_MINE)) {
                    actions.push_back({SR_ACT_BUILD_MINE, 'M', false, false, false, SR_COUNT, true});
                }
                if (!(items & BIT_SOLAR)) {
                    actions.push_back({SR_ACT_BUILD_SOLAR, 'S', false, false, false, SR_COUNT, true});
                }
                if (!(items & BIT_FOREST)) {
                    actions.push_back({SR_ACT_BUILD_FOREST, 'F', true, false, false, SR_COUNT, true});
                }
                actions.push_back({SR_ACT_BUILD_SENSOR, 'O', false, false, false, SR_COUNT, true});
                actions.push_back({SR_ACT_BUILD_CONDENSER, 'N', false, false, false, SR_COUNT, true});
                actions.push_back({SR_ACT_BUILD_BOREHOLE, 'N', true, false, false, SR_COUNT, true});
                actions.push_back({SR_ACT_BUILD_AIRBASE, VK_OEM_PERIOD, false, false, false, SR_COUNT, true});
                actions.push_back({SR_ACT_BUILD_BUNKER, VK_OEM_PERIOD, true, false, false, SR_COUNT, true});
            }
            // Automate: our modal handles skip
            actions.push_back({SR_ACT_AUTOMATE, 'A', true, false, true, SR_COUNT, false});
        }

        // Colony — game opens naming dialog, handles skip itself
        if (veh->is_colony()) {
            actions.push_back({SR_ACT_BUILD_BASE, 'B', false, false, false, SR_COUNT, false});
        }

        // Combat: long range fire — our modal handles skip
        if (veh->is_combat_unit()) {
            actions.push_back({SR_ACT_LONG_RANGE, 'F', false, false, true, SR_COUNT, false});
        }

        // Supply — game opens resource popup, handles skip itself
        if (veh->is_supply()) {
            actions.push_back({SR_ACT_CONVOY, 'O', false, false, false, SR_COUNT, false});
        }

        // Transport
        if (veh->is_transport()) {
            actions.push_back({SR_ACT_UNLOAD, 'U', false, false, false, SR_ACT_DONE_UNLOAD, true});
        }

        // Land units: airdrop — our modal handles skip
        if (veh->triad() != TRIAD_SEA && veh->triad() != TRIAD_AIR) {
            actions.push_back({SR_ACT_AIRDROP, 'I', false, false, true, SR_COUNT, false});
        }

        // On base tile — opens base screen, no skip
        if (tile && tile->is_base()) {
            actions.push_back({SR_ACT_OPEN_BASE, VK_RETURN, false, false, false, SR_COUNT, false});
        }

        if (actions.empty()) {
            sr_output(loc(SR_ACT_MENU_EMPTY), true);
            return true;
        }

        int total = (int)actions.size();
        int index = 0;
        char buf[256];

        snprintf(buf, sizeof(buf), loc(SR_ACT_MENU_OPEN), total);
        sr_output(buf, true);

        snprintf(buf, sizeof(buf), loc(SR_ACT_MENU_ITEM),
            1, total, loc(actions[0].label));
        sr_output(buf, false);

        bool want_close = false;
        bool confirmed = false;
        MSG modal_msg;

        while (!want_close) {
            if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
                if (modal_msg.message == WM_QUIT) {
                    PostQuitMessage((int)modal_msg.wParam);
                    break;
                }
                if (modal_msg.message == WM_KEYDOWN) {
                    WPARAM k = modal_msg.wParam;
                    if (k == VK_ESCAPE) {
                        want_close = true;
                    } else if (k == VK_RETURN) {
                        want_close = true;
                        confirmed = true;
                    } else if (k == VK_UP) {
                        index = (index - 1 + total) % total;
                        snprintf(buf, sizeof(buf), loc(SR_ACT_MENU_ITEM),
                            index + 1, total, loc(actions[index].label));
                        sr_output(buf, true);
                    } else if (k == VK_DOWN) {
                        index = (index + 1) % total;
                        snprintf(buf, sizeof(buf), loc(SR_ACT_MENU_ITEM),
                            index + 1, total, loc(actions[index].label));
                        sr_output(buf, true);
                    }
                }
            } else {
                Sleep(10);
            }
        }

        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

        if (confirmed && index >= 0 && index < total) {
            // Re-validate unit still exists
            if (veh_id >= 0 && veh_id < *VehCount
                && Vehs[veh_id].faction_id == MapWin->cOwner) {
                const UnitAction& act = actions[index];
                sr_debug_log("ACT-MENU: executing %s (vk=%d shift=%d ctrl=%d accessible=%d)",
                    loc(act.label), (int)act.vk, act.need_shift, act.need_ctrl, act.use_accessible);

                // Set skip flag so OnTimer announces "Skipped. Next unit:"
                if (act.vk == VK_SPACE && !act.use_accessible) {
                    sr_unit_skipped = true;
                }

                if (act.need_shift) keybd_event(VK_SHIFT, 0, 0, 0);
                if (act.need_ctrl) keybd_event(VK_CONTROL, 0, 0, 0);

                if (act.use_accessible) {
                    PostMessage(hwnd, WM_KEYDOWN, act.vk, 0);
                    PostMessage(hwnd, WM_KEYUP, act.vk, 0);
                } else {
                    WinProc(hwnd, WM_KEYDOWN, act.vk, 0);
                }

                if (act.need_ctrl) keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                if (act.need_shift) keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);

                // Announce confirmation for actions without other feedback
                if (act.confirm != SR_COUNT) {
                    sr_output(loc(act.confirm), true);
                }

                // Advance to next unit for actions that consume the turn
                if (act.skip_after && !act.use_accessible) {
                    mod_veh_skip(veh_id);
                }
            }
        } else {
            sr_output(loc(SR_ACT_MENU_CANCEL), true);
        }
        return true;
    }

    // Ctrl+F3 = faction status overview
    if (wParam == VK_F3 && ctrl_key_down() && !shift_key_down()
        && cur_win == GW_World && !*GameHalted) {
        int fid = *CurrentPlayerFaction;
        Faction* f = &Factions[fid];
        char buf[1024];
        int pos = 0;

        // Turn and year
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_STATUS_HEADER), *CurrentTurn, *CurrentMissionYear);
        pos += snprintf(buf + pos, sizeof(buf) - pos, " ");

        // Energy credits
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_STATUS_CREDITS), f->energy_credits);
        pos += snprintf(buf + pos, sizeof(buf) - pos, " ");

        // Allocation sliders
        int psych = f->SE_alloc_psych;
        int labs = f->SE_alloc_labs;
        int econ = 100 - psych - labs;
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_STATUS_ALLOC), econ, psych, labs);
        pos += snprintf(buf + pos, sizeof(buf) - pos, " ");

        // Income breakdown
        int commerce = f->turn_commerce_income;
        int surplus = f->energy_surplus_total;
        int maint = f->facility_maint_total;
        int gross = surplus + maint;
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_STATUS_INCOME), commerce, gross, maint, surplus);
        pos += snprintf(buf + pos, sizeof(buf) - pos, " ");

        // Research status
        int tech_id = f->tech_research_id;
        if (tech_id >= 0 && Tech && Tech[tech_id].name) {
            int acc = f->tech_accumulated;
            int cost = f->tech_cost;
            int pct = (cost > 0) ? (acc * 100 / cost) : 0;
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                loc(SR_STATUS_RESEARCH),
                sr_game_str(Tech[tech_id].name), acc, cost, pct);
        } else {
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                loc(SR_STATUS_RESEARCH_NONE));
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, " ");

        // Bases and population
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_STATUS_BASES), f->base_count, f->pop_total);

        sr_debug_log("FACTION-STATUS: %s", buf);
        sr_output(buf, true);
        return true;
    }

    return false;
}

/// Accessible time controls modal (Shift+T on world map).
/// Lets user browse and select from 6 time control presets.
void RunTimeControls() {
    int total = MaxTimeControlNum;
    int index = AlphaIniPrefs->time_controls;
    if (index < 0 || index >= total) index = 0;

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_TC_OPEN),
        total, sr_game_str(TimeControl[index].name));
    sr_output(buf, true);

    snprintf(buf, sizeof(buf), loc(SR_TC_ITEM),
        index + 1, total, sr_game_str(TimeControl[index].name),
        TimeControl[index].turn, TimeControl[index].base,
        TimeControl[index].unit);
    sr_output(buf, false);

    bool want_close = false;
    bool confirmed = false;
    MSG modal_msg;

    while (!want_close) {
        if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
            if (modal_msg.message == WM_QUIT) {
                PostQuitMessage((int)modal_msg.wParam);
                break;
            }
            if (modal_msg.message == WM_KEYDOWN) {
                WPARAM k = modal_msg.wParam;
                if (k == VK_ESCAPE) {
                    want_close = true;
                } else if (k == VK_RETURN) {
                    want_close = true;
                    confirmed = true;
                } else if (k == VK_UP) {
                    index = (index - 1 + total) % total;
                    snprintf(buf, sizeof(buf), loc(SR_TC_ITEM),
                        index + 1, total, sr_game_str(TimeControl[index].name),
                        TimeControl[index].turn, TimeControl[index].base,
                        TimeControl[index].unit);
                    sr_output(buf, true);
                } else if (k == VK_DOWN) {
                    index = (index + 1) % total;
                    snprintf(buf, sizeof(buf), loc(SR_TC_ITEM),
                        index + 1, total, sr_game_str(TimeControl[index].name),
                        TimeControl[index].turn, TimeControl[index].base,
                        TimeControl[index].unit);
                    sr_output(buf, true);
                }
            }
        } else {
            Sleep(10);
        }
    }

    MSG drain;
    while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
    while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

    if (confirmed) {
        AlphaIniPrefs->time_controls = index;
        set_time_controls();
        snprintf(buf, sizeof(buf), loc(SR_TC_SET),
            sr_game_str(TimeControl[index].name));
        sr_output(buf, true);
        sr_debug_log("TIME-CONTROLS: set to %d (%s)",
            index, sr_game_str(TimeControl[index].name));
    } else {
        sr_output(loc(SR_TC_CANCELLED), true);
    }
}

} // namespace WorldMapHandler
