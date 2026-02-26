/*
 * BaseScreenHandler - Accessibility handler for the base screen.
 * Announces base info and provides keyboard navigation through data sections.
 */

#include "gui.h"
#include "base.h"
#include "base_handler.h"
#include "screen_reader.h"
#include "localization.h"

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

// Nerve staple state
static bool _nerveStapleConfirm = false;

// Base rename state
static bool _renameActive = false;
static char _renameBuf[25] = {};    // working buffer
static char _renameOriginal[25] = {}; // backup for cancel
static int _renameLen = 0;

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
    if (base->nutrient_surplus <= 0) {
        snprintf(growth_str, sizeof(growth_str), "%s", loc(SR_FMT_GROWTH_NEVER));
    } else {
        snprintf(growth_str, sizeof(growth_str), loc(SR_FMT_TURNS), growth_turns);
    }

    int nut_cost = mod_cost_factor(base->faction_id, RSC_NUTRIENT, *CurrentBaseID)
        * (base->pop_size + 1);

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_FMT_RESOURCES_V2),
        base->nutrient_surplus, base->nutrients_accumulated, nut_cost, growth_str,
        base->mineral_surplus, base->energy_surplus);
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

    char credits_str[64];
    snprintf(credits_str, sizeof(credits_str), loc(SR_FMT_FACTION_CREDITS),
        f->energy_credits);

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_FMT_ECONOMY_V2),
        base->economy_total, base->psych_total, base->labs_total,
        credits_str);
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
    _nerveStapleConfirm = false;
    _renameActive = false;

    if (!valid_base()) {
        sr_output(loc(SR_BASE_SCREEN), true);
        return;
    }

    BASE* base = &Bases[*CurrentBaseID];
    int item = base->item();
    int cost = mineral_cost(*CurrentBaseID, item);

    char turns_str[64];
    int prod_turns = turns_to_complete(base);
    if (base->mineral_surplus <= 0) {
        snprintf(turns_str, sizeof(turns_str), "%s", loc(SR_FMT_GROWTH_NEVER));
    } else {
        snprintf(turns_str, sizeof(turns_str), loc(SR_FMT_TURNS), prod_turns);
    }

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_FMT_BASE_OPEN_V2),
        sr_base_name(base), (int)base->pop_size,
        sr_prod(item), base->minerals_accumulated, cost, turns_str);
    sr_output(buf, true);
    if (announce_help) {
        sr_output(loc(SR_BASE_HELP), false);  // queue help on first open only
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
    return loc(SR_BASE_HELP);
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
                        sr_output(_renameBuf, true);
                    } else {
                        sr_output(loc(SR_RENAME_CANCEL), true); // "empty" feedback
                    }
                }
                return true;
            }
            if (wParam == 'R' && ctrl_key_down()) {
                // Read current name buffer
                if (_renameLen > 0) {
                    sr_output(_renameBuf, true);
                } else {
                    sr_output(loc(SR_RENAME_CANCEL), true);
                }
                return true;
            }
            return true; // consume all other keydown in rename mode
        }
        if (msg == WM_CHAR) {
            char ch = (char)wParam;
            // Accept printable ASCII chars (space through tilde)
            if (ch >= 32 && ch <= 126 && _renameLen < 24) {
                _renameBuf[_renameLen++] = ch;
                _renameBuf[_renameLen] = '\0';
                char letter[4];
                snprintf(letter, sizeof(letter), loc(SR_RENAME_CHAR_FMT), ch);
                sr_output(letter, true);
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
