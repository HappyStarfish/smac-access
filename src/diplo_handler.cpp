/*
 * DiplomacyHandler — announces diplomacy session open/close and provides
 * relationship summary during active diplomacy dialogs.
 *
 * This handler does NOT replace communicate() or its modal loop.
 * It observes DiploWinState and diplo_second_faction to detect when
 * a diplomacy session is active and adds screen reader announcements.
 */

#include "diplo_handler.h"
#include "screen_reader.h"
#include "localization.h"

namespace DiplomacyHandler {

static bool _wasActive = false;
static int _lastFaction = -1;

bool IsActive() {
    return *DiploWinState != 0;
}

/// Get the treaty status string for the relationship between two factions.
static const char* get_treaty_status(int faction1, int faction2) {
    if (has_treaty(faction1, faction2, DIPLO_PACT)) {
        return loc(SR_DIPLO_STATUS_PACT);
    } else if (has_treaty(faction1, faction2, DIPLO_TREATY)) {
        return loc(SR_DIPLO_STATUS_TREATY);
    } else if (has_treaty(faction1, faction2, DIPLO_TRUCE)) {
        return loc(SR_DIPLO_STATUS_TRUCE);
    } else if (has_treaty(faction1, faction2, DIPLO_VENDETTA)) {
        return loc(SR_DIPLO_STATUS_VENDETTA);
    }
    return loc(SR_DIPLO_STATUS_NONE);
}

/// Get patience description for AI-to-player relationship.
static const char* get_patience_str(int player, int other) {
    if (player < 0 || player >= MaxPlayerNum
        || other < 0 || other >= MaxPlayerNum) {
        return "";
    }
    int8_t patience = Factions[other].diplo_patience[player];
    if (patience <= 0) {
        return loc(SR_DIPLO_PATIENCE_THIN);
    } else if (patience <= 3) {
        return loc(SR_DIPLO_PATIENCE_WEARING);
    }
    return loc(SR_DIPLO_PATIENCE_OK);
}

/// Build a relationship summary string for S/Tab.
static void build_summary(int player, int other, char* buf, int bufsize) {
    if (other < 0 || other >= MaxPlayerNum
        || player < 0 || player >= MaxPlayerNum) {
        buf[0] = '\0';
        return;
    }

    const char* treaty = get_treaty_status(player, other);

    // Patience
    char patience_buf[128];
    snprintf(patience_buf, sizeof(patience_buf),
             loc(SR_DIPLO_PATIENCE), get_patience_str(player, other));

    // Extra info (surrendered, infiltrator)
    char extra[256];
    extra[0] = '\0';
    if (has_treaty(other, player, DIPLO_HAVE_SURRENDERED)) {
        snprintf(extra, sizeof(extra), "%s", loc(SR_DIPLO_SURRENDERED));
    }
    if (has_treaty(player, other, DIPLO_HAVE_INFILTRATOR)) {
        if (extra[0]) {
            int len = strlen(extra);
            snprintf(extra + len, sizeof(extra) - len, ". %s",
                     loc(SR_DIPLO_INFILTRATOR));
        } else {
            snprintf(extra, sizeof(extra), "%s", loc(SR_DIPLO_INFILTRATOR));
        }
    }
    if (!extra[0]) {
        // If no extra info, put an empty string
        extra[0] = '\0';
    }

    snprintf(buf, bufsize, loc(SR_DIPLO_SUMMARY),
             treaty, patience_buf, extra);
}

void OnTimer() {
    bool active = IsActive();

    if (active && !_wasActive) {
        // Diplomacy just opened
        int faction2 = *diplo_second_faction;
        _lastFaction = faction2;
        if (faction2 >= 0 && faction2 < MaxPlayerNum) {
            char buf[512];
            snprintf(buf, sizeof(buf), loc(SR_DIPLO_OPEN),
                     sr_game_str(MFactions[faction2].adj_name_faction));
            sr_output(buf, true);
            sr_debug_log("DIPLO-OPEN faction=%d name=%s\n",
                         faction2, MFactions[faction2].adj_name_faction);
        }
    } else if (!active && _wasActive) {
        // Diplomacy just closed
        sr_output(loc(SR_DIPLO_CLOSED), true);
        sr_debug_log("DIPLO-CLOSE\n");
        _lastFaction = -1;
    }

    _wasActive = active;
}

bool HandleKey(UINT msg, WPARAM wParam) {
    if (!IsActive() || !sr_is_available()) {
        return false;
    }

    if (msg != WM_KEYDOWN) {
        return false;
    }

    int player = MapWin->cOwner;
    int other = *diplo_second_faction;

    // S or Tab: Relationship summary
    if (wParam == 'S' || wParam == VK_TAB) {
        if (!ctrl_key_down() && !shift_key_down() && !alt_key_down()) {
            char buf[512];
            build_summary(player, other, buf, sizeof(buf));
            if (buf[0]) {
                sr_output(buf, true);
            }
            return true;
        }
    }

    // Ctrl+F1: Help
    if (wParam == VK_F1 && ctrl_key_down()) {
        sr_output(loc(SR_DIPLO_HELP), true);
        return true;
    }

    return false;
}

void RunCommlink() {
    if (!sr_is_available() || !MapWin) return;

    int player = MapWin->cOwner;
    if (player < 0 || player >= MaxPlayerNum) return;

    // Build list of factions we have commlink with
    struct FactionEntry { int id; };
    FactionEntry entries[MaxPlayerNum];
    int total = 0;

    for (int i = 1; i < MaxPlayerNum; i++) {
        if (i == player) continue;
        if (!is_alive(i)) continue;
        if (!has_treaty(player, i, DIPLO_COMMLINK)) continue;
        entries[total++] = {i};
    }

    if (total == 0) {
        sr_output(loc(SR_DIPLO_COMMLINK_EMPTY), true);
        return;
    }

    int index = 0;
    bool want_close = false;
    bool confirmed = false;

    // Announce open
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_DIPLO_COMMLINK_OPEN), total);
    sr_output(buf, true);

    // Announce first item
    {
        int fid = entries[0].id;
        const char* treaty = get_treaty_status(player, fid);
        snprintf(buf, sizeof(buf), loc(SR_DIPLO_COMMLINK_ITEM),
            1, total, sr_game_str(MFactions[fid].adj_name_faction),
            treaty, Factions[fid].pop_total);
        sr_output(buf, false);
    }

    // Modal loop
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
                    int fid = entries[index].id;
                    const char* treaty = get_treaty_status(player, fid);
                    snprintf(buf, sizeof(buf), loc(SR_DIPLO_COMMLINK_ITEM),
                        index + 1, total,
                        sr_game_str(MFactions[fid].adj_name_faction),
                        treaty, Factions[fid].pop_total);
                    sr_output(buf, true);
                } else if (k == VK_DOWN) {
                    index = (index + 1) % total;
                    int fid = entries[index].id;
                    const char* treaty = get_treaty_status(player, fid);
                    snprintf(buf, sizeof(buf), loc(SR_DIPLO_COMMLINK_ITEM),
                        index + 1, total,
                        sr_game_str(MFactions[fid].adj_name_faction),
                        treaty, Factions[fid].pop_total);
                    sr_output(buf, true);
                } else if (k == 'S' || k == VK_TAB) {
                    // Summary of current faction
                    int fid = entries[index].id;
                    build_summary(player, fid, buf, sizeof(buf));
                    if (buf[0]) sr_output(buf, true);
                }
            }
        } else {
            Sleep(10);
        }
    }

    // Drain WM_CHAR/WM_KEYUP to prevent leakage
    MSG drain;
    while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
    while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

    if (confirmed && index >= 0 && index < total) {
        int fid = entries[index].id;
        snprintf(buf, sizeof(buf), loc(SR_DIPLO_COMMLINK_CONTACT),
            sr_game_str(MFactions[fid].adj_name_faction));
        sr_output(buf, true);
        sr_debug_log("COMMLINK contact faction=%d\n", fid);
        // Set up diplomacy state and call communicate directly.
        // commlink_attempter shows its own faction popup (not accessible),
        // commlink_attempt didn't open diplomacy. Direct communicate works
        // because it sets up DiploWinState internally.
        *diplo_second_faction = fid;
        diplo_lock(fid);
        diplomacy_caption(player, fid);
        communicate(player, fid, 1);
    }
}

} // namespace DiplomacyHandler
