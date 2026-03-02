/*
 * OrbitalHandler — F6 Orbital Status accessible screen.
 *
 * Full modal replacement: intercepts F6 before the game, reads
 * satellite counts from Faction data, runs own PeekMessage loop.
 */

#include "orbital_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "gui.h"
#include "faction.h"

static bool _active = false;
static bool _wantClose = false;

/// Announce player's orbital summary.
static void AnnounceSummary() {
    int owner = MapWin->cOwner;
    Faction& f = Factions[owner];

    int total = f.satellites_nutrient + f.satellites_mineral
              + f.satellites_energy + f.satellites_ODP;

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_ORBITAL_SUMMARY),
             total,
             f.satellites_nutrient,
             f.satellites_mineral,
             f.satellites_energy,
             f.satellites_ODP);
    sr_output(buf, true);
}

/// Announce comparison with other factions (known intel).
static void AnnounceDetail() {
    int owner = MapWin->cOwner;

    // Player first
    Faction& pf = Factions[owner];
    int player_total = pf.satellites_nutrient + pf.satellites_mineral
                     + pf.satellites_energy + pf.satellites_ODP;

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_ORBITAL_PLAYER),
             sr_game_str(MFactions[owner].noun_faction), player_total);
    sr_output(buf, true);

    // Other alive factions
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (i == owner || !is_alive(i)) continue;

        // Only show if we have intel (treaty or better, or infiltrator)
        Faction& of = Factions[i];
        int total = of.satellites_nutrient + of.satellites_mineral
                  + of.satellites_energy + of.satellites_ODP;

        snprintf(buf, sizeof(buf), loc(SR_ORBITAL_FACTION),
                 sr_game_str(MFactions[i].noun_faction), total);
        sr_output(buf, false);
    }
}

namespace OrbitalHandler {

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
            sr_output(loc(SR_ORBITAL_HELP), true);
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

    return true;
}

void RunModal() {
    _active = true;
    _wantClose = false;

    sr_output(loc(SR_ORBITAL_OPEN), true);
    AnnounceSummary();

    sr_run_modal_pump(&_wantClose);

    _active = false;
    sr_output(loc(SR_ORBITAL_CLOSED), true);
}

} // namespace OrbitalHandler
