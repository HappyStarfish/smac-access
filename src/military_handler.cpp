/*
 * MilitaryHandler — F7 Military Status accessible screen.
 *
 * Full modal replacement: intercepts F7 before the game, reads
 * military stats from Faction data, runs own PeekMessage loop.
 */

#include "military_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "gui.h"
#include "faction.h"

static bool _active = false;
static bool _wantClose = false;

// Active unit type list for detail mode
static int _unitIds[512];
static int _unitCounts[512];
static int _unitListCount = 0;
static int _unitIndex = 0;
static bool _detailMode = false;

/// Announce main military summary.
static void AnnounceSummary() {
    int owner = MapWin->cOwner;
    Faction& f = Factions[owner];

    const char* rank_names[] = {
        "Lowest", "Low", "Below Average", "Average",
        "Above Average", "High", "Very High", "Highest"
    };
    int rank_idx = f.ranking;
    if (rank_idx < 0) rank_idx = 0;
    if (rank_idx > 7) rank_idx = 7;

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_MILITARY_SUMMARY),
             f.total_combat_units,
             f.base_count,
             f.pop_total,
             f.mil_strength_1,
             rank_names[rank_idx],
             f.best_weapon_value,
             f.best_armor_value,
             f.best_land_speed,
             f.planet_busters);
    sr_output(buf, true);
}

/// Build list of active unit types.
static void BuildUnitList() {
    int owner = MapWin->cOwner;
    Faction& f = Factions[owner];
    _unitListCount = 0;

    for (int i = 0; i < 512; i++) {
        if (f.units_active[i] > 0 && Units[i].name[0] != '\0') {
            _unitIds[_unitListCount] = i;
            _unitCounts[_unitListCount] = f.units_active[i];
            _unitListCount++;
        }
    }
}

/// Announce a single unit type in detail mode.
static void AnnounceUnit(int index) {
    if (index < 0 || index >= _unitListCount) return;
    int uid = _unitIds[index];

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_MILITARY_UNIT),
             sr_game_str(Units[uid].name),
             index + 1, _unitListCount,
             _unitCounts[index]);
    sr_output(buf, true);
}

/// Announce ranking comparison.
static void AnnounceRankings() {
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_MILITARY_RANKINGS_HEADER));
    sr_output(buf, true);

    for (int rank = 7; rank >= 0; rank--) {
        int fid = FactionRankings[rank];
        if (fid > 0 && fid < MaxPlayerNum && is_alive(fid)) {
            snprintf(buf, sizeof(buf), loc(SR_MILITARY_RANK_ENTRY),
                     rank + 1,
                     sr_game_str(MFactions[fid].noun_faction),
                     Factions[fid].mil_strength_1);
            sr_output(buf, false);
        }
    }
}

namespace MilitaryHandler {

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return true;

    // In detail mode (unit list), handle Up/Down
    if (_detailMode) {
        switch (wParam) {
        case VK_UP:
            if (_unitListCount == 0) return true;
            if (_unitIndex > 0) {
                _unitIndex--;
            } else {
                _unitIndex = _unitListCount - 1;
            }
            AnnounceUnit(_unitIndex);
            return true;

        case VK_DOWN:
            if (_unitListCount == 0) return true;
            if (_unitIndex < _unitListCount - 1) {
                _unitIndex++;
            } else {
                _unitIndex = 0;
            }
            AnnounceUnit(_unitIndex);
            return true;

        case VK_HOME:
            if (_unitListCount == 0) return true;
            _unitIndex = 0;
            AnnounceUnit(_unitIndex);
            return true;

        case VK_END:
            if (_unitListCount == 0) return true;
            _unitIndex = _unitListCount - 1;
            AnnounceUnit(_unitIndex);
            return true;

        case VK_ESCAPE:
            // First Escape exits detail mode, second closes handler
            _detailMode = false;
            sr_output(loc(SR_MILITARY_DETAIL_CLOSED), true);
            return true;

        default:
            break;
        }

        // D key in detail mode = unit type stats
        if (wParam == 'D' && !ctrl_key_down() && !shift_key_down()) {
            if (_unitIndex >= 0 && _unitIndex < _unitListCount) {
                int uid = _unitIds[_unitIndex];
                UNIT* u = &Units[uid];
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

                char buf[512];
                int pos = snprintf(buf, sizeof(buf), loc(SR_MILITARY_UNIT_DETAIL),
                    sr_game_str(u->name), _unitCounts[_unitIndex],
                    chassis_name, weapon_name, atk, armor_name, def,
                    reactor_name, move_speed, hp);

                // Append abilities if any
                if (u->ability_flags) {
                    pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "%s", loc(SR_UNIT_ABILITIES));
                    for (int i = 0; i < MaxAbilityNum; i++) {
                        if (u->ability_flags & (1u << i)) {
                            const char* aname = Ability[i].name
                                ? sr_game_str(Ability[i].name) : "???";
                            pos += snprintf(buf + pos, sizeof(buf) - pos,
                                " %s,", aname);
                            if (pos >= (int)sizeof(buf) - 50) break;
                        }
                    }
                    if (pos > 0 && buf[pos - 1] == ',') {
                        buf[pos - 1] = '.';
                    }
                }
                sr_output(buf, true);
            }
            return true;
        }

        // F1 in detail mode
        if (wParam == VK_F1 && ctrl_key_down()) {
            sr_output(loc(SR_MILITARY_HELP), true);
            return true;
        }

        return true; // consume all keys in detail mode
    }

    switch (wParam) {
    case VK_ESCAPE:
        _wantClose = true;
        return true;

    case VK_F1:
        if (ctrl_key_down()) {
            sr_output(loc(SR_MILITARY_HELP), true);
        }
        return true;

    default:
        break;
    }

    // S key = summary / ranking comparison
    if (wParam == 'S' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceRankings();
        return true;
    }

    // D key = detail unit list
    if (wParam == 'D' && !ctrl_key_down() && !shift_key_down()) {
        BuildUnitList();
        if (_unitListCount == 0) {
            sr_output(loc(SR_MILITARY_NO_UNITS), true);
            return true;
        }
        _detailMode = true;
        _unitIndex = 0;

        char buf[128];
        snprintf(buf, sizeof(buf), loc(SR_MILITARY_DETAIL_OPEN), _unitListCount);
        sr_output(buf, true);
        AnnounceUnit(0);
        return true;
    }

    return true;
}

void RunModal() {
    _active = true;
    _wantClose = false;
    _detailMode = false;
    _unitListCount = 0;
    _unitIndex = 0;

    sr_output(loc(SR_MILITARY_OPEN), true);
    AnnounceSummary();

    sr_run_modal_pump(&_wantClose);

    _active = false;
    _detailMode = false;
    _unitListCount = 0;
    sr_output(loc(SR_MILITARY_CLOSED), true);
}

} // namespace MilitaryHandler
