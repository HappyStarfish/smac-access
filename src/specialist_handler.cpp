/*
 * Specialist Management handler for base screen.
 * Allows blind players to manage base specialists via keyboard.
 * Activated with Ctrl+W in the base screen. Modal loop pattern.
 */

#include "specialist_handler.h"
#include "engine.h"
#include "gui.h"
#include "modal_utils.h"
#include "base.h"
#include "screen_reader.h"
#include "localization.h"
#include "map.h"
#include "path.h"

namespace SpecialistHandler {

static bool _active = false;
static bool _wantClose = false;
static int _currentSlot = 0;   // 0-based index into total citizens
static int _totalSlots = 0;    // pop_size + 1 (total citizens)
static int _workerCount = 0;   // number of tile-working citizens
static int _specCount = 0;     // specialist_total

// Worker tile indices: maps worker slot index to worked_tiles bit index
static int _workerTileBit[21];  // max 21 tiles in base radius
static int _workerSlotCount = 0;

// Build worker slot mapping from worked_tiles bitmask
static void build_worker_map(BASE* base) {
    _workerSlotCount = 0;
    for (int i = 0; i < 21; i++) {
        if (base->worked_tiles & (1 << i)) {
            _workerTileBit[_workerSlotCount++] = i;
        }
    }
}

// Check if slot index is a worker (0..workerCount-1) or specialist
static bool is_worker_slot(int slot) {
    return slot < _workerCount;
}

// Check if a citizen type is available to the faction
static bool citizen_available(int citizen_id, int faction_id, int pop_size) {
    if (citizen_id < 0 || citizen_id >= MaxSpecialistNum) return false;
    if (!has_tech(Citizen[citizen_id].preq_tech, faction_id)) return false;
    if (has_tech(Citizen[citizen_id].obsol_tech, faction_id)) return false;
    // Psych specialists with bonus >= 2 bypass min size check
    if (Citizen[citizen_id].psych_bonus >= 2) return true;
    if (pop_size < Rules->min_base_size_specialists) return false;
    return true;
}

// Find next available citizen type in direction (+1 or -1) from current
static int next_citizen_type(int current_type, int direction, int faction_id, int pop_size) {
    for (int i = 1; i < MaxSpecialistNum; i++) {
        int idx = (current_type + direction * i + MaxSpecialistNum) % MaxSpecialistNum;
        if (citizen_available(idx, faction_id, pop_size)) {
            return idx;
        }
    }
    return current_type;
}

// Get best available specialist type for this base
static int get_best_specialist(BASE* base) {
    return best_specialist(base, 1, 1, 2);
}

// Announce the current slot
static void announce_slot() {
    if (*CurrentBaseID < 0 || *CurrentBaseID >= *BaseCount) return;
    BASE* base = &Bases[*CurrentBaseID];
    int base_id = *CurrentBaseID;
    int faction_id = base->faction_id;
    char buf[512];

    if (is_worker_slot(_currentSlot)) {
        // Worker slot: show tile info
        int worker_idx = _currentSlot;
        if (worker_idx < _workerSlotCount) {
            int tile_bit = _workerTileBit[worker_idx];
            int tx, ty;
            next_tile(base->x, base->y, tile_bit, &tx, &ty);

            int N = mod_crop_yield(faction_id, base_id, tx, ty, 0);
            int M = mod_mine_yield(faction_id, base_id, tx, ty, 0);
            int E = mod_energy_yield(faction_id, base_id, tx, ty, 0);

            char yield_str[64];
            snprintf(yield_str, sizeof(yield_str), loc(SR_TILE_YIELDS), N, M, E);

            if (tile_bit == 0) {
                // Base center tile
                snprintf(buf, sizeof(buf), loc(SR_SPEC_WORKER),
                    _currentSlot + 1, _totalSlots, yield_str);
                strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
                strncat(buf, loc(SR_SPEC_CENTER), sizeof(buf) - strlen(buf) - 1);
            } else {
                snprintf(buf, sizeof(buf), loc(SR_SPEC_WORKER),
                    _currentSlot + 1, _totalSlots, yield_str);
            }
        } else {
            snprintf(buf, sizeof(buf), loc(SR_SPEC_WORKER),
                _currentSlot + 1, _totalSlots, "");
        }
    } else {
        // Specialist slot
        int spec_idx = _currentSlot - _workerCount;
        int type_id = base->specialist_type(spec_idx);
        const char* name = "???";
        if (type_id >= 0 && type_id < MaxCitizenNum && Citizen[type_id].singular_name) {
            name = Citizen[type_id].singular_name;
        }

        char bonus_str[128];
        int econ = (type_id >= 0 && type_id < MaxCitizenNum) ? Citizen[type_id].econ_bonus : 0;
        int psych = (type_id >= 0 && type_id < MaxCitizenNum) ? Citizen[type_id].psych_bonus : 0;
        int labs = (type_id >= 0 && type_id < MaxCitizenNum) ? Citizen[type_id].labs_bonus : 0;
        snprintf(bonus_str, sizeof(bonus_str), loc(SR_SPEC_BONUS), econ, psych, labs);

        snprintf(buf, sizeof(buf), loc(SR_SPEC_SPECIALIST),
            _currentSlot + 1, _totalSlots, name, bonus_str);
    }

    sr_output(buf, true);
}

// Announce summary of all citizens
static void announce_summary() {
    if (*CurrentBaseID < 0 || *CurrentBaseID >= *BaseCount) return;
    BASE* base = &Bases[*CurrentBaseID];
    char detail[512];
    int pos = 0;

    pos += snprintf(detail + pos, sizeof(detail) - pos, loc(SR_SPEC_SUMMARY),
        _workerCount, _specCount);

    if (_specCount > 0) {
        // Count specialists by type
        int type_counts[MaxCitizenNum] = {};
        for (int i = 0; i < _specCount && i < MaxBaseSpecNum; i++) {
            int type_id = base->specialist_type(i);
            if (type_id >= 0 && type_id < MaxCitizenNum) {
                type_counts[type_id]++;
            }
        }
        for (int i = 0; i < MaxCitizenNum; i++) {
            if (type_counts[i] > 0 && Citizen[i].singular_name) {
                pos += snprintf(detail + pos, sizeof(detail) - pos, " %d %s.",
                    type_counts[i], Citizen[i].singular_name);
            }
        }
    }

    char buf[640];
    snprintf(buf, sizeof(buf), loc(SR_SPEC_OPEN), detail);
    sr_output(buf, true);
}

// Refresh cached counts after a conversion
static void refresh_counts() {
    if (*CurrentBaseID < 0 || *CurrentBaseID >= *BaseCount) return;
    BASE* base = &Bases[*CurrentBaseID];
    _specCount = base->specialist_total;
    _workerCount = __builtin_popcount(base->worked_tiles);
    _totalSlots = _workerCount + _specCount;
    build_worker_map(base);
}

// Convert the current worker to a specialist
static void convert_to_specialist() {
    if (*CurrentBaseID < 0 || *CurrentBaseID >= *BaseCount) return;
    BASE* base = &Bases[*CurrentBaseID];

    if (!is_worker_slot(_currentSlot)) return;

    int worker_idx = _currentSlot;
    if (worker_idx >= _workerSlotCount) return;
    int tile_bit = _workerTileBit[worker_idx];

    // Cannot remove base center (bit 0)
    if (tile_bit == 0) {
        sr_output(loc(SR_SPEC_CANNOT_CENTER), true);
        return;
    }

    // Check min size
    int best_type = get_best_specialist(base);
    if (!citizen_available(best_type, base->faction_id, base->pop_size)) {
        sr_output(loc(SR_SPEC_CANNOT_MORE), true);
        return;
    }

    // Must keep at least 1 worker (base center)
    if (_workerCount <= 1) {
        sr_output(loc(SR_SPEC_CANNOT_MORE), true);
        return;
    }

    // Clear the worked tile bit
    base->worked_tiles &= ~(1 << tile_bit);
    // Increment specialist count
    base->specialist_total++;
    // Set specialist type for the new specialist (last index)
    int new_spec_idx = base->specialist_total - 1;
    base->set_specialist_type(new_spec_idx, best_type);

    // Refresh local counts (no base_compute/redraw during modal loop)
    refresh_counts();

    // Move cursor to the new specialist
    _currentSlot = _workerCount + new_spec_idx;
    if (_currentSlot >= _totalSlots) _currentSlot = _totalSlots - 1;

    const char* name = "???";
    if (best_type >= 0 && best_type < MaxCitizenNum && Citizen[best_type].singular_name) {
        name = Citizen[best_type].singular_name;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_SPEC_TO_SPECIALIST), name);
    sr_output(buf, true);
    sr_debug_log("SPEC: worker->specialist type=%d (%s)\n", best_type, name);
}

// Convert the current specialist to a worker
static void convert_to_worker() {
    if (*CurrentBaseID < 0 || *CurrentBaseID >= *BaseCount) return;
    BASE* base = &Bases[*CurrentBaseID];

    if (is_worker_slot(_currentSlot)) return;

    if (_specCount <= 0) {
        sr_output(loc(SR_SPEC_CANNOT_LESS), true);
        return;
    }

    int spec_idx = _currentSlot - _workerCount;

    // Remove specialist: shift types down if not last
    for (int i = spec_idx; i < base->specialist_total - 1 && i < MaxBaseSpecNum - 1; i++) {
        base->set_specialist_type(i, base->specialist_type(i + 1));
    }
    base->specialist_total--;

    // Note: game will auto-assign freed citizen to best tile when
    // base_compute(1) runs at modal loop exit.

    // Refresh local counts (no base_compute/redraw during modal loop)
    refresh_counts();

    // Keep cursor in bounds
    if (_currentSlot >= _totalSlots) _currentSlot = _totalSlots - 1;
    if (_currentSlot < 0) _currentSlot = 0;

    sr_output(loc(SR_SPEC_TO_WORKER), true);
    sr_debug_log("SPEC: specialist->worker\n");
}

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return false;

    bool ctrl = ctrl_key_down();

    switch (wParam) {
    case VK_UP:
        if (ctrl) return false;
        _currentSlot = (_currentSlot + _totalSlots - 1) % _totalSlots;
        announce_slot();
        return true;

    case VK_DOWN:
        if (ctrl) return false;
        _currentSlot = (_currentSlot + 1) % _totalSlots;
        announce_slot();
        return true;

    case VK_LEFT:
    case VK_RIGHT:
        if (ctrl) return false;
        // Change specialist type (only for specialist slots)
        if (!is_worker_slot(_currentSlot) && _specCount > 0) {
            BASE* base = &Bases[*CurrentBaseID];
            int spec_idx = _currentSlot - _workerCount;
            int current_type = base->specialist_type(spec_idx);
            int direction = (wParam == VK_LEFT) ? -1 : +1;
            int new_type = next_citizen_type(current_type, direction,
                base->faction_id, base->pop_size);
            if (new_type != current_type) {
                base->set_specialist_type(spec_idx, new_type);
                const char* name = "???";
                if (new_type >= 0 && new_type < MaxCitizenNum
                    && Citizen[new_type].singular_name) {
                    name = Citizen[new_type].singular_name;
                }
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_SPEC_TYPE_CHANGED), name);
                sr_output(buf, true);
            } else {
                sr_output(loc(SR_SPEC_NO_OTHER_TYPE), true);
            }
        } else if (is_worker_slot(_currentSlot)) {
            sr_output(loc(SR_SPEC_WORKER_NO_TYPE), true);
        }
        return true;

    case VK_RETURN:
    case VK_SPACE:
        // Toggle: worker -> specialist or specialist -> worker
        if (is_worker_slot(_currentSlot)) {
            convert_to_specialist();
        } else {
            convert_to_worker();
        }
        return true;

    case 'S':
        if (!ctrl) {
            announce_summary();
            return true;
        }
        return false;

    case VK_TAB:
        announce_summary();
        return true;

    case 'I':
        if (ctrl) {
            announce_slot();
            return true;
        }
        return false;

    case VK_F1:
        if (ctrl) {
            sr_output(loc(SR_SPEC_HELP), true);
            return true;
        }
        return false;

    case VK_ESCAPE:
        _wantClose = true;
        return true;

    default:
        return false;
    }
}

void RunModal() {
    if (*CurrentBaseID < 0 || *CurrentBaseID >= *BaseCount) return;
    BASE* base = &Bases[*CurrentBaseID];

    // Only allow on own bases
    if (base->faction_id != *CurrentPlayerFaction) return;

    _active = true;
    _wantClose = false;

    // Initialize state
    refresh_counts();
    _currentSlot = 0;

    sr_debug_log("SpecialistHandler::RunModal enter, base=%s pop=%d workers=%d specs=%d\n",
        base->name, (int)base->pop_size, _workerCount, _specCount);

    // Announce summary on open
    announce_summary();

    sr_run_modal_pump(&_wantClose);

    // Final recalculate (no GraphicWin_redraw — base screen redraws itself,
    // and redraw would trigger text hooks that get announced as noise)
    base_compute(1);

    sr_output(loc(SR_SPEC_CLOSE), true);
    sr_debug_log("SpecialistHandler::RunModal exit\n");

    // Set _active = false LAST so announce triggers stay suppressed
    // during base_compute and any pending paint messages
    _active = false;
}

} // namespace SpecialistHandler
