/*
 * BaseScreenHandler - Accessibility handler for the base screen.
 * Announces base info and provides keyboard navigation through data sections.
 */

#include "gui.h"
#include "base.h"
#include "base_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "map.h"

// Helper: convert game production name to UTF-8
static const char* sr_prod(int item_id) {
    return sr_game_str(prod_name(item_id));
}
// Helper: convert game base name to UTF-8
static const char* sr_base_name(BASE* b) {
    return sr_game_str(b->name);
}

static int _currentSection = BS_Overview;
static int _lastBaseID = -1;
static bool _announceInterrupt = true;  // controls sr_output interrupt flag

// Production picker state
static bool _prodPickerActive = false;
static int _prodPickerIndex = 0;
static int _prodPickerCount = 0;
static int _prodPickerItems[600]; // item IDs: positive=unit, negative=facility/project

// Queue management state
static bool _queueActive = false;
static int _queueIndex = 0;
static bool _queueInsertMode = false; // picker opened from queue Insert key

// Facility demolition state
static bool _demolitionActive = false;
static int _demolitionIndex = 0;
static int _demolitionCount = 0;
static int _demolitionItems[65]; // FacilityId values of built facilities
static bool _demolitionConfirm = false; // Awaiting second Delete press

// Garrison list state
static bool _garrisonActive = false;
static int _garrisonIndex = 0;
static int _garrisonCount = 0;
static int _garrisonVehIds[64]; // Vehicle indices in Vehs[]

// Supported units list state
static bool _supportActive = false;
static int _supportIndex = 0;
static int _supportCount = 0;
static int _supportVehIds[128]; // Vehicle indices in Vehs[]

// Nerve staple state
static bool _nerveStapleConfirm = false;

// Base rename state
static bool _renameActive = false;
static char _renameBuf[25] = {};    // working buffer
static char _renameOriginal[25] = {}; // backup for cancel
static int _renameLen = 0;

// Tile assignment state
static bool _tileAssignActive = false;
static int _tileAssignIndex = 0;
static int _tileAssignCount = 0;

struct TileAssignEntry {
    int bit_index;    // 0-20
    int x, y;         // map coordinates
    int N, M, E;      // yields (food, minerals, energy)
    int flags;        // BaseTileFlags value
    bool worked;      // currently being worked
};
static TileAssignEntry _tileAssignList[21];

static const SrStr section_str_ids[] = {
    SR_SEC_OVERVIEW,
    SR_SEC_RESOURCES,
    SR_SEC_PRODUCTION,
    SR_SEC_ECONOMY,
    SR_SEC_FACILITIES,
    SR_SEC_STATUS,
    SR_SEC_UNITS
};

static bool valid_base() {
    return *CurrentBaseID >= 0 && *CurrentBaseID < *BaseCount;
}

/// Calculate turns until nutrient box fills for population growth.
/// Returns 0 if no growth possible (surplus <= 0).
static int turns_to_growth(BASE* base) {
    if (base->nutrient_surplus <= 0) return 0;
    int cost = mod_cost_factor(base->faction_id, RSC_NUTRIENT, *CurrentBaseID)
        * (base->pop_size + 1);
    int remaining = max(0, cost - base->nutrients_accumulated);
    return (remaining + base->nutrient_surplus - 1) / base->nutrient_surplus;
}

/// Calculate turns until current production item completes.
/// Returns 0 if no mineral surplus.
static int turns_to_complete(BASE* base) {
    int cost = mineral_cost(*CurrentBaseID, base->item());
    int remaining = max(0, cost - base->minerals_accumulated);
    if (base->mineral_surplus <= 0) return 0;
    return (remaining + base->mineral_surplus - 1) / base->mineral_surplus;
}

/// Build a string describing workers and specialists breakdown.
/// Example: "3 workers, 1 Librarian, 1 Engineer"
static int build_workforce_str(BASE* base, char* buf, int bufsize) {
    int workers = __builtin_popcount(base->worked_tiles);
    // worked_tiles includes the base tile itself, but that's counted as a worker
    // The base tile is always worked, so workers = popcount(worked_tiles)
    // Total pop = workers + specialists (base tile citizen counted in worked_tiles)
    // Actually: pop_size + 1 = popcount(worked_tiles) + specialist_total
    // workers displayed = popcount(worked_tiles) - 1 (subtract base tile) is wrong
    // In SMAC: worked_tiles bit 0 = base center. The citizens working tiles are the "workers".
    // pop_size + 1 = total citizens. worked_tiles bits = tiles being worked (incl base center).
    // specialist_total = citizens not working tiles.
    // So "workers" (tile-working citizens) = popcount(worked_tiles) which includes base center.
    // We'll report it simply as workers count.

    int pos = 0;
    char workers_buf[64];
    snprintf(workers_buf, sizeof(workers_buf), loc(SR_FMT_WORKERS), workers);
    pos += snprintf(buf + pos, bufsize - pos, "%s", workers_buf);

    if (base->specialist_total > 0) {
        // Count specialists by type
        int type_counts[MaxCitizenNum] = {};
        for (int i = 0; i < base->specialist_total && i < MaxBaseSpecNum; i++) {
            int type_id = base->specialist_type(i);
            if (type_id >= 0 && type_id < MaxCitizenNum) {
                type_counts[type_id]++;
            }
        }
        for (int i = 0; i < MaxCitizenNum; i++) {
            if (type_counts[i] > 0 && Citizen[i].singular_name) {
                pos += snprintf(buf + pos, bufsize - pos, ", %d %s",
                    type_counts[i], Citizen[i].singular_name);
            }
        }
    }

    return pos;
}

static void announce_section_overview() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    char workforce[256];
    build_workforce_str(base, workforce, sizeof(workforce));

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_FMT_OVERVIEW_V2),
        sr_base_name(base), (int)base->x, (int)base->y, (int)base->pop_size,
        workforce,
        base->talent_total, base->drone_total);
    sr_output(buf, _announceInterrupt);
}

static void announce_section_resources() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];

    // Growth info
    char growth_str[64];
    int growth_turns = turns_to_growth(base);
    if (base->nutrient_surplus < 0) {
        snprintf(growth_str, sizeof(growth_str), "%s", loc(SR_FMT_STARVATION));
    } else if (base->nutrient_surplus == 0) {
        snprintf(growth_str, sizeof(growth_str), "%s", loc(SR_FMT_GROWTH_NEVER));
    } else {
        snprintf(growth_str, sizeof(growth_str), loc(SR_FMT_TURNS), growth_turns);
    }

    int nut_cost = mod_cost_factor(base->faction_id, RSC_NUTRIENT, *CurrentBaseID)
        * (base->pop_size + 1);

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_FMT_RESOURCES_V3),
        base->nutrient_surplus, base->nutrients_accumulated, nut_cost, growth_str,
        base->nutrient_consumption,
        base->mineral_surplus, base->mineral_consumption,
        base->energy_surplus, base->energy_inefficiency);
    sr_output(buf, _announceInterrupt);
}

static void announce_section_production() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    int item = base->item();
    int cost = mineral_cost(*CurrentBaseID, item);
    int accumulated = base->minerals_accumulated;

    // Turns info
    char turns_str[64];
    int prod_turns = turns_to_complete(base);
    if (base->mineral_surplus <= 0) {
        snprintf(turns_str, sizeof(turns_str), "%s", loc(SR_FMT_GROWTH_NEVER));
    } else {
        snprintf(turns_str, sizeof(turns_str), loc(SR_FMT_TURNS), prod_turns);
    }

    char buf[512];
    int pos = snprintf(buf, sizeof(buf), loc(SR_FMT_PRODUCTION_V2),
        sr_prod(item), accumulated, cost, turns_str);

    // Queue items (skip first which is current production)
    if (base->queue_size > 1) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_FMT_QUEUE));
        for (int i = 1; i < base->queue_size && i < 10; i++) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, " %s,",
                sr_prod(base->queue_items[i]));
        }
        if (pos > 0 && buf[pos - 1] == ',') {
            buf[pos - 1] = '.';
        }
    }

    // Hurry cost
    int mins_left = max(0, cost - accumulated);
    if (mins_left > 0 && base->can_hurry_item()) {
        int hcost = hurry_cost(*CurrentBaseID, item, mins_left);
        if (hcost > 0) {
            snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_HURRY_COST), hcost);
        }
    }

    sr_output(buf, _announceInterrupt);
}

static void announce_section_economy() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    Faction* f = &Factions[base->faction_id];

    // Calculate commerce income from trade
    int commerce = 0;
    for (int i = 0; i < 8; i++) {
        commerce += BaseCommerceImport[i];
    }

    char credits_str[64];
    snprintf(credits_str, sizeof(credits_str), loc(SR_FMT_FACTION_CREDITS),
        f->energy_credits);

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_FMT_ECONOMY_V3),
        base->economy_total, base->psych_total, base->labs_total,
        commerce, credits_str);
    sr_output(buf, _announceInterrupt);
}

static void announce_section_facilities() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];

    int count = 0;
    int total_maint = 0;
    char fac_list[800];
    int fpos = 0;

    for (int i = FAC_HEADQUARTERS; i <= Fac_ID_Last; i++) {
        if (base->has_fac_built((FacilityId)i) && Facility[i].name) {
            if (fpos > 0) {
                fpos += snprintf(fac_list + fpos, sizeof(fac_list) - fpos, ", ");
            }
            fpos += snprintf(fac_list + fpos, sizeof(fac_list) - fpos, "%s", sr_game_str(Facility[i].name));
            total_maint += Facility[i].maint;
            count++;
            if (fpos >= (int)sizeof(fac_list) - 50) break;
        }
    }

    char buf[1024];
    if (count == 0) {
        snprintf(buf, sizeof(buf), "%s", loc(SR_FMT_FACILITIES_NONE));
    } else {
        char count_str[64];
        snprintf(count_str, sizeof(count_str), loc(SR_FMT_FACILITIES_COUNT),
            count, total_maint);
        int pos = snprintf(buf, sizeof(buf), loc(SR_FMT_FACILITIES_V2), count_str);
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", fac_list);
    }

    sr_output(buf, _announceInterrupt);
}

/// Count military units stationed at the current base (for police calculation).
static int count_police_units() {
    if (!valid_base()) return 0;
    BASE* base = &Bases[*CurrentBaseID];
    int bx = base->x;
    int by = base->y;
    int count = 0;
    for (int i = 0; i < *VehCount; i++) {
        VEH* v = &Vehs[i];
        if (v->x == bx && v->y == by && v->faction_id == base->faction_id) {
            UNIT* u = &Units[v->unit_id];
            if (Weapon[u->weapon_id].offense_value > 0) {
                count++;
            }
        }
    }
    return count;
}

/// Check if any base of the faction has headquarters.
static bool faction_has_hq(int faction_id) {
    for (int i = 0; i < *BaseCount; i++) {
        if (Bases[i].faction_id == faction_id
            && has_fac_built(FAC_HEADQUARTERS, i)) {
            return true;
        }
    }
    return false;
}

/// Check if base has any military unit defending it.
static bool base_has_defender() {
    if (!valid_base()) return false;
    BASE* base = &Bases[*CurrentBaseID];
    int bx = base->x;
    int by = base->y;
    for (int i = 0; i < *VehCount; i++) {
        VEH* v = &Vehs[i];
        if (v->x == bx && v->y == by && v->faction_id == base->faction_id) {
            UNIT* u = &Units[v->unit_id];
            if (Weapon[u->weapon_id].offense_value > 0
                || Armor[u->armor_id].defense_value > 1) {
                return true;
            }
        }
    }
    return false;
}

static void announce_section_status() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    char buf[512];
    int pos = snprintf(buf, sizeof(buf), "%s", loc(SR_FMT_STATUS));

    if (base->drone_riots_active()) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_FMT_DRONE_RIOTS));
    }
    if (base->golden_age_active()) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_FMT_GOLDEN_AGE));
    }
    if (base->eco_damage > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_ECO_DAMAGE), base->eco_damage);
    }
    if (base->governor_flags & GOV_ACTIVE) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_FMT_GOVERNOR));
    }
    if (base->nerve_staple_turns_left > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_NERVE_STAPLE),
            (int)base->nerve_staple_turns_left);
    }
    // Starvation warning
    if (base->nutrient_surplus < 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s", loc(SR_FMT_STARVATION));
    }
    // Low minerals warning
    if (base->mineral_surplus <= 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s", loc(SR_FMT_LOW_MINERALS));
    }
    // Inefficiency warning
    if (base->energy_inefficiency > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_INEFFICIENCY),
            base->energy_inefficiency);
    }
    // No headquarters warning
    if (!faction_has_hq(base->faction_id)) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_FMT_NO_HQ));
    }
    // High support cost warning
    if (base->mineral_consumption > 0
        && base->mineral_consumption > base->mineral_intake_2 / 2) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_HIGH_SUPPORT),
            base->mineral_consumption);
    }
    // Undefended warning
    if (!base_has_defender()) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_FMT_UNDEFENDED));
    }
    // Police units
    int police = count_police_units();
    if (police > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_POLICE_UNITS), police);
    }

    if (pos == (int)strlen(loc(SR_FMT_STATUS))) {
        snprintf(buf, sizeof(buf), "%s", loc(SR_FMT_STATUS_NORMAL));
    }

    sr_output(buf, _announceInterrupt);
}

static void announce_section_units() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    int bx = base->x;
    int by = base->y;

    char names[800];
    int npos = 0;
    int count = 0;

    for (int i = 0; i < *VehCount; i++) {
        VEH* v = &Vehs[i];
        if (v->x == bx && v->y == by) {
            if (npos > 0) {
                npos += snprintf(names + npos, sizeof(names) - npos, ", ");
            }
            npos += snprintf(names + npos, sizeof(names) - npos, "%s", v->name());
            count++;
            if (npos >= (int)sizeof(names) - 50) break;
        }
    }

    char buf[1024];
    if (count == 0) {
        snprintf(buf, sizeof(buf), "%s", loc(SR_FMT_UNITS_NONE));
    } else {
        int pos = snprintf(buf, sizeof(buf), "%s ", loc(SR_SEC_UNITS));
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_UNITS), count);
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", names);
    }

    sr_output(buf, _announceInterrupt);
}

/// Build list of available production items for current base.
static void prod_picker_build_list() {
    _prodPickerCount = 0;
    if (!valid_base()) return;
    int base_id = *CurrentBaseID;
    int faction_id = Bases[base_id].faction_id;

    // Facilities (1 to Fac_ID_Last=64)
    for (int i = FAC_HEADQUARTERS; i <= Fac_ID_Last; i++) {
        if (facility_avail((FacilityId)i, faction_id, base_id, 0)) {
            if (_prodPickerCount < 600) {
                _prodPickerItems[_prodPickerCount++] = -i;
            }
        }
    }
    // Secret Projects (SP_ID_First=70 to SP_ID_Last=133)
    for (int i = SP_ID_First; i <= SP_ID_Last; i++) {
        if (facility_avail((FacilityId)i, faction_id, base_id, 0)) {
            if (_prodPickerCount < 600) {
                _prodPickerItems[_prodPickerCount++] = -i;
            }
        }
    }
    // Units (0 to MaxProtoNum-1)
    for (int i = 0; i < MaxProtoNum; i++) {
        if (veh_avail(i, faction_id, base_id)) {
            if (_prodPickerCount < 600) {
                _prodPickerItems[_prodPickerCount++] = i;
            }
        }
    }
}

/// Announce detailed info about the highlighted production picker item.
static void prod_picker_announce_detail() {
    if (_prodPickerCount == 0 || !valid_base()) return;
    int item = _prodPickerItems[_prodPickerIndex];
    int cost = mineral_cost(*CurrentBaseID, item);
    char buf[1024];

    if (item < 0) {
        // Facility or Secret Project
        int fac_id = -item;
        const char* name = Facility[fac_id].name ? sr_game_str(Facility[fac_id].name) : "???";
        const char* effect = Facility[fac_id].effect ? sr_game_str(Facility[fac_id].effect) : "";

        if (fac_id >= SP_ID_First) {
            // Secret Project
            snprintf(buf, sizeof(buf), loc(SR_PROD_DETAIL_PROJECT),
                name, effect, cost);
        } else {
            // Normal facility
            snprintf(buf, sizeof(buf), loc(SR_PROD_DETAIL_FAC),
                name, effect, cost, (int)Facility[fac_id].maint);
        }
    } else {
        // Unit
        UNIT* u = &Units[item];
        const char* chassis_name = Chassis[u->chassis_id].offsv1_name
            ? sr_game_str(Chassis[u->chassis_id].offsv1_name) : "???";
        const char* weapon_name = Weapon[u->weapon_id].name
            ? sr_game_str(Weapon[u->weapon_id].name) : "???";
        const char* armor_name = Armor[u->armor_id].name
            ? sr_game_str(Armor[u->armor_id].name) : "???";
        const char* reactor_name = Reactor[u->reactor_id].name
            ? sr_game_str(Reactor[u->reactor_id].name) : "???";
        int atk = Weapon[u->weapon_id].offense_value;
        int def = Armor[u->armor_id].defense_value;
        int move_speed = Chassis[u->chassis_id].speed;
        int hp = 10 * Reactor[u->reactor_id].power;

        int pos = snprintf(buf, sizeof(buf), loc(SR_PROD_DETAIL_UNIT),
            sr_game_str(u->name), chassis_name, weapon_name, atk,
            armor_name, def, reactor_name, move_speed, hp);

        // Append abilities if any
        if (u->ability_flags) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_UNIT_ABILITIES));
            for (int i = 0; i < MaxAbilityNum; i++) {
                if (u->ability_flags & (1u << i)) {
                    const char* aname = Ability[i].name ? sr_game_str(Ability[i].name) : "???";
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s,", aname);
                    if (pos >= (int)sizeof(buf) - 50) break;
                }
            }
            // Replace trailing comma with period
            if (pos > 0 && buf[pos - 1] == ',') {
                buf[pos - 1] = '.';
            }
        }
    }

    sr_output(buf, true);
}

/// Announce current production picker item.
static void prod_picker_announce_item(bool interrupt = true) {
    if (_prodPickerCount == 0 || !valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    int item = _prodPickerItems[_prodPickerIndex];
    int cost = mineral_cost(*CurrentBaseID, item);

    char turns_str[64];
    if (base->mineral_surplus <= 0) {
        snprintf(turns_str, sizeof(turns_str), "%s", loc(SR_FMT_GROWTH_NEVER));
    } else {
        int turns = (cost + base->mineral_surplus - 1) / base->mineral_surplus;
        snprintf(turns_str, sizeof(turns_str), loc(SR_FMT_TURNS), turns);
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_PROD_PICKER_ITEM),
        _prodPickerIndex + 1, _prodPickerCount,
        sr_prod(item), cost, turns_str);
    sr_output(buf, interrupt);
}

/// Announce queue item at _queueIndex with position, cost, and turns info.
static void queue_announce_item(bool interrupt = true) {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    int total = base->queue_size + 1;
    if (_queueIndex < 0 || _queueIndex >= total) return;

    int item = base->queue_items[_queueIndex];
    int cost = mineral_cost(*CurrentBaseID, item);

    char turns_str[64];
    if (base->mineral_surplus <= 0) {
        snprintf(turns_str, sizeof(turns_str), "%s", loc(SR_FMT_GROWTH_NEVER));
    } else {
        int remaining = (_queueIndex == 0)
            ? max(0, cost - base->minerals_accumulated)
            : cost;
        int turns = (remaining + base->mineral_surplus - 1) / base->mineral_surplus;
        snprintf(turns_str, sizeof(turns_str), loc(SR_FMT_TURNS), turns);
    }

    char buf[256];
    if (_queueIndex == 0) {
        snprintf(buf, sizeof(buf), loc(SR_QUEUE_ITEM_CURRENT),
            _queueIndex + 1, total, sr_prod(item),
            base->minerals_accumulated, cost, turns_str);
    } else {
        snprintf(buf, sizeof(buf), loc(SR_QUEUE_ITEM),
            _queueIndex + 1, total, sr_prod(item), cost, turns_str);
    }
    sr_output(buf, interrupt);
}

/// Swap two queue entries. Does not allow swapping item 0.
static bool queue_swap_items(int a, int b) {
    if (!valid_base()) return false;
    BASE* base = &Bases[*CurrentBaseID];
    int total = base->queue_size + 1;
    if (a <= 0 || b <= 0 || a >= total || b >= total) return false;

    int tmp = base->queue_items[a];
    base->queue_items[a] = base->queue_items[b];
    base->queue_items[b] = tmp;
    GraphicWin_redraw(BaseWin);
    return true;
}

/// Remove item at given index from queue (not item 0). Shifts remaining items down.
static bool queue_remove_item(int idx) {
    if (!valid_base()) return false;
    BASE* base = &Bases[*CurrentBaseID];
    int total = base->queue_size + 1;
    if (idx <= 0 || idx >= total) return false;

    for (int i = idx; i < total - 1; i++) {
        base->queue_items[i] = base->queue_items[i + 1];
    }
    base->queue_items[total - 1] = 0;
    base->queue_size--;
    GraphicWin_redraw(BaseWin);
    return true;
}

/// Insert item at given position in queue. Shifts items up.
static bool queue_insert_item(int item_id, int pos) {
    if (!valid_base()) return false;
    BASE* base = &Bases[*CurrentBaseID];
    int total = base->queue_size + 1;
    if (total >= 10) return false; // queue full
    if (pos < 1) pos = 1;
    if (pos > total) pos = total;

    // Shift items up from end to pos
    for (int i = total; i > pos; i--) {
        base->queue_items[i] = base->queue_items[i - 1];
    }
    base->queue_items[pos] = item_id;
    base->queue_size++;
    GraphicWin_redraw(BaseWin);
    return true;
}

/// Announce detail for the current queue item (reuses prod_picker_announce_detail logic).
static void queue_announce_detail() {
    if (!valid_base() || !_queueActive) return;
    BASE* base = &Bases[*CurrentBaseID];
    int total = base->queue_size + 1;
    if (_queueIndex < 0 || _queueIndex >= total) return;

    int item = base->queue_items[_queueIndex];
    int cost = mineral_cost(*CurrentBaseID, item);
    char buf[1024];

    if (item < 0) {
        int fac_id = -item;
        const char* name = Facility[fac_id].name ? sr_game_str(Facility[fac_id].name) : "???";
        const char* effect = Facility[fac_id].effect ? sr_game_str(Facility[fac_id].effect) : "";
        if (fac_id >= SP_ID_First) {
            snprintf(buf, sizeof(buf), loc(SR_PROD_DETAIL_PROJECT), name, effect, cost);
        } else {
            snprintf(buf, sizeof(buf), loc(SR_PROD_DETAIL_FAC),
                name, effect, cost, (int)Facility[fac_id].maint);
        }
    } else {
        UNIT* u = &Units[item];
        const char* chassis_name = Chassis[u->chassis_id].offsv1_name
            ? sr_game_str(Chassis[u->chassis_id].offsv1_name) : "???";
        const char* weapon_name = Weapon[u->weapon_id].name
            ? sr_game_str(Weapon[u->weapon_id].name) : "???";
        const char* armor_name = Armor[u->armor_id].name
            ? sr_game_str(Armor[u->armor_id].name) : "???";
        const char* reactor_name = Reactor[u->reactor_id].name
            ? sr_game_str(Reactor[u->reactor_id].name) : "???";
        int atk = Weapon[u->weapon_id].offense_value;
        int def = Armor[u->armor_id].defense_value;
        int move_speed = Chassis[u->chassis_id].speed;
        int hp = 10 * Reactor[u->reactor_id].power;
        snprintf(buf, sizeof(buf), loc(SR_PROD_DETAIL_UNIT),
            sr_game_str(u->name), chassis_name, weapon_name, atk,
            armor_name, def, reactor_name, move_speed, hp);
    }
    sr_output(buf, true);
}

/// Build list of built facilities for demolition mode.
static void demolition_build_list() {
    _demolitionCount = 0;
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    for (int i = FAC_HEADQUARTERS; i <= Fac_ID_Last; i++) {
        if (base->has_fac_built((FacilityId)i) && Facility[i].name) {
            if (_demolitionCount < 65) {
                _demolitionItems[_demolitionCount++] = i;
            }
        }
    }
}

/// Announce the current demolition list item.
static void demolition_announce_item(bool interrupt = true) {
    if (_demolitionCount == 0) return;
    int fac_id = _demolitionItems[_demolitionIndex];
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_DEMOLITION_ITEM),
        _demolitionIndex + 1, _demolitionCount,
        sr_game_str(Facility[fac_id].name), (int)Facility[fac_id].maint);
    sr_output(buf, interrupt);
}

/// Announce detailed info about the selected facility.
static void demolition_announce_detail() {
    if (_demolitionCount == 0) return;
    int fac_id = _demolitionItems[_demolitionIndex];
    const char* name = Facility[fac_id].name ? sr_game_str(Facility[fac_id].name) : "???";
    const char* effect = Facility[fac_id].effect ? sr_game_str(Facility[fac_id].effect) : "";
    int cost = Facility[fac_id].cost;
    int maint = Facility[fac_id].maint;
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_DEMOLITION_DETAIL),
        name, effect, cost, maint);
    sr_output(buf, true);
}

/// Execute facility demolition: remove facility, set scrapped flag, recalculate.
static void demolition_execute() {
    if (_demolitionCount == 0 || !valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];

    if (base->state_flags & BSTATE_FACILITY_SCRAPPED) {
        sr_output(loc(SR_DEMOLITION_BLOCKED), true);
        return;
    }

    int fac_id = _demolitionItems[_demolitionIndex];
    const char* name = Facility[fac_id].name ? sr_game_str(Facility[fac_id].name) : "???";

    // Remove facility using set_fac
    set_fac((FacilityId)fac_id, *CurrentBaseID, false);
    base->state_flags |= BSTATE_FACILITY_SCRAPPED;
    base_compute(1);
    GraphicWin_redraw(BaseWin);

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_DEMOLITION_DONE), name);
    sr_output(buf, true);
    sr_debug_log("DEMOLITION: %s (fac_id=%d)", name, fac_id);

    // Rebuild list and adjust index
    demolition_build_list();
    if (_demolitionCount == 0) {
        sr_output(loc(SR_DEMOLITION_EMPTY), false);
        _demolitionActive = false;
    } else {
        if (_demolitionIndex >= _demolitionCount) {
            _demolitionIndex = _demolitionCount - 1;
        }
        demolition_announce_item(false);
    }
}

/// Build list of vehicle IDs stationed at the current base.
static void garrison_build_list() {
    _garrisonCount = 0;
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    int bx = base->x;
    int by = base->y;
    for (int i = 0; i < *VehCount; i++) {
        VEH* v = &Vehs[i];
        if (v->x == bx && v->y == by && v->faction_id == base->faction_id) {
            if (_garrisonCount < 64) {
                _garrisonVehIds[_garrisonCount++] = i;
            }
        }
    }
}

/// Announce the current garrison list item.
static void garrison_announce_item(bool interrupt = true) {
    if (_garrisonCount == 0) return;
    int veh_id = _garrisonVehIds[_garrisonIndex];
    VEH* v = &Vehs[veh_id];
    const char* morale_name = Morale[v->morale].name
        ? sr_game_str(Morale[v->morale].name) : "???";
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_GARRISON_ITEM),
        _garrisonIndex + 1, _garrisonCount,
        sr_game_str(v->name()), v->cur_hitpoints(), v->max_hitpoints(),
        morale_name);
    sr_output(buf, interrupt);
}

/// Announce detailed info about the selected garrison unit.
static void garrison_announce_detail() {
    if (_garrisonCount == 0) return;
    int veh_id = _garrisonVehIds[_garrisonIndex];
    VEH* v = &Vehs[veh_id];
    UNIT* u = &Units[v->unit_id];

    const char* weapon_name = Weapon[u->weapon_id].name
        ? sr_game_str(Weapon[u->weapon_id].name) : "???";
    const char* armor_name = Armor[u->armor_id].name
        ? sr_game_str(Armor[u->armor_id].name) : "???";
    const char* chassis_name = Chassis[u->chassis_id].offsv1_name
        ? sr_game_str(Chassis[u->chassis_id].offsv1_name) : "???";
    int atk = Weapon[u->weapon_id].offense_value;
    int def = Armor[u->armor_id].defense_value;
    int move_speed = Chassis[u->chassis_id].speed;

    // Home base name
    const char* home_name = "---";
    if (v->home_base_id >= 0 && v->home_base_id < *BaseCount) {
        home_name = sr_game_str(Bases[v->home_base_id].name);
    }

    // Build abilities string
    char abilities[256] = "";
    if (u->ability_flags) {
        int pos = snprintf(abilities, sizeof(abilities), "%s", loc(SR_UNIT_ABILITIES));
        for (int i = 0; i < MaxAbilityNum; i++) {
            if (u->ability_flags & (1u << i)) {
                const char* aname = Ability[i].name ? sr_game_str(Ability[i].name) : "???";
                pos += snprintf(abilities + pos, sizeof(abilities) - pos, " %s,", aname);
                if (pos >= (int)sizeof(abilities) - 50) break;
            }
        }
        if (pos > 0 && abilities[pos - 1] == ',') {
            abilities[pos - 1] = '.';
        }
    }

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_GARRISON_DETAIL),
        sr_game_str(v->name()), weapon_name, atk, armor_name, def,
        chassis_name, move_speed, home_name, abilities);
    sr_output(buf, true);
}

/// Activate the selected garrison unit: close base, select unit on world map.
static void garrison_activate() {
    if (_garrisonCount == 0 || !valid_base()) return;
    int veh_id = _garrisonVehIds[_garrisonIndex];
    VEH* v = &Vehs[veh_id];

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_GARRISON_ACTIVATE), sr_game_str(v->name()));

    *CurrentVehID = veh_id;
    veh_wake(veh_id);
    _garrisonActive = false;
    GraphicWin_close((Win*)BaseWin);

    sr_output(buf, true);
    sr_debug_log("GARRISON-ACTIVATE: veh_id=%d %s", veh_id, v->name());
}

/// Set the home base of the selected garrison unit to the current base.
static void garrison_set_home() {
    if (_garrisonCount == 0 || !valid_base()) return;
    int veh_id = _garrisonVehIds[_garrisonIndex];
    VEH* v = &Vehs[veh_id];

    action_home(veh_id, *CurrentBaseID);

    BASE* base = &Bases[*CurrentBaseID];
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_GARRISON_HOME_SET), sr_game_str(base->name));
    sr_output(buf, true);
    sr_debug_log("GARRISON-HOME: veh_id=%d -> base %s", veh_id, base->name);
}

/// Build list of vehicle IDs supported by (home based at) the current base.
static void support_build_list() {
    _supportCount = 0;
    if (!valid_base()) return;
    int base_id = *CurrentBaseID;
    BASE* base = &Bases[base_id];
    for (int i = 0; i < *VehCount; i++) {
        VEH* v = &Vehs[i];
        if (v->home_base_id == base_id && v->faction_id == base->faction_id) {
            if (_supportCount < 128) {
                _supportVehIds[_supportCount++] = i;
            }
        }
    }
}

/// Announce the current support list item.
static void support_announce_item(bool interrupt = true) {
    if (_supportCount == 0) return;
    int veh_id = _supportVehIds[_supportIndex];
    VEH* v = &Vehs[veh_id];
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_SUPPORT_ITEM),
        _supportIndex + 1, _supportCount,
        sr_game_str(v->name()), (int)v->x, (int)v->y,
        v->cur_hitpoints(), v->max_hitpoints());
    sr_output(buf, interrupt);
}

/// Announce detailed info about the selected support unit.
static void support_announce_detail() {
    if (_supportCount == 0) return;
    int veh_id = _supportVehIds[_supportIndex];
    VEH* v = &Vehs[veh_id];
    UNIT* u = &Units[v->unit_id];

    const char* weapon_name = Weapon[u->weapon_id].name
        ? sr_game_str(Weapon[u->weapon_id].name) : "???";
    const char* armor_name = Armor[u->armor_id].name
        ? sr_game_str(Armor[u->armor_id].name) : "???";
    const char* chassis_name = Chassis[u->chassis_id].offsv1_name
        ? sr_game_str(Chassis[u->chassis_id].offsv1_name) : "???";
    int atk = Weapon[u->weapon_id].offense_value;
    int def = Armor[u->armor_id].defense_value;
    int move_speed = Chassis[u->chassis_id].speed;

    const char* home_name = "---";
    if (v->home_base_id >= 0 && v->home_base_id < *BaseCount) {
        home_name = sr_game_str(Bases[v->home_base_id].name);
    }

    // Build abilities string
    char abilities[256] = "";
    if (u->ability_flags) {
        int pos = snprintf(abilities, sizeof(abilities), "%s", loc(SR_UNIT_ABILITIES));
        for (int i = 0; i < MaxAbilityNum; i++) {
            if (u->ability_flags & (1u << i)) {
                const char* aname = Ability[i].name ? sr_game_str(Ability[i].name) : "???";
                pos += snprintf(abilities + pos, sizeof(abilities) - pos, " %s,", aname);
                if (pos >= (int)sizeof(abilities) - 50) break;
            }
        }
        if (pos > 0 && abilities[pos - 1] == ',') {
            abilities[pos - 1] = '.';
        }
    }

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_SUPPORT_DETAIL),
        sr_game_str(v->name()), weapon_name, atk, armor_name, def,
        chassis_name, move_speed, home_name, (int)v->x, (int)v->y, abilities);
    sr_output(buf, true);
}

/// Activate the selected support unit: close base, select unit on world map.
static void support_activate() {
    if (_supportCount == 0 || !valid_base()) return;
    int veh_id = _supportVehIds[_supportIndex];
    VEH* v = &Vehs[veh_id];

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_SUPPORT_ACTIVATE), sr_game_str(v->name()));

    *CurrentVehID = veh_id;
    veh_wake(veh_id);
    _supportActive = false;
    GraphicWin_close((Win*)BaseWin);

    sr_output(buf, true);
    sr_debug_log("SUPPORT-ACTIVATE: veh_id=%d %s", veh_id, v->name());
}

/// Announce psych detail for the current base.
static void announce_psych_detail() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];

    int police = count_police_units();

    // Determine riot status string
    const char* riot_status;
    if (base->drone_riots_active()) {
        riot_status = loc(SR_FMT_DRONE_RIOTS);
    } else if (base->golden_age_active()) {
        riot_status = loc(SR_FMT_GOLDEN_AGE);
    } else {
        riot_status = loc(SR_FMT_STATUS_NORMAL);
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_PSYCH_DETAIL),
        police, base->psych_total,
        base->talent_total, base->drone_total,
        riot_status);
    sr_output(buf, true);
}

static void announce_current_section() {
    switch (_currentSection) {
        case BS_Overview:    announce_section_overview(); break;
        case BS_Resources:   announce_section_resources(); break;
        case BS_Production:  announce_section_production(); break;
        case BS_Economy:     announce_section_economy(); break;
        case BS_Facilities:  announce_section_facilities(); break;
        case BS_Status:      announce_section_status(); break;
        case BS_Units:       announce_section_units(); break;
    }
}

namespace BaseScreenHandler {

bool IsPickerActive() {
    return _prodPickerActive;
}

bool IsQueueActive() {
    return _queueActive;
}

bool IsDemolitionActive() {
    return _demolitionActive;
}

bool IsGarrisonActive() {
    return _garrisonActive;
}

bool IsSupportActive() {
    return _supportActive;
}

bool IsRenameActive() {
    return _renameActive;
}

bool IsActive() {
    if (!*GameHalted) {
        int state = Win_get_key_window();
        if (state == (int32_t)(&MapWin->oMainWin.oWinBase.field_4)) {
            return Win_is_visible(BaseWin) != 0;
        }
    }
    return false;
}

void OnOpen(bool announce_help) {
    _currentSection = BS_Overview;
    _lastBaseID = *CurrentBaseID;
    _garrisonActive = false;
    _supportActive = false;
    _nerveStapleConfirm = false;
    _renameActive = false;

    if (!valid_base()) {
        sr_output(loc(SR_BASE_SCREEN), true);
        return;
    }

    BASE* base = &Bases[*CurrentBaseID];
    char buf[768];
    int pos = 0;

    // 1. Special states (only if present, announced first)
    if (base->drone_riots_active()) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_FMT_DRONE_RIOTS));
    }
    if (base->golden_age_active()) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_FMT_GOLDEN_AGE));
    }
    if (base->eco_damage > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_ECO_DAMAGE), base->eco_damage);
    }
    if (base->nerve_staple_turns_left > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_NERVE_STAPLE),
            (int)base->nerve_staple_turns_left);
    }
    if (pos > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, " ");
    }

    // 2. Base name and population
    pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_BASE_OPEN_NAME),
        sr_base_name(base), (int)base->pop_size);

    // 3. Resources with growth info
    char growth_str[64];
    int growth_turns = turns_to_growth(base);
    if (base->nutrient_surplus <= 0) {
        snprintf(growth_str, sizeof(growth_str), "%s", loc(SR_FMT_GROWTH_NEVER));
    } else {
        snprintf(growth_str, sizeof(growth_str), loc(SR_FMT_TURNS), growth_turns);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, " ");
    pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_BASE_OPEN_RESOURCES),
        base->nutrient_surplus, growth_str,
        base->mineral_surplus, base->energy_surplus);

    // 4. Mood (only if talents or drones > 0)
    if (base->talent_total > 0 || base->drone_total > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, " ");
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_BASE_OPEN_MOOD),
            base->talent_total, base->drone_total);
    }

    // 5. Starvation warning
    if (base->nutrient_surplus < 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s", loc(SR_FMT_STARVATION));
    }

    // 6. Production
    int item = base->item();
    int cost = mineral_cost(*CurrentBaseID, item);
    char prod_turns_str[64];
    int prod_turns = turns_to_complete(base);
    if (base->mineral_surplus <= 0) {
        snprintf(prod_turns_str, sizeof(prod_turns_str), "%s", loc(SR_FMT_GROWTH_NEVER));
    } else {
        snprintf(prod_turns_str, sizeof(prod_turns_str), loc(SR_FMT_TURNS), prod_turns);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, " ");
    pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_FMT_BASE_OPEN_PROD),
        sr_prod(item), base->minerals_accumulated, cost, prod_turns_str);

    // 7. Undefended warning
    if (!base_has_defender()) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s", loc(SR_FMT_UNDEFENDED));
    }

    sr_output(buf, true);

    // 8. Help hint (only on first open) — uses GetHelpText() so it's always in sync
    if (announce_help) {
        sr_output(GetHelpText(), false);
    }
    sr_debug_log("BASE-OPEN: %s", buf);
}

void OnClose() {
    _lastBaseID = -1;
    _prodPickerActive = false;
    _queueActive = false;
    _queueInsertMode = false;
    _demolitionActive = false;
    _demolitionConfirm = false;
    _garrisonActive = false;
    _supportActive = false;
    _tileAssignActive = false;
    _nerveStapleConfirm = false;
    _renameActive = false;
    sr_debug_log("BASE-CLOSE");
}

void SetSectionFromTab(int tab_index) {
    switch (tab_index) {
        case 0: _currentSection = BS_Resources; break;
        case 1: _currentSection = BS_Overview; break;
        case 2: _currentSection = BS_Production; break;
        default: break;
    }
}

void AnnounceCurrentSection(bool interrupt) {
    _announceInterrupt = interrupt;
    announce_current_section();
    _announceInterrupt = true;
}

const char* GetHelpText() {
    return loc(SR_BASE_HELP_V2);
}

bool IsTileAssignActive() {
    return _tileAssignActive;
}

/// Build a compact terrain description string for a tile.
static int build_tile_terrain_desc(int x, int y, char* buf, int bufsize) {
    MAP* sq = mapsq(x, y);
    if (!sq) return snprintf(buf, bufsize, "?");

    int pos = 0;
    int alt = sq->alt_level();
    bool is_land = (alt >= ALT_SHORE_LINE);

    // Terrain type
    if (alt == ALT_OCEAN_TRENCH) {
        pos += snprintf(buf + pos, bufsize - pos, "%s", loc(SR_TERRAIN_TRENCH));
    } else if (alt <= ALT_OCEAN) {
        pos += snprintf(buf + pos, bufsize - pos, "%s", loc(SR_TERRAIN_OCEAN));
    } else if (alt == ALT_OCEAN_SHELF) {
        pos += snprintf(buf + pos, bufsize - pos, "%s", loc(SR_TERRAIN_SHELF));
    } else {
        if (sq->is_rocky())
            pos += snprintf(buf + pos, bufsize - pos, "%s", loc(SR_TERRAIN_ROCKY));
        else if (sq->is_rolling())
            pos += snprintf(buf + pos, bufsize - pos, "%s", loc(SR_TERRAIN_ROLLING));
        else
            pos += snprintf(buf + pos, bufsize - pos, "%s", loc(SR_TERRAIN_FLAT));
    }

    // Moisture (land only)
    if (is_land) {
        if (sq->is_rainy())
            pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_TERRAIN_RAINY));
        else if (sq->is_moist())
            pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_TERRAIN_MOIST));
        else if (sq->is_arid())
            pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_TERRAIN_ARID));
    }

    // Key features
    if (sq->is_fungus())
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_FUNGUS));
    if (sq->items & BIT_FOREST)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_FOREST));
    if (sq->items & BIT_FARM)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_FARM));
    if (sq->items & BIT_SOIL_ENRICHER)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_SOIL_ENRICHER));
    if (sq->items & BIT_MINE)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_MINE));
    if (sq->items & BIT_SOLAR)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_SOLAR));
    if (sq->items & BIT_CONDENSER)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_CONDENSER));
    if (sq->items & BIT_ECH_MIRROR)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_ECH_MIRROR));
    if (sq->items & BIT_THERMAL_BORE)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_BOREHOLE));
    if (sq->items & BIT_ROAD)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_ROAD));
    if (sq->items & BIT_MAGTUBE)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_MAGTUBE));
    if (sq->items & BIT_BUNKER)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_BUNKER));
    if (sq->items & BIT_AIRBASE)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_AIRBASE));
    if (sq->items & BIT_SENSOR)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_SENSOR));
    if (sq->items & BIT_RIVER)
        pos += snprintf(buf + pos, bufsize - pos, ", %s", loc(SR_FEATURE_RIVER));

    return pos;
}

/// Build the tile assignment list for the current base.
static void tile_assign_build_list() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];
    int faction_id = base->faction_id;
    int base_id = *CurrentBaseID;

    _tileAssignCount = 0;
    for (int i = 0; i < 21; i++) {
        int tx, ty;
        MAP* sq = next_tile(base->x, base->y, i, &tx, &ty);
        if (!sq) continue;

        TileAssignEntry& e = _tileAssignList[_tileAssignCount];
        e.bit_index = i;
        e.x = tx;
        e.y = ty;
        e.flags = BaseTileFlags[i];
        e.worked = (base->worked_tiles & (1 << i)) != 0;

        // Calculate yields even for unavailable tiles (useful for info)
        if (e.flags == 0 || e.flags == BR_BASE_IN_TILE || e.worked) {
            e.N = mod_crop_yield(faction_id, base_id, tx, ty, 0);
            e.M = mod_mine_yield(faction_id, base_id, tx, ty, 0);
            e.E = mod_energy_yield(faction_id, base_id, tx, ty, 0);
        } else {
            e.N = e.M = e.E = 0;
        }
        _tileAssignCount++;
    }
}

/// Get the unavailability reason string for a tile.
static const char* tile_unavail_reason(int flags) {
    if (flags & BR_FOREIGN_TILE) return loc(SR_TILE_UNAVAIL_FOREIGN);
    if (flags & BR_WORKER_ACTIVE) return loc(SR_TILE_UNAVAIL_OTHER_BASE);
    if (flags & BR_VEH_IN_TILE) return loc(SR_TILE_UNAVAIL_VEHICLE);
    if (flags & BR_NOT_VISIBLE) return loc(SR_TILE_UNAVAIL_UNEXPLORED);
    if (flags & BR_NOT_AVAILABLE) return loc(SR_TILE_UNAVAIL_UNEXPLORED);
    return loc(SR_TILE_UNAVAIL_UNEXPLORED);
}

/// Announce the current tile assignment item.
static void tile_assign_announce_item() {
    if (_tileAssignIndex < 0 || _tileAssignIndex >= _tileAssignCount) return;
    TileAssignEntry& e = _tileAssignList[_tileAssignIndex];

    char terrain[256];
    build_tile_terrain_desc(e.x, e.y, terrain, sizeof(terrain));

    char buf[512];
    int pos1 = _tileAssignIndex + 1;
    int total = _tileAssignCount;

    if (e.bit_index == 0) {
        // Base center — always worked
        snprintf(buf, sizeof(buf), loc(SR_TILE_ASSIGN_CENTER),
            pos1, total, e.x, e.y, terrain, e.N, e.M, e.E);
    } else if (e.worked) {
        snprintf(buf, sizeof(buf), loc(SR_TILE_ASSIGN_WORKED),
            pos1, total, e.x, e.y, terrain, e.N, e.M, e.E);
    } else if (e.flags == 0) {
        // Available (no flags = available)
        snprintf(buf, sizeof(buf), loc(SR_TILE_ASSIGN_AVAILABLE),
            pos1, total, e.x, e.y, terrain, e.N, e.M, e.E);
    } else {
        // Unavailable
        snprintf(buf, sizeof(buf), loc(SR_TILE_ASSIGN_UNAVAIL),
            pos1, total, tile_unavail_reason(e.flags), e.x, e.y, terrain);
    }

    sr_output(buf, true);
}

/// Toggle worker on/off for the current tile.
static void tile_assign_toggle() {
    if (!valid_base()) return;
    if (_tileAssignIndex < 0 || _tileAssignIndex >= _tileAssignCount) return;
    TileAssignEntry& e = _tileAssignList[_tileAssignIndex];
    BASE* base = &Bases[*CurrentBaseID];

    // Base center: cannot toggle
    if (e.bit_index == 0) {
        sr_output(loc(SR_TILE_ASSIGN_CANNOT_CENTER), true);
        return;
    }

    if (e.worked) {
        // Disable governor citizen management so manual changes persist
        if (base->governor_flags & GOV_ACTIVE && base->governor_flags & GOV_MANAGE_CITIZENS) {
            base->governor_flags &= ~GOV_MANAGE_CITIZENS;
            sr_debug_log("TILE-ASSIGN: disabled GOV_MANAGE_CITIZENS for %s", base->name);
        }
        // Remove worker from this tile → convert to specialist
        base->worked_tiles &= ~(1 << e.bit_index);
        base->specialist_total++;
        int spec_type = best_specialist(base, 1, 1, 2);
        if (base->specialist_total > 0) {
            base->set_specialist_type(base->specialist_total - 1, spec_type);
        }
        base_compute(1);
        GraphicWin_redraw((Win*)BaseWin);

        // Rebuild list after change
        tile_assign_build_list();

        char buf[256];
        const char* spec_name = sr_game_str(Citizen[spec_type].singular_name);
        snprintf(buf, sizeof(buf), loc(SR_TILE_ASSIGN_REMOVED), spec_name);
        sr_output(buf, true);
        sr_debug_log("TILE-ASSIGN: removed worker from bit %d, now specialist %s",
            e.bit_index, Citizen[spec_type].singular_name);
    } else {
        // Check if tile is available
        if (e.flags != 0) {
            sr_output(loc(SR_TILE_ASSIGN_CANNOT_UNAVAIL), true);
            return;
        }
        // Check if there's a free citizen (specialist to convert)
        if (base->specialist_total <= 0) {
            sr_output(loc(SR_TILE_ASSIGN_CANNOT_NO_FREE), true);
            return;
        }
        // Disable governor citizen management so manual changes persist
        if (base->governor_flags & GOV_ACTIVE && base->governor_flags & GOV_MANAGE_CITIZENS) {
            base->governor_flags &= ~GOV_MANAGE_CITIZENS;
            sr_debug_log("TILE-ASSIGN: disabled GOV_MANAGE_CITIZENS for %s", base->name);
        }
        // Assign worker to this tile
        base->specialist_total--;
        base->worked_tiles |= (1 << e.bit_index);
        base_compute(1);
        GraphicWin_redraw((Win*)BaseWin);

        // Rebuild list after change
        tile_assign_build_list();

        // Re-read yields after recompute
        TileAssignEntry& updated = _tileAssignList[_tileAssignIndex];
        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_TILE_ASSIGN_ASSIGNED),
            updated.N, updated.M, updated.E);
        sr_output(buf, true);
        sr_debug_log("TILE-ASSIGN: assigned worker to bit %d (%d,%d)",
            e.bit_index, e.x, e.y);
    }
}

/// Announce tile assignment summary.
static void tile_assign_announce_summary() {
    if (!valid_base()) return;
    BASE* base = &Bases[*CurrentBaseID];

    int worked = __builtin_popcount(base->worked_tiles);
    int total_N = 0, total_M = 0, total_E = 0;
    for (int i = 0; i < _tileAssignCount; i++) {
        if (_tileAssignList[i].worked) {
            total_N += _tileAssignList[i].N;
            total_M += _tileAssignList[i].M;
            total_E += _tileAssignList[i].E;
        }
    }

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_TILE_ASSIGN_SUMMARY),
        worked, _tileAssignCount, (int)base->specialist_total,
        total_N, total_M, total_E);
    sr_output(buf, true);
}

bool Update(UINT msg, WPARAM wParam) {
    // Rename mode: handle both WM_KEYDOWN and WM_CHAR
    if (_renameActive) {
        if (msg == WM_KEYDOWN) {
            if (wParam == VK_RETURN) {
                // Confirm rename
                if (!valid_base()) { _renameActive = false; return true; }
                BASE* base = &Bases[*CurrentBaseID];
                _renameBuf[_renameLen] = '\0';
                if (_renameLen > 0) {
                    strncpy(base->name, _renameBuf, 24);
                    base->name[24] = '\0';
                    GraphicWin_redraw(BaseWin);
                    char buf[128];
                    snprintf(buf, sizeof(buf), loc(SR_RENAME_DONE), sr_game_str(base->name));
                    sr_output(buf, true);
                    sr_debug_log("RENAME: %s", base->name);
                } else {
                    // Empty name: restore original
                    strncpy(base->name, _renameOriginal, 24);
                    base->name[24] = '\0';
                    sr_output(loc(SR_RENAME_CANCEL), true);
                }
                _renameActive = false;
                return true;
            }
            if (wParam == VK_ESCAPE) {
                // Cancel rename, restore original
                if (valid_base()) {
                    BASE* base = &Bases[*CurrentBaseID];
                    strncpy(base->name, _renameOriginal, 24);
                    base->name[24] = '\0';
                }
                _renameActive = false;
                sr_output(loc(SR_RENAME_CANCEL), true);
                return true;
            }
            if (wParam == VK_BACK) {
                if (_renameLen > 0) {
                    _renameLen--;
                    _renameBuf[_renameLen] = '\0';
                    if (_renameLen > 0) {
                        sr_output(sr_game_str(_renameBuf), true);
                    } else {
                        sr_output(loc(SR_RENAME_CANCEL), true); // "empty" feedback
                    }
                }
                return true;
            }
            if (wParam == 'R' && ctrl_key_down()) {
                // Read current name buffer
                if (_renameLen > 0) {
                    sr_output(sr_game_str(_renameBuf), true);
                } else {
                    sr_output(loc(SR_RENAME_CANCEL), true);
                }
                return true;
            }
            return true; // consume all other keydown in rename mode
        }
        if (msg == WM_CHAR) {
            unsigned char ch = (unsigned char)wParam;
            // Accept printable chars (Windows-1252, including umlauts)
            if (ch >= 32 && ch != 127 && _renameLen < 24) {
                _renameBuf[_renameLen++] = (char)ch;
                _renameBuf[_renameLen] = '\0';
                // Convert single char from Windows-1252 to UTF-8 for speech
                char ansi[2] = { (char)ch, '\0' };
                char utf8[8];
                sr_ansi_to_utf8(ansi, utf8, sizeof(utf8));
                sr_output(utf8, true);
            }
            return true; // consume all WM_CHAR in rename mode
        }
        return false;
    }

    if (msg != WM_KEYDOWN) return false;

    // Production picker active: intercept navigation keys
    if (_prodPickerActive) {
        if (wParam == VK_UP) {
            if (_prodPickerCount > 0) {
                _prodPickerIndex = (_prodPickerIndex + _prodPickerCount - 1) % _prodPickerCount;
                prod_picker_announce_item();
            }
            return true;
        }
        if (wParam == VK_DOWN) {
            if (_prodPickerCount > 0) {
                _prodPickerIndex = (_prodPickerIndex + 1) % _prodPickerCount;
                prod_picker_announce_item();
            }
            return true;
        }
        if (wParam == VK_RETURN) {
            if (_prodPickerCount > 0 && valid_base()) {
                int item = _prodPickerItems[_prodPickerIndex];
                if (_queueInsertMode) {
                    // Insert into queue after _queueIndex
                    int insert_pos = _queueIndex + 1;
                    if (queue_insert_item(item, insert_pos)) {
                        _queueIndex = insert_pos;
                        char buf[256];
                        snprintf(buf, sizeof(buf), loc(SR_QUEUE_ADDED),
                            sr_prod(item), insert_pos + 1);
                        sr_output(buf, true);
                        sr_debug_log("QUEUE-INSERT: %s at pos %d", sr_prod(item), insert_pos);
                    }
                    _prodPickerActive = false;
                    _queueInsertMode = false;
                    // Stay in queue mode, announce current item
                    queue_announce_item(false);
                } else {
                    BASE* base = &Bases[*CurrentBaseID];
                    base->queue_items[0] = item;
                    base->minerals_accumulated = 0;
                    GraphicWin_redraw(BaseWin);
                    char buf[256];
                    snprintf(buf, sizeof(buf), loc(SR_PROD_PICKER_SELECT),
                        sr_prod(item));
                    sr_output(buf, true);
                    sr_debug_log("PROD-PICKER: selected %s (id=%d)", sr_prod(item), item);
                    _prodPickerActive = false;
                }
            } else {
                _prodPickerActive = false;
                _queueInsertMode = false;
            }
            return true;
        }
        if (wParam == VK_ESCAPE) {
            _prodPickerActive = false;
            if (_queueInsertMode) {
                _queueInsertMode = false;
                sr_output(loc(SR_PROD_PICKER_CANCEL), true);
                // Return to queue mode, announce current item
                queue_announce_item(false);
            } else {
                sr_output(loc(SR_PROD_PICKER_CANCEL), true);
            }
            return true;
        }
        if (wParam == 'D') {
            prod_picker_announce_detail();
            return true;
        }
        // Consume all other keys while picker is active
        return true;
    }

    // Queue management mode: intercept navigation keys
    if (_queueActive) {
        if (!valid_base()) {
            _queueActive = false;
            return true;
        }
        BASE* base = &Bases[*CurrentBaseID];
        int total = base->queue_size + 1;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if (wParam == VK_UP && !shift) {
            _queueIndex = (_queueIndex + total - 1) % total;
            queue_announce_item();
            return true;
        }
        if (wParam == VK_DOWN && !shift) {
            _queueIndex = (_queueIndex + 1) % total;
            queue_announce_item();
            return true;
        }
        if (wParam == VK_UP && shift) {
            if (_queueIndex == 0) {
                sr_output(loc(SR_QUEUE_CANNOT_MOVE_CURRENT), true);
            } else if (_queueIndex == 1) {
                // Can't swap with item 0
                sr_output(loc(SR_QUEUE_CANNOT_MOVE_CURRENT), true);
            } else {
                int item = base->queue_items[_queueIndex];
                queue_swap_items(_queueIndex, _queueIndex - 1);
                _queueIndex--;
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_QUEUE_MOVED),
                    sr_prod(item), _queueIndex + 1);
                sr_output(buf, true);
            }
            return true;
        }
        if (wParam == VK_DOWN && shift) {
            if (_queueIndex == 0) {
                sr_output(loc(SR_QUEUE_CANNOT_MOVE_CURRENT), true);
            } else if (_queueIndex >= total - 1) {
                // Already at bottom, nowhere to move
                sr_output(loc(SR_QUEUE_CANNOT_MOVE_CURRENT), true);
            } else {
                int item = base->queue_items[_queueIndex];
                queue_swap_items(_queueIndex, _queueIndex + 1);
                _queueIndex++;
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_QUEUE_MOVED),
                    sr_prod(item), _queueIndex + 1);
                sr_output(buf, true);
            }
            return true;
        }
        if (wParam == VK_DELETE) {
            if (_queueIndex == 0) {
                sr_output(loc(SR_QUEUE_CANNOT_REMOVE_CURRENT), true);
            } else {
                const char* name = sr_prod(base->queue_items[_queueIndex]);
                char buf[256];
                queue_remove_item(_queueIndex);
                int new_total = base->queue_size + 1;
                snprintf(buf, sizeof(buf), loc(SR_QUEUE_REMOVED), name, new_total);
                sr_output(buf, true);
                sr_debug_log("QUEUE-REMOVE: %s, %d remaining", name, new_total);
                // Clamp index
                if (_queueIndex >= new_total) {
                    _queueIndex = new_total - 1;
                }
                queue_announce_item(false);
            }
            return true;
        }
        if (wParam == VK_INSERT) {
            if (total >= 10) {
                sr_output(loc(SR_QUEUE_FULL), true);
            } else {
                // Open picker in insert mode
                prod_picker_build_list();
                if (_prodPickerCount == 0) {
                    sr_output(loc(SR_PROD_PICKER_EMPTY), true);
                } else {
                    _prodPickerActive = true;
                    _queueInsertMode = true;
                    _prodPickerIndex = 0;
                    char buf[256];
                    snprintf(buf, sizeof(buf), loc(SR_PROD_PICKER_OPEN), _prodPickerCount);
                    sr_output(buf, true);
                    prod_picker_announce_item(false);
                }
            }
            return true;
        }
        if (wParam == 'D') {
            queue_announce_detail();
            return true;
        }
        if (wParam == VK_ESCAPE) {
            _queueActive = false;
            sr_output(loc(SR_QUEUE_CLOSED), true);
            return true;
        }
        // Consume all other keys while queue is active
        return true;
    }

    // Facility demolition mode: intercept navigation keys
    if (_demolitionActive) {
        if (!valid_base()) {
            _demolitionActive = false;
            _demolitionConfirm = false;
            return true;
        }

        if (wParam == VK_UP) {
            _demolitionConfirm = false;
            if (_demolitionCount > 0) {
                _demolitionIndex = (_demolitionIndex + _demolitionCount - 1) % _demolitionCount;
                demolition_announce_item();
            }
            return true;
        }
        if (wParam == VK_DOWN) {
            _demolitionConfirm = false;
            if (_demolitionCount > 0) {
                _demolitionIndex = (_demolitionIndex + 1) % _demolitionCount;
                demolition_announce_item();
            }
            return true;
        }
        if (wParam == VK_DELETE) {
            if (_demolitionCount > 0) {
                if (!_demolitionConfirm) {
                    _demolitionConfirm = true;
                    int fac_id = _demolitionItems[_demolitionIndex];
                    char buf[256];
                    snprintf(buf, sizeof(buf), loc(SR_DEMOLITION_CONFIRM),
                        sr_game_str(Facility[fac_id].name));
                    sr_output(buf, true);
                } else {
                    _demolitionConfirm = false;
                    demolition_execute();
                }
            }
            return true;
        }
        if (wParam == 'D') {
            _demolitionConfirm = false;
            demolition_announce_detail();
            return true;
        }
        if (wParam == VK_ESCAPE) {
            _demolitionActive = false;
            _demolitionConfirm = false;
            sr_output(loc(SR_DEMOLITION_CANCEL), true);
            return true;
        }
        // Consume all other keys while demolition is active
        return true;
    }

    // Garrison list mode: intercept navigation keys
    if (_garrisonActive) {
        if (!valid_base()) {
            _garrisonActive = false;
            return true;
        }

        if (wParam == VK_UP) {
            if (_garrisonCount > 0) {
                _garrisonIndex = (_garrisonIndex + _garrisonCount - 1) % _garrisonCount;
                garrison_announce_item();
            }
            return true;
        }
        if (wParam == VK_DOWN) {
            if (_garrisonCount > 0) {
                _garrisonIndex = (_garrisonIndex + 1) % _garrisonCount;
                garrison_announce_item();
            }
            return true;
        }
        if (wParam == 'D') {
            garrison_announce_detail();
            return true;
        }
        if (wParam == VK_RETURN) {
            garrison_activate();
            return true;
        }
        if (wParam == 'B') {
            garrison_set_home();
            return true;
        }
        if (wParam == VK_ESCAPE) {
            _garrisonActive = false;
            sr_output(loc(SR_GARRISON_CLOSE), true);
            return true;
        }
        // Let game-native base keys (H=Hurry) pass through
        return true;
    }

    // Supported units list mode: intercept navigation keys
    if (_supportActive) {
        if (!valid_base()) {
            _supportActive = false;
            return true;
        }

        if (wParam == VK_UP) {
            if (_supportCount > 0) {
                _supportIndex = (_supportIndex + _supportCount - 1) % _supportCount;
                support_announce_item();
            }
            return true;
        }
        if (wParam == VK_DOWN) {
            if (_supportCount > 0) {
                _supportIndex = (_supportIndex + 1) % _supportCount;
                support_announce_item();
            }
            return true;
        }
        if (wParam == 'D') {
            support_announce_detail();
            return true;
        }
        if (wParam == VK_RETURN) {
            support_activate();
            return true;
        }
        if (wParam == VK_ESCAPE) {
            _supportActive = false;
            sr_output(loc(SR_SUPPORT_CLOSE), true);
            return true;
        }
        return true;
    }

    // Tile assignment mode: intercept navigation keys
    if (_tileAssignActive) {
        if (!valid_base()) {
            _tileAssignActive = false;
            return true;
        }

        if (wParam == VK_UP) {
            if (_tileAssignCount > 0) {
                _tileAssignIndex = (_tileAssignIndex + _tileAssignCount - 1) % _tileAssignCount;
                tile_assign_announce_item();
            }
            return true;
        }
        if (wParam == VK_DOWN) {
            if (_tileAssignCount > 0) {
                _tileAssignIndex = (_tileAssignIndex + 1) % _tileAssignCount;
                tile_assign_announce_item();
            }
            return true;
        }
        if (wParam == VK_SPACE) {
            tile_assign_toggle();
            return true;
        }
        if (wParam == 'S' || wParam == VK_TAB) {
            tile_assign_announce_summary();
            return true;
        }
        if (wParam == VK_ESCAPE) {
            _tileAssignActive = false;
            sr_output(loc(SR_TILE_ASSIGN_CLOSE), true);
            return true;
        }
        // Consume all other keys while tile assign is active
        return true;
    }

    // Ctrl+Q: toggle queue management mode
    if (wParam == 'Q' && ctrl_key_down()) {
        if (!valid_base()) return true;
        BASE* base = &Bases[*CurrentBaseID];
        if (base->faction_id != MapWin->cOwner) return true;

        int total = base->queue_size + 1;
        _queueActive = true;
        _queueIndex = 0;

        if (total <= 1) {
            char buf[256];
            snprintf(buf, sizeof(buf), loc(SR_QUEUE_OPEN_ONE), sr_prod(base->queue_items[0]));
            sr_output(buf, true);
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), loc(SR_QUEUE_OPEN), total);
            sr_output(buf, true);
            queue_announce_item(false);
        }
        sr_debug_log("QUEUE-OPEN: %d items", total);
        return true;
    }

    // Ctrl+Shift+P: open production picker
    if (wParam == 'P' && ctrl_key_down() && (GetKeyState(VK_SHIFT) & 0x8000)) {
        if (!valid_base()) return true;
        BASE* base = &Bases[*CurrentBaseID];
        if (base->faction_id != MapWin->cOwner) return true;

        prod_picker_build_list();
        if (_prodPickerCount == 0) {
            sr_output(loc(SR_PROD_PICKER_EMPTY), true);
            return true;
        }
        _prodPickerActive = true;
        _prodPickerIndex = 0;

        // Announce what's currently being built
        int cur_item = base->item();
        if (cur_item != 0) {
            int cur_cost = mineral_cost(*CurrentBaseID, cur_item);
            char turns_str[64];
            int prod_turns = turns_to_complete(base);
            if (base->mineral_surplus <= 0) {
                snprintf(turns_str, sizeof(turns_str), "%s", loc(SR_FMT_GROWTH_NEVER));
            } else {
                snprintf(turns_str, sizeof(turns_str), loc(SR_FMT_TURNS), prod_turns);
            }
            char cur_buf[256];
            snprintf(cur_buf, sizeof(cur_buf), loc(SR_PROD_PICKER_CURRENT),
                sr_prod(cur_item), base->minerals_accumulated, cur_cost, turns_str);
            sr_output(cur_buf, true);
        } else {
            sr_output(loc(SR_PROD_PICKER_CURRENT_NONE), true);
        }

        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_PROD_PICKER_OPEN), _prodPickerCount);
        sr_output(buf, false);
        // Announce first item (queued after open message)
        prod_picker_announce_item(false);
        return true;
    }

    // Ctrl+D: open facility demolition mode
    if (wParam == 'D' && ctrl_key_down()) {
        if (!valid_base()) return true;
        BASE* base = &Bases[*CurrentBaseID];
        if (base->faction_id != MapWin->cOwner) return true;

        demolition_build_list();
        if (_demolitionCount == 0) {
            sr_output(loc(SR_DEMOLITION_EMPTY), true);
            return true;
        }

        _demolitionActive = true;
        _demolitionConfirm = false;
        _demolitionIndex = 0;

        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_DEMOLITION_OPEN),
            _demolitionCount, 1, _demolitionCount,
            sr_game_str(Facility[_demolitionItems[0]].name));
        sr_output(buf, true);
        sr_debug_log("DEMOLITION-OPEN: %d facilities", _demolitionCount);
        return true;
    }

    // Ctrl+U: open garrison list
    if (wParam == 'U' && ctrl_key_down()) {
        if (!valid_base()) return true;

        garrison_build_list();
        if (_garrisonCount == 0) {
            sr_output(loc(SR_GARRISON_EMPTY), true);
            return true;
        }

        _garrisonActive = true;
        _garrisonIndex = 0;

        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_GARRISON_OPEN), _garrisonCount);
        sr_output(buf, true);
        garrison_announce_item(false);
        sr_debug_log("GARRISON-OPEN: %d units", _garrisonCount);
        return true;
    }

    // Ctrl+Shift+S: open supported units list
    if (wParam == 'S' && ctrl_key_down() && (GetKeyState(VK_SHIFT) & 0x8000)) {
        if (!valid_base()) return true;

        support_build_list();
        if (_supportCount == 0) {
            sr_output(loc(SR_SUPPORT_EMPTY), true);
            return true;
        }

        _supportActive = true;
        _supportIndex = 0;

        BASE* base = &Bases[*CurrentBaseID];
        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_SUPPORT_OPEN),
            _supportCount, base->mineral_consumption);
        sr_output(buf, true);
        support_announce_item(false);
        sr_debug_log("SUPPORT-OPEN: %d units", _supportCount);
        return true;
    }

    // Ctrl+Shift+Y: psych detail
    if (wParam == 'Y' && ctrl_key_down() && (GetKeyState(VK_SHIFT) & 0x8000)) {
        announce_psych_detail();
        return true;
    }

    // Ctrl+T: open tile assignment mode
    if (wParam == 'T' && ctrl_key_down()) {
        if (!valid_base()) return true;
        BASE* base = &Bases[*CurrentBaseID];
        if (base->faction_id != MapWin->cOwner) return true;

        tile_assign_build_list();
        _tileAssignActive = true;
        _tileAssignIndex = 0;

        int worked = __builtin_popcount(base->worked_tiles);
        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_TILE_ASSIGN_OPEN),
            _tileAssignCount, worked);
        sr_output(buf, true);
        tile_assign_announce_item();
        sr_debug_log("TILE-ASSIGN-OPEN: %d tiles, %d worked", _tileAssignCount, worked);
        return true;
    }

    // Ctrl+N: nerve staple
    if (wParam == 'N' && ctrl_key_down()) {
        if (!valid_base()) return true;
        BASE* base = &Bases[*CurrentBaseID];
        if (base->faction_id != MapWin->cOwner) return true;

        if (!can_staple(*CurrentBaseID)) {
            sr_output(loc(SR_NERVE_STAPLE_CANNOT), true);
            _nerveStapleConfirm = false;
            return true;
        }

        if (!_nerveStapleConfirm) {
            _nerveStapleConfirm = true;
            char buf[256];
            snprintf(buf, sizeof(buf), loc(SR_NERVE_STAPLE_CONFIRM), sr_base_name(base));
            sr_output(buf, true);
        } else {
            _nerveStapleConfirm = false;
            BaseWin_action_staple(*CurrentBaseID);
            // Re-read the base after action
            base = &Bases[*CurrentBaseID];
            char buf[256];
            snprintf(buf, sizeof(buf), loc(SR_NERVE_STAPLE_DONE),
                sr_base_name(base), (int)base->nerve_staple_turns_left);
            sr_output(buf, true);
            sr_debug_log("NERVE-STAPLE: %s, %d turns", base->name, (int)base->nerve_staple_turns_left);
        }
        return true;
    }

    // F2: rename base
    if (wParam == VK_F2) {
        if (!valid_base()) return true;
        BASE* base = &Bases[*CurrentBaseID];
        if (base->faction_id != MapWin->cOwner) return true;

        _renameActive = true;
        _renameLen = 0;
        _renameBuf[0] = '\0';
        strncpy(_renameOriginal, base->name, 24);
        _renameOriginal[24] = '\0';

        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_RENAME_OPEN), sr_base_name(base));
        sr_output(buf, true);
        sr_debug_log("RENAME-OPEN: %s", base->name);
        return true;
    }

    // Ctrl+Up/Down: cycle sections
    if ((wParam == VK_UP || wParam == VK_DOWN) && ctrl_key_down()) {
        if (wParam == VK_DOWN) {
            _currentSection = (_currentSection + 1) % BS_Count;
        } else {
            _currentSection = (_currentSection + BS_Count - 1) % BS_Count;
        }
        sr_debug_log("BASE-SECTION: %s", loc(section_str_ids[_currentSection]));
        announce_current_section();
        return true;
    }

    // Ctrl+I: repeat current section
    if (wParam == 'I' && ctrl_key_down()) {
        announce_current_section();
        return true;
    }

    return false;
}

} // namespace BaseScreenHandler
