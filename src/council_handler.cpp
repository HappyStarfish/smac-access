/*
 * CouncilHandler — Planetary Council accessibility.
 *
 * Observer pattern: the game handles all council navigation and voting.
 * We announce context (vote counts, governor info, help) and detect
 * council open/close via an inline hook on call_council (0x52C880).
 */

#include "council_handler.h"
#include "screen_reader.h"
#include "localization.h"

namespace CouncilHandler {

static bool _councilActive = false;
static int _councilFaction = -1;
static bool _wasHuman = false;

bool IsActive() {
    return _councilActive;
}

void OnCouncilOpen(int faction_id) {
    _councilActive = true;
    _councilFaction = faction_id;
    _wasHuman = is_human(faction_id);

    if (!_wasHuman || !sr_is_available()) {
        return;
    }

    // Build governor string
    char gov_buf[256];
    int gov = *GovernorFaction;
    if (gov > 0 && gov < MaxPlayerNum) {
        snprintf(gov_buf, sizeof(gov_buf), loc(SR_COUNCIL_GOVERNOR),
                 sr_game_str(MFactions[gov].adj_name_faction));
    } else {
        snprintf(gov_buf, sizeof(gov_buf), "%s", loc(SR_COUNCIL_NO_GOVERNOR));
    }

    // Announce: "Planetary Council, [Faction]. Governor: [Name/None]."
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_COUNCIL_OPEN),
             sr_game_str(MFactions[faction_id].adj_name_faction), gov_buf);
    sr_output(buf, true);

    // Queue vote summary (non-interrupting)
    AnnounceVoteSummary();
}

void OnCouncilClose() {
    bool wasHuman = _wasHuman;
    _councilActive = false;
    _councilFaction = -1;
    _wasHuman = false;

    if (wasHuman && sr_is_available()) {
        sr_output(loc(SR_COUNCIL_CLOSED), true);
    }
}

void AnnounceVoteSummary() {
    if (!sr_is_available()) return;

    char buf[1024];
    int offset = 0;
    int total_votes = 0;
    int player_votes = 0;
    int player_id = MapWin->cOwner;

    // Collect votes for all living non-alien factions
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (!is_alive(i) || is_alien(i)) continue;
        int votes = council_votes(i);
        total_votes += votes;

        char line[128];
        snprintf(line, sizeof(line), loc(SR_COUNCIL_VOTE_LINE),
                 sr_game_str(MFactions[i].adj_name_faction), votes);

        int len = (int)strlen(line);
        if (offset + len + 2 < (int)sizeof(buf)) {
            if (offset > 0) {
                buf[offset++] = ' ';
            }
            memcpy(buf + offset, line, len);
            offset += len;
            buf[offset++] = '.';
        }

        if (i == player_id) {
            player_votes = votes;
        }
    }
    buf[offset] = '\0';

    // Announce per-faction votes (queued)
    if (offset > 0) {
        sr_output(buf, false);
    }

    // Total and majority
    int majority = total_votes / 2 + 1;
    char summary[256];
    snprintf(summary, sizeof(summary), loc(SR_COUNCIL_TOTAL),
             total_votes, majority);
    sr_output(summary, false);

    // Player's votes
    char yours[128];
    snprintf(yours, sizeof(yours), loc(SR_COUNCIL_YOUR_VOTES),
             player_votes, total_votes);
    sr_output(yours, false);
}

void AnnouncePreCouncilInfo() {
    if (!sr_is_available()) return;

    int player_id = MapWin->cOwner;

    // Check if council can be called
    int can_call = can_call_council(player_id, 0);
    if (can_call) {
        sr_output(loc(SR_COUNCIL_CAN_CALL), true);
    } else {
        sr_output(loc(SR_COUNCIL_CANNOT_CALL), true);
    }

    // Announce current governor
    int gov = *GovernorFaction;
    if (gov > 0 && gov < MaxPlayerNum) {
        char buf[256];
        snprintf(buf, sizeof(buf), loc(SR_COUNCIL_GOVERNOR),
                 sr_game_str(MFactions[gov].adj_name_faction));
        sr_output(buf, false);
    } else {
        sr_output(loc(SR_COUNCIL_NO_GOVERNOR), false);
    }

    // Vote summary
    AnnounceVoteSummary();
}

bool HandleKey(UINT msg, WPARAM wParam) {
    if (!IsActive() || !sr_is_available()) return false;
    if (msg != WM_KEYDOWN) return false;

    // S or Tab (no modifiers) → vote summary
    if ((wParam == 'S' || wParam == VK_TAB)
        && !ctrl_key_down() && !shift_key_down() && !alt_key_down()) {
        AnnounceVoteSummary();
        return true;
    }

    // Ctrl+F1 → help
    if (wParam == VK_F1 && ctrl_key_down() && !shift_key_down() && !alt_key_down()) {
        sr_output(loc(SR_COUNCIL_HELP), true);
        return true;
    }

    return false;
}

} // namespace CouncilHandler
