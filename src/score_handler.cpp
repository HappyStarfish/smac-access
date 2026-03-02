/*
 * ScoreHandler — F8 Score/Rankings accessible screen.
 *
 * Full modal replacement: intercepts F8 before the game, reads
 * FactionRankings[] and faction stats, runs own PeekMessage loop.
 */

#include "score_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "gui.h"
#include "faction.h"

static bool _active = false;
static bool _wantClose = false;

// Ranked list of alive factions (highest rank first)
static int _rankedFactions[MaxPlayerNum];
static int _rankedCount = 0;
static int _currentIndex = 0;

/// Build ranked faction list from FactionRankings (7=highest to 0=lowest).
static void BuildRankedList() {
    _rankedCount = 0;
    for (int rank = 7; rank >= 0; rank--) {
        int fid = FactionRankings[rank];
        if (fid > 0 && fid < MaxPlayerNum && is_alive(fid)) {
            _rankedFactions[_rankedCount++] = fid;
        }
    }
}

/// Announce a faction at the given list index.
static void AnnounceFaction(int index) {
    if (index < 0 || index >= _rankedCount) return;
    int fid = _rankedFactions[index];
    Faction& f = Factions[fid];

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_SCORE_FACTION),
             index + 1,
             sr_game_str(MFactions[fid].noun_faction),
             index + 1, _rankedCount,
             f.base_count, f.pop_total, f.tech_achieved);
    sr_output(buf, true);
}

/// Announce detail for the faction at the given list index.
static void AnnounceDetail(int index) {
    if (index < 0 || index >= _rankedCount) return;
    int fid = _rankedFactions[index];
    int player = MapWin->cOwner;
    Faction& f = Factions[fid];

    // Get diplomacy status relative to player
    const char* diplo_str;
    if (fid == player) {
        diplo_str = "";
    } else if (has_treaty(player, fid, DIPLO_PACT)) {
        diplo_str = loc(SR_DIPLO_STATUS_PACT);
    } else if (has_treaty(player, fid, DIPLO_TREATY)) {
        diplo_str = loc(SR_DIPLO_STATUS_TREATY);
    } else if (has_treaty(player, fid, DIPLO_TRUCE)) {
        diplo_str = loc(SR_DIPLO_STATUS_TRUCE);
    } else if (has_treaty(player, fid, DIPLO_VENDETTA)) {
        diplo_str = loc(SR_DIPLO_STATUS_VENDETTA);
    } else {
        diplo_str = loc(SR_DIPLO_STATUS_NONE);
    }

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_SCORE_DETAIL),
             sr_game_str(MFactions[fid].noun_faction),
             f.base_count, f.pop_total, f.tech_achieved,
             f.mil_strength_1, diplo_str);
    sr_output(buf, true);
}

/// Announce player's own ranking summary.
static void AnnouncePlayerSummary() {
    int owner = MapWin->cOwner;
    Faction& f = Factions[owner];

    // Find player's rank position in our list
    int pos = -1;
    for (int i = 0; i < _rankedCount; i++) {
        if (_rankedFactions[i] == owner) {
            pos = i;
            break;
        }
    }

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_SCORE_PLAYER),
             sr_game_str(MFactions[owner].noun_faction),
             pos + 1, _rankedCount,
             f.base_count, f.pop_total, f.tech_achieved);
    sr_output(buf, true);
}

namespace ScoreHandler {

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return true;

    switch (wParam) {
    case VK_UP:
        if (_rankedCount == 0) return true;
        if (_currentIndex > 0) {
            _currentIndex--;
        } else {
            _currentIndex = _rankedCount - 1;
        }
        AnnounceFaction(_currentIndex);
        return true;

    case VK_DOWN:
        if (_rankedCount == 0) return true;
        if (_currentIndex < _rankedCount - 1) {
            _currentIndex++;
        } else {
            _currentIndex = 0;
        }
        AnnounceFaction(_currentIndex);
        return true;

    case VK_HOME:
        if (_rankedCount == 0) return true;
        _currentIndex = 0;
        AnnounceFaction(_currentIndex);
        return true;

    case VK_END:
        if (_rankedCount == 0) return true;
        _currentIndex = _rankedCount - 1;
        AnnounceFaction(_currentIndex);
        return true;

    case VK_ESCAPE:
        _wantClose = true;
        return true;

    case VK_F1:
        if (ctrl_key_down()) {
            sr_output(loc(SR_SCORE_HELP), true);
        }
        return true;

    default:
        break;
    }

    if (wParam == 'D' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceDetail(_currentIndex);
        return true;
    }

    if (wParam == 'S' && !ctrl_key_down() && !shift_key_down()) {
        AnnouncePlayerSummary();
        return true;
    }

    return true;
}

void RunModal() {
    BuildRankedList();
    _active = true;
    _wantClose = false;
    _currentIndex = 0;

    if (_rankedCount == 0) {
        sr_output(loc(SR_SCORE_EMPTY), true);
        _active = false;
        return;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_SCORE_OPEN), _rankedCount);
    sr_output(buf, true);

    AnnounceFaction(0);

    sr_run_modal_pump(&_wantClose);

    _active = false;
    _rankedCount = 0;
    sr_output(loc(SR_SCORE_CLOSED), true);
}

} // namespace ScoreHandler
