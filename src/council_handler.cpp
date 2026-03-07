/*
 * CouncilHandler — Planetary Council accessibility.
 *
 * Observer pattern: the game handles all council navigation and voting.
 * We announce context (vote counts, governor info, help) and detect
 * council open/close via an inline hook on call_council (0x52C880).
 *
 * GOVVOTE popup: builds popup_list from eligible() factions so arrow
 * navigation reliably announces candidates with position.
 *
 * Vote results: scans captured text for result keywords (CONFIRMED,
 * ELECTED, BESTÄTIGT, GEWÄHLT, etc.) and announces the outcome.
 */

#include "council_handler.h"
#include "screen_reader.h"
#include "localization.h"

// phWnd defined in gui.cpp
extern HWND* phWnd;

namespace CouncilHandler {

static bool _councilActive = false;
static int _councilFaction = -1;
static bool _wasHuman = false;
static char _lastResult[256] = {};

// Vote screen click targets (x coordinates per popup_list entry)
static int _voteClickX[SR_POPUP_LIST_MAX] = {};
static bool _voteListBuilt = false;

bool IsActive() {
    return _councilActive;
}

void OnCouncilOpen(int faction_id) {
    _councilActive = true;
    _councilFaction = faction_id;
    _wasHuman = is_human(faction_id);
    _lastResult[0] = '\0';

    if (!_wasHuman || !sr_is_available()) {
        sr_debug_log("COUNCIL OnOpen: skipped (human=%d sr=%d)", _wasHuman, sr_is_available());
        return;
    }

    sr_debug_log("COUNCIL OnOpen: announcing for faction %d", faction_id);

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
    _lastResult[0] = '\0';
    _voteListBuilt = false;
    if (sr_popup_list.active) sr_popup_list_clear();

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

void OnGovVotePopup() {
    if (!sr_is_available()) return;

    // Build popup_list: Abstain first, then eligible faction leaders
    sr_popup_list_clear();
    strncpy(sr_popup_list.label, "GOVVOTE", sizeof(sr_popup_list.label) - 1);

    // Item 0: Abstain
    strncpy(sr_popup_list.items[0], loc(SR_COUNCIL_ABSTAIN),
            sizeof(sr_popup_list.items[0]) - 1);
    sr_popup_list.count = 1;

    // Add eligible factions in order (1..MaxPlayerNum)
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (!is_alive(i) || is_alien(i)) continue;
        if (!eligible(i)) continue;
        if (sr_popup_list.count >= SR_POPUP_LIST_MAX) break;

        const char* name = sr_game_str(MFactions[i].adj_name_faction);
        strncpy(sr_popup_list.items[sr_popup_list.count], name,
                sizeof(sr_popup_list.items[0]) - 1);
        sr_popup_list.items[sr_popup_list.count][sizeof(sr_popup_list.items[0]) - 1] = '\0';
        sr_popup_list.count++;
    }

    sr_popup_list.index = 0;
    sr_popup_list.active = true;

    // Build candidates summary for announcement
    char names[512] = {};
    int pos = 0;
    for (int i = 0; i < sr_popup_list.count; i++) {
        if (pos > 0) {
            pos += snprintf(names + pos, sizeof(names) - pos, ", ");
        }
        pos += snprintf(names + pos, sizeof(names) - pos, "%s",
                        sr_popup_list.items[i]);
    }

    char buf[1024];
    snprintf(buf, sizeof(buf), loc(SR_COUNCIL_CANDIDATES), names);
    sr_output(buf, true);

    sr_debug_log("COUNCIL GOVVOTE: %d candidates built", sr_popup_list.count);
}

// Result keyword patterns (case-insensitive matching)
static bool sr_contains_ci(const char* text, const char* pattern) {
    if (!text || !pattern) return false;
    int tlen = (int)strlen(text);
    int plen = (int)strlen(pattern);
    if (plen > tlen) return false;
    for (int i = 0; i <= tlen - plen; i++) {
        bool match = true;
        for (int j = 0; j < plen; j++) {
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)pattern[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

void CheckAndAnnounceResults() {
    if (!_councilActive || !_wasHuman || !sr_is_available()) return;

    int count = sr_item_count();
    if (count < 3) return;

    // Scan captured items for result keywords
    // Look for: BESTÄTIGT, GEWÄHLT, CONFIRMED, ELECTED,
    //           ANGENOMMEN, ABGELEHNT, EINSPRUCH, PASSED, FAILED, VETOED
    static const char* result_keywords[] = {
        // German
        "BEST\xC3\x84TIGT",    // BESTÄTIGT (UTF-8)
        "GEW\xC3\x84HLT",     // GEWÄHLT (UTF-8)
        "ANGENOMMEN",
        "ABGELEHNT",
        "EINSPRUCH",
        // English
        "CONFIRMED",
        "ELECTED",
        "PASSED",
        "FAILED",
        "VETOED",
        NULL
    };

    for (int i = 0; i < count; i++) {
        const char* item = sr_item_get(i);
        if (!item || strlen(item) < 4) continue;

        for (int k = 0; result_keywords[k]; k++) {
            if (sr_contains_ci(item, result_keywords[k])) {
                // Avoid re-announcing the same result
                if (strcmp(_lastResult, item) == 0) return;
                strncpy(_lastResult, item, sizeof(_lastResult) - 1);
                _lastResult[sizeof(_lastResult) - 1] = '\0';

                char buf[512];
                snprintf(buf, sizeof(buf), loc(SR_COUNCIL_RESULT),
                         sr_game_str(item));
                sr_output(buf, true);
                sr_debug_log("COUNCIL RESULT: %s", item);
                return;
            }
        }
    }
}

void TryBuildVoteList() {
    if (!_councilActive || !_wasHuman || !sr_is_available()) return;
    if (sr_popup_list.active) return; // already have a list

    int count = sr_item_count();
    if (count < 5) return;

    // Detect vote screen: look for "STIMMEN" or "VOTES" in items
    bool is_vote_screen = false;
    for (int i = 0; i < count; i++) {
        const char* it = sr_item_get(i);
        if (!it) continue;
        if (strstr(it, "STIMMEN") || strstr(it, "VOTES")) {
            is_vote_screen = true;
            break;
        }
    }
    if (!is_vote_screen) return;

    // Build popup_list from game state (reliable, language-independent)
    sr_popup_list_clear();
    strncpy(sr_popup_list.label, "COUNCILVOTE", sizeof(sr_popup_list.label) - 1);

    int player_id = MapWin->cOwner;

    for (int i = 1; i < MaxPlayerNum; i++) {
        if (!is_alive(i) || is_alien(i)) continue;
        if (sr_popup_list.count >= SR_POPUP_LIST_MAX - 1) break;

        int votes = council_votes(i);
        const char* title = sr_game_str(MFactions[i].title_leader);
        const char* name = sr_game_str(MFactions[i].name_leader);

        // Find this faction's x-coordinate from captured text
        int click_x = -1;
        for (int j = 0; j < count; j++) {
            const char* it = sr_item_get(j);
            int jx = sr_item_get_x(j);
            int jy = sr_item_get_y(j);
            if (!it || jx < 0 || jx > 10000 || jy < 140) continue;
            // Match by leader name (case-insensitive substring)
            if (sr_contains_ci(it, MFactions[i].name_leader)) {
                click_x = jx;
                break;
            }
        }

        char entry[256];
        if (i == player_id) {
            snprintf(entry, sizeof(entry), "%s %s: %d (%s)",
                title, name, votes, loc(SR_COUNCIL_YOUR_FACTION));
        } else {
            snprintf(entry, sizeof(entry), "%s %s: %d", title, name, votes);
        }
        int idx = sr_popup_list.count;
        strncpy(sr_popup_list.items[idx], entry, sizeof(sr_popup_list.items[0]) - 1);
        sr_popup_list.items[idx][sizeof(sr_popup_list.items[0]) - 1] = '\0';
        _voteClickX[idx] = click_x;
        sr_popup_list.count++;
    }

    if (sr_popup_list.count > 0) {
        sr_popup_list.index = 0;
        sr_popup_list.active = true;
        _voteListBuilt = true;

        sr_debug_log("COUNCIL VOTE LIST: %d factions", sr_popup_list.count);
        for (int i = 0; i < sr_popup_list.count; i++) {
            sr_debug_log("  [%d] click_x=%d: %s", i, _voteClickX[i],
                sr_popup_list.items[i]);
        }

        // Announce first item
        char buf[300];
        snprintf(buf, sizeof(buf), loc(SR_POPUP_LIST_FMT),
            1, sr_popup_list.count, sr_popup_list.items[0]);
        sr_output(buf, true);
    }
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

    // Enter on vote list → simulate click on selected faction column
    if (wParam == VK_RETURN && _voteListBuilt && sr_popup_list.active
        && sr_popup_list.count > 0 && !ctrl_key_down()) {
        int idx = sr_popup_list.index;
        if (idx >= 0 && idx < sr_popup_list.count) {
            int click_x = _voteClickX[idx] + 25; // center of column
            int click_y = 200; // middle of faction area
            sr_debug_log("COUNCIL VOTE CLICK: idx=%d buf_x=%d buf_y=%d", idx, click_x, click_y);

            // Convert buffer coords to screen coords for SendInput
            POINT pt = { click_x, click_y };
            ClientToScreen(*phWnd, &pt);
            int screen_w = GetSystemMetrics(SM_CXSCREEN);
            int screen_h = GetSystemMetrics(SM_CYSCREEN);
            LONG abs_x = (pt.x * 65536 + screen_w / 2) / screen_w;
            LONG abs_y = (pt.y * 65536 + screen_h / 2) / screen_h;
            sr_debug_log("COUNCIL VOTE CLICK: screen=(%ld,%ld) abs=(%ld,%ld)",
                pt.x, pt.y, abs_x, abs_y);

            // Save cursor, move + click, restore
            POINT old_cursor;
            GetCursorPos(&old_cursor);

            INPUT inputs[3] = {};
            // Move
            inputs[0].type = INPUT_MOUSE;
            inputs[0].mi.dx = abs_x;
            inputs[0].mi.dy = abs_y;
            inputs[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
            // Click down
            inputs[1].type = INPUT_MOUSE;
            inputs[1].mi.dx = abs_x;
            inputs[1].mi.dy = abs_y;
            inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
            // Click up
            inputs[2].type = INPUT_MOUSE;
            inputs[2].mi.dx = abs_x;
            inputs[2].mi.dy = abs_y;
            inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;

            SendInput(3, inputs, sizeof(INPUT));
            SetCursorPos(old_cursor.x, old_cursor.y);
            sr_popup_list_clear();
            _voteListBuilt = false;
        }
        return true;
    }

    return false;
}

} // namespace CouncilHandler
