/*
 * LabsHandler — F2 Labs/Research Status accessible screen.
 *
 * Full modal replacement: intercepts F2 before the game, builds
 * research summary from Faction data, runs own PeekMessage loop.
 */

#include "labs_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "gui.h"

static bool _active = false;
static bool _wantClose = false;
static bool _showDetail = false;

/// Announce the main research summary.
static void AnnounceSummary() {
    int owner = MapWin->cOwner;
    Faction& f = Factions[owner];

    int tech_id = f.tech_research_id;
    const char* tech_name = (tech_id >= 0 && Tech[tech_id].name)
        ? sr_game_str(Tech[tech_id].name) : "None";

    int labs = f.labs_total;
    int accumulated = f.tech_accumulated;
    int cost = f.tech_cost;
    int turns = -1;
    if (labs > 0 && cost > accumulated) {
        turns = (cost - accumulated + labs - 1) / labs;
    }

    int alloc_labs = f.SE_alloc_labs;
    int alloc_psych = f.SE_alloc_psych;
    int alloc_econ = 100 - alloc_labs - alloc_psych;
    int techs = f.tech_achieved;

    char buf[512];
    if (turns >= 0) {
        snprintf(buf, sizeof(buf), loc(SR_LABS_SUMMARY),
                 tech_name, accumulated, cost, turns,
                 labs, techs, alloc_econ, alloc_psych, alloc_labs);
    } else {
        snprintf(buf, sizeof(buf), loc(SR_LABS_SUMMARY_DONE),
                 tech_name, labs, techs, alloc_econ, alloc_psych, alloc_labs);
    }
    sr_output(buf, true);
}

/// Announce per-base labs breakdown.
static void AnnounceDetail() {
    int owner = MapWin->cOwner;
    int total_labs = 0;
    int base_count = 0;

    // First pass: count and total
    for (int i = 0; i < *BaseCount; i++) {
        if (Bases[i].faction_id == owner) {
            total_labs += Bases[i].labs_total;
            base_count++;
        }
    }

    if (base_count == 0) {
        sr_output(loc(SR_LABS_NO_BASES), true);
        return;
    }

    char header[128];
    snprintf(header, sizeof(header), loc(SR_LABS_DETAIL_HEADER),
             base_count, total_labs);
    sr_output(header, true);

    // Second pass: announce each base
    int idx = 0;
    for (int i = 0; i < *BaseCount; i++) {
        if (Bases[i].faction_id == owner) {
            idx++;
            char buf[256];
            snprintf(buf, sizeof(buf), loc(SR_LABS_DETAIL_BASE),
                     sr_game_str(Bases[i].name),
                     idx, base_count,
                     Bases[i].labs_total);
            sr_output(buf, false);
        }
    }
}

namespace LabsHandler {

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return true;

    switch (wParam) {
    case VK_ESCAPE:
        _wantClose = true;
        return true;

    case VK_F1:
        if (ctrl_key_down()) {
            sr_output(loc(SR_LABS_HELP), true);
        }
        return true;

    default:
        break;
    }

    if (wParam == 'S' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceSummary();
        return true;
    }

    if (wParam == 'D' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceDetail();
        return true;
    }

    return true; // consume all keys while modal
}

void RunModal() {
    _active = true;
    _wantClose = false;
    _showDetail = false;

    sr_output(loc(SR_LABS_OPEN), true);
    AnnounceSummary();

    sr_run_modal_pump(&_wantClose);

    _active = false;
    sr_output(loc(SR_LABS_CLOSED), true);
}

} // namespace LabsHandler
