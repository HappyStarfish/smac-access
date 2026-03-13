/*
 * Council vote hooks: governor election, proposal voting, buy-votes.
 * Extracted from screen_reader.cpp — Pattern A full modal replacements.
 *
 * Three hooks:
 *   - call_council (0x52C880): wraps council flow with OnOpen/OnClose
 *   - gfx_event_loop (0x602600): intercepts governor vote + buy-votes menu
 *   - council_buy_votes (0x427230): intercepts proposal vote + AI buy-votes
 */

#include "main.h"
#include "sr_council_hooks.h"
#include "sr_internal.h"
#include "screen_reader.h"
#include "council_handler.h"
#include "localization.h"
#include "modal_utils.h"

// ========== Planetary Council Hook ==========

static fp_1int sr_orig_call_council = NULL;

static int __cdecl sr_hook_call_council(int faction_id) {
    sr_debug_log("COUNCIL HOOK: call_council(%d) human=%d sr=%d",
        faction_id, is_human(faction_id), sr_is_available());
    CouncilHandler::OnCouncilOpen(faction_id);
    sr_debug_log("COUNCIL HOOK: OnCouncilOpen done, IsActive=%d", CouncilHandler::IsActive());
    int result = sr_orig_call_council(faction_id);
    sr_debug_log("COUNCIL HOOK: call_council returned %d", result);
    CouncilHandler::OnCouncilClose();
    return result;
}

// ========== Council Vote Screen Hook ==========
// Replaces the graphical click-to-vote event loop (0x602600) at call site 0x52D419.
// See docs/council-vote-reversing.md for full reverse engineering notes.
//
// Pattern A: full modal replacement. Builds list of eligible candidates,
// runs own PeekMessage loop with keyboard navigation.
// Return: >0 = faction_id voted for, 0 = abstain, <0 = cancel.

typedef int (__fastcall *FGfxEventLoop)(void* this_ptr, void* edx, int a1, void* callback);
static FGfxEventLoop sr_orig_gfx_event_loop = NULL;

// Accessible vote modal: keyboard-navigable list of eligible candidates.
static int sr_council_vote_modal(int voter) {
    // Build candidate list: entry 0 = Abstain, rest = eligible factions
    struct VoteEntry {
        int faction_id;  // 0 = abstain, 1-7 = faction
        char label[256];
    };
    VoteEntry entries[MaxPlayerNum + 1];
    int count = 0;

    // Entry 0: Abstain
    snprintf(entries[0].label, sizeof(entries[0].label), "%s", loc(SR_COUNCIL_ABSTAIN));
    entries[0].faction_id = 0;
    count = 1;

    // Add eligible factions
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (!is_alive(i) || is_alien(i)) continue;
        if (!eligible(i)) continue;
        if (count >= MaxPlayerNum + 1) break;

        int votes = council_votes(i);
        const char* title = sr_game_str(MFactions[i].title_leader);
        const char* name = sr_game_str(MFactions[i].name_leader);

        if (i == voter) {
            snprintf(entries[count].label, sizeof(entries[count].label),
                "%s %s: %d (%s)", title, name, votes, loc(SR_COUNCIL_YOUR_FACTION));
        } else {
            snprintf(entries[count].label, sizeof(entries[count].label),
                "%s %s: %d", title, name, votes);
        }
        entries[count].faction_id = i;
        count++;
    }

    if (count == 0) {
        sr_debug_log("COUNCIL-VOTE-MODAL: no candidates found");
        return -1;
    }

    int index = 0;
    bool want_close = false;
    int result = -1;

    // Announce first entry
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_COUNCIL_VOTE_PROMPT), entries[0].label);
    sr_output(buf, true);

    sr_debug_log("COUNCIL-VOTE-MODAL: starting, %d entries (voter=%d)", count, voter);

    // Modal message loop
    MSG msg;
    while (!want_close) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
            if (msg.message == WM_KEYDOWN) {
                WPARAM key = msg.wParam;
                if (sr_modal_handle_utility_key(key)) continue;
                bool announce = false;

                switch (key) {
                case VK_UP:
                    index = (index + count - 1) % count;
                    announce = true;
                    break;
                case VK_DOWN:
                    index = (index + 1) % count;
                    announce = true;
                    break;
                case VK_RETURN:
                    result = entries[index].faction_id;
                    want_close = true;
                    break;
                case VK_ESCAPE:
                    result = 0; // abstain on escape
                    want_close = true;
                    break;
                case 'S':
                case VK_TAB:
                    if (!(GetKeyState(VK_CONTROL) & 0x8000)) {
                        CouncilHandler::AnnounceVoteSummary();
                    }
                    break;
                }

                if (announce && !want_close) {
                    snprintf(buf, sizeof(buf), "%d / %d: %s",
                        index + 1, count, entries[index].label);
                    sr_output(buf, true);
                }
                continue;
            }
            if (msg.message == WM_KEYUP || msg.message == WM_CHAR
                || msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) {
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Sleep(10);
        }
    }

    // Drain leftover keyboard messages
    MSG drain;
    while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
    while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

    // Announce result
    if (result > 0) {
        int fi = result;
        const char* name = sr_game_str(MFactions[fi].adj_name_faction);
        snprintf(buf, sizeof(buf), loc(SR_COUNCIL_VOTE_SELECTED), name);
        sr_output(buf, true);
        sr_debug_log("COUNCIL-VOTE-MODAL: voted for faction %d (%s)", fi, name);
    } else {
        sr_output(loc(SR_COUNCIL_ABSTAIN), true);
        sr_debug_log("COUNCIL-VOTE-MODAL: abstain (result=%d)", result);
    }

    return result;
}

static int __fastcall sr_council_vote_hook(void* this_ptr, void* edx_unused,
                                           int arg1, void* callback) {
    // This hooks ALL calls to 0x602600 (120+ sites). Only intercept during council.
    if (!CouncilHandler::IsActive() || !sr_is_available() || sr_all_disabled()) {
        return sr_orig_gfx_event_loop(this_ptr, edx_unused, arg1, callback);
    }

    int voter = *(int*)0x939284;
    if (!is_human(voter)) {
        return sr_orig_gfx_event_loop(this_ptr, edx_unused, arg1, callback);
    }

    // Buy-votes menu: BUYVOTEMENU popup detected during buy_council_vote.
    // Show accessible modal instead of passing through to the game's visual UI.
    if (CouncilHandler::InBuyVotes() && CouncilHandler::ConsumeBuyMenuFlag()) {
        CouncilHandler::ConsumePopupFlag(); // also clear popup flag
        sr_debug_log("GFX-HOOK: BUYVOTEMENU intercepted, showing buy-votes modal");

        // Read game globals set by buy_council_vote before calling 0x602600:
        int target = *diplo_second_faction;
        int energy_price = ParseNumTable[0]; // $NUM0 = energy cost
        int techs[4] = {
            *diplo_entry_id,       // 0x93FAA8 = tech slot 0
            *diplo_tech_id2,       // 0x93FA18 = tech slot 1
            *(int*)0x93FA1C,       // tech slot 2
            *(int*)0x93FA28,       // tech slot 3
        };

        // Count available techs and build tech name string
        int num_techs = 0;
        char tech_names[512] = {};
        int tn_offset = 0;
        for (int t = 0; t < 4; t++) {
            if (techs[t] < 0) break;
            const char* tname = (Tech[techs[t]].name)
                ? sr_game_str(Tech[techs[t]].name) : "???";
            if (num_techs > 0 && tn_offset + 4 < (int)sizeof(tech_names)) {
                memcpy(tech_names + tn_offset, ", ", 2);
                tn_offset += 2;
            }
            int tlen = (int)strlen(tname);
            if (tn_offset + tlen < (int)sizeof(tech_names)) {
                memcpy(tech_names + tn_offset, tname, tlen);
                tn_offset += tlen;
            }
            num_techs++;
        }
        tech_names[tn_offset] = '\0';

        sr_debug_log("GFX-HOOK: buy-votes target=%d price=%d techs=%d (%s)",
            target, energy_price, num_techs, tech_names);

        // Build options: Cancel, Pay energy, [Give techs]
        struct BuyOption { int ret_val; char label[256]; };
        BuyOption options[3];
        int opt_count = 0;

        // Option 0: Cancel
        snprintf(options[opt_count].label, sizeof(options[opt_count].label),
            "%s", loc(SR_COUNCIL_BUY_CANCEL));
        options[opt_count].ret_val = 0;
        opt_count++;

        // Option 1: Pay energy
        snprintf(options[opt_count].label, sizeof(options[opt_count].label),
            loc(SR_COUNCIL_BUY_ENERGY), energy_price);
        options[opt_count].ret_val = 1;
        opt_count++;

        // Option 2: Give techs (only if techs available)
        if (num_techs > 0) {
            const char* fmt = (num_techs == 1)
                ? loc(SR_COUNCIL_BUY_TECH)
                : loc(SR_COUNCIL_BUY_TECHS);
            snprintf(options[opt_count].label, sizeof(options[opt_count].label),
                fmt, tech_names);
            // Return value: 1 + num_techs (so game transfers all available techs)
            options[opt_count].ret_val = 1 + num_techs;
            opt_count++;
        }

        // Announce prompt: "FactionName: offer text"
        const char* target_name = (target > 0 && target < MaxPlayerNum)
            ? sr_game_str(MFactions[target].adj_name_faction) : "???";
        char prompt[512];
        snprintf(prompt, sizeof(prompt), loc(SR_COUNCIL_BUY_PROMPT),
            target_name, options[1].label);
        sr_output(prompt, false);

        // Modal loop
        int index = 0;
        bool want_close = false;
        int result = 0;
        char buf[512];

        MSG msg;
        while (!want_close) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    PostQuitMessage((int)msg.wParam);
                    break;
                }
                if (msg.message == WM_KEYDOWN) {
                    WPARAM key = msg.wParam;
                    if (sr_modal_handle_utility_key(key)) continue;
                    bool announce = false;

                    switch (key) {
                    case VK_UP:
                        index = (index + opt_count - 1) % opt_count;
                        announce = true;
                        break;
                    case VK_DOWN:
                        index = (index + 1) % opt_count;
                        announce = true;
                        break;
                    case VK_RETURN:
                        result = options[index].ret_val;
                        want_close = true;
                        break;
                    case VK_ESCAPE:
                        result = 0; // cancel
                        want_close = true;
                        break;
                    }

                    if (announce && !want_close) {
                        snprintf(buf, sizeof(buf), "%d / %d: %s",
                            index + 1, opt_count, options[index].label);
                        sr_output(buf, true);
                    }
                    continue;
                }
                if (msg.message == WM_KEYUP || msg.message == WM_CHAR
                    || msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) {
                    continue;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                Sleep(10);
            }
        }

        // Drain leftover keyboard messages
        MSG drain;
        while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
        while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

        // Announce result
        if (result == 1) {
            snprintf(buf, sizeof(buf), loc(SR_COUNCIL_BUY_PAID),
                energy_price, target_name);
            sr_output(buf, true);
        } else if (result >= 2) {
            snprintf(buf, sizeof(buf), loc(SR_COUNCIL_BUY_GAVE_TECH), target_name);
            sr_output(buf, true);
        } else {
            sr_output(loc(SR_COUNCIL_BUY_DECLINED), true);
        }

        sr_debug_log("GFX-HOOK: buy-votes modal result=%d (index=%d)", result, index);
        return result;
    }

    // If a BasePop (popup) was just loaded, this 0x602600 call is the event loop
    // for that popup (COUNCILISSUES, COUNCILRECENTPROP, etc.). Pass through so
    // the game handles it. Only intercept when NO popup is pending = governor vote.
    if (CouncilHandler::ConsumePopupFlag()) {
        sr_debug_log("GFX-HOOK: council popup pending, passing through to game");
        return sr_orig_gfx_event_loop(this_ptr, edx_unused, arg1, callback);
    }

    sr_debug_log("GFX-HOOK: no popup pending, this=%p arg1=%d inBuyVotes=%d",
        this_ptr, arg1, CouncilHandler::InBuyVotes());

    // Check if this is a governor vote (eligible candidates exist)
    // or a proposal buy-votes screen (no eligible candidates).
    bool has_eligible = false;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (is_alive(i) && !is_alien(i) && eligible(i)) {
            has_eligible = true;
            break;
        }
    }

    if (has_eligible) {
        // Governor election: show candidate selection modal
        return sr_council_vote_modal(voter);
    }

    // Proposal buy-votes screen: human already voted via COUNCILVOTE popup.
    // Return 0 to skip the graphical buy-votes screen and proceed to counting.
    sr_debug_log("COUNCIL-VOTE-HOOK: proposal buy-screen, skipping (voter=%d)", voter);
    return 0;
}

// ========== Council Proposal Vote Hook (0x427230) ==========
// Hooks the buy-votes/vote-interaction function which handles proposal votes.
// This function uses a different blocking mechanism than 0x602600 (governor).
// For proposals with a human voter, we show an accessible YES/NO/ABSTAIN modal
// and skip the original buy-votes interaction entirely.

typedef int (__thiscall *FCouncilBuyVotes)(void* this_ptr, int arg1, int arg2);
static FCouncilBuyVotes sr_orig_council_buy_votes = NULL;

// Proposal vote modal: YES / NO / ABSTAIN
static int sr_proposal_vote_modal(int voter, int arg1, int arg2) {
    // Try to determine the proposal name from the Proposal array.
    // arg1 or arg2 might be the proposal index — log both for now.
    sr_debug_log("PROPOSAL-MODAL: voter=%d arg1=%d arg2=%d", voter, arg1, arg2);

    // Build vote entries: Yes, No, Abstain
    struct VoteEntry {
        int value;  // 1=yes, -1=no, 0=abstain
        const char* label;
    };
    VoteEntry entries[3] = {
        { 1, loc(SR_COUNCIL_PROPOSAL_YES) },
        { -1, loc(SR_COUNCIL_PROPOSAL_NO) },
        { 0, loc(SR_COUNCIL_ABSTAIN) },
    };
    int count = 3;
    int index = 0;
    bool want_close = false;
    int result = 0; // default: abstain

    // Announce proposal vote prompt with proposal name
    char buf[512];
    const char* proposal_name = "";
    if (arg2 >= 0 && arg2 < MaxProposalNum && Proposal && Proposal[arg2].name) {
        sr_debug_log("PROPOSAL-MODAL: raw name='%s'", Proposal[arg2].name);
        proposal_name = sr_game_str(Proposal[arg2].name);
    }
    sr_debug_log("PROPOSAL-MODAL: proposal_name='%s' prompt='%s'",
        proposal_name, loc(SR_COUNCIL_PROPOSAL_PROMPT));
    snprintf(buf, sizeof(buf), loc(SR_COUNCIL_PROPOSAL_PROMPT), proposal_name);
    sr_debug_log("PROPOSAL-MODAL: announcing '%s'", buf);
    sr_output(buf, true);

    sr_debug_log("PROPOSAL-MODAL: starting modal (voter=%d)", voter);

    // Modal message loop
    MSG msg;
    while (!want_close) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
            if (msg.message == WM_KEYDOWN) {
                WPARAM key = msg.wParam;
                if (sr_modal_handle_utility_key(key)) continue;
                bool announce = false;

                switch (key) {
                case VK_UP:
                    index = (index + count - 1) % count;
                    announce = true;
                    break;
                case VK_DOWN:
                    index = (index + 1) % count;
                    announce = true;
                    break;
                case VK_RETURN:
                    result = entries[index].value;
                    want_close = true;
                    break;
                case VK_ESCAPE:
                    result = 0; // abstain
                    want_close = true;
                    break;
                case 'S':
                case VK_TAB:
                    if (!(GetKeyState(VK_CONTROL) & 0x8000)) {
                        CouncilHandler::AnnounceVoteSummary();
                    }
                    break;
                }

                if (announce && !want_close) {
                    snprintf(buf, sizeof(buf), "%d / %d: %s",
                        index + 1, count, entries[index].label);
                    sr_output(buf, true);
                }
                continue;
            }
            if (msg.message == WM_KEYUP || msg.message == WM_CHAR
                || msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) {
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Sleep(10);
        }
    }

    // Drain leftover keyboard messages
    MSG drain;
    while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
    while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

    // Announce result
    const char* vote_text = (result > 0) ? loc(SR_COUNCIL_PROPOSAL_YES)
                          : (result < 0) ? loc(SR_COUNCIL_PROPOSAL_NO)
                          : loc(SR_COUNCIL_ABSTAIN);
    snprintf(buf, sizeof(buf), loc(SR_COUNCIL_PROPOSAL_VOTED), vote_text);
    sr_output(buf, true);

    sr_debug_log("PROPOSAL-MODAL: result=%d (%s)", result, vote_text);
    return result;
}

static int __fastcall sr_council_buy_votes_hook(void* this_ptr, void* edx_unused,
                                                 int arg1, int arg2) {
    if (!CouncilHandler::IsActive() || !sr_is_available() || sr_all_disabled()) {
        return sr_orig_council_buy_votes(this_ptr, arg1, arg2);
    }

    int voter = *CurrentPlayerFaction;
    if (!is_human(voter)) {
        sr_debug_log("BUY-VOTES-HOOK: AI voter %d, calling original", voter);
        return sr_orig_council_buy_votes(this_ptr, arg1, arg2);
    }

    // Human player: skip the original buy-votes function entirely.
    // The original 0x427230 handles BOTH governor buy-votes AND proposal
    // votes in a single call, using a blocking mechanism we can't hook.
    // Governor vote was already cast via our 0x602600 hook (sr_council_vote_modal).
    // Now show proposal vote modal so the human can vote on the proposal too.

    bool has_eligible = false;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (is_alive(i) && !is_alien(i) && eligible(i)) {
            has_eligible = true;
            break;
        }
    }

    sr_debug_log("BUY-VOTES-HOOK: human voter=%d arg1=%d arg2=%d has_eligible=%d",
        voter, arg1, arg2, has_eligible);

    // Show proposal vote modal (YES/NO/ABSTAIN)
    int vote_result = sr_proposal_vote_modal(voter, arg1, arg2);

    // Record the human's proposal vote directly in the council vote array.
    // CouncilWin stores votes at this + 0xA54 + faction * 4.
    // arg1 appears to be the governor result (voted-for faction), arg2 the caller.
    // For proposals, voting YES = voting for the calling faction (arg2).
    int vote_value = 0;
    if (vote_result > 0) {
        vote_value = arg2; // YES = vote for the proposing faction
        sr_debug_log("BUY-VOTES-HOOK: recording YES, value=%d", vote_value);
    } else {
        vote_value = 0; // NO or ABSTAIN
        sr_debug_log("BUY-VOTES-HOOK: recording NO/ABSTAIN, value=0");
    }

    int* vote_array = (int*)((char*)this_ptr + 0xA54);
    vote_array[voter] = vote_value;
    sr_debug_log("BUY-VOTES-HOOK: wrote vote_array[%d] = %d", voter, vote_value);

    // === Phase 1: Buy votes diagnostic ===
    // Try calling buy_council_vote for each AI faction.
    // buy_council_vote uses 0x602600 for its BUYVOTEMENU, which we hook.
    // vote_for: -1=YEA, -2=NAY (for proposals, vote_type=1)
    int buy_vote_for = (vote_result > 0) ? -1 : -2; // Buy votes matching player's vote
    sr_debug_log("BUY-VOTES-HOOK: starting buy_council_vote phase, vote_for=%d", buy_vote_for);

    for (int i = 1; i < MaxPlayerNum; i++) {
        if (!is_alive(i) || is_alien(i)) continue;
        if (i == voter) continue; // Don't try to buy own vote

        sr_debug_log("BUY-VOTES-HOOK: calling buy_council_vote(%d, %d, 1, %d)",
            voter, i, buy_vote_for);
        CouncilHandler::SetBuyVotes(true);
        buy_council_vote(voter, i, 1, buy_vote_for);
        CouncilHandler::SetBuyVotes(false);
        sr_debug_log("BUY-VOTES-HOOK: buy_council_vote returned for faction %d", i);
    }

    sr_debug_log("BUY-VOTES-HOOK: buy_council_vote phase complete");

    // Fill in AI votes using council_get_vote and count results.
    // council_get_vote(faction, proposal_index, governor_result)
    // Returns: faction_id (vote for) or 0 (vote against/abstain)
    int yes_votes = 0;
    int no_votes = 0;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (!is_alive(i) || is_alien(i)) continue;
        int votes = council_votes(i);
        if (i == voter) {
            // Human vote already recorded
            if (vote_value > 0) {
                yes_votes += votes;
            } else {
                no_votes += votes;
            }
            continue;
        }
        // AI: ask the game how they would vote
        int ai_vote = council_get_vote(i, arg2, arg1);
        vote_array[i] = ai_vote;
        sr_debug_log("BUY-VOTES-HOOK: AI faction %d votes %d (%d votes)",
            i, ai_vote, votes);
        if (ai_vote > 0) {
            yes_votes += votes;
        } else {
            no_votes += votes;
        }
    }

    // Announce the result
    const char* proposal_name = "";
    if (arg2 >= 0 && arg2 < MaxProposalNum && Proposal && Proposal[arg2].name) {
        proposal_name = sr_game_str(Proposal[arg2].name);
    }

    char result_buf[512];
    sr_debug_log("BUY-VOTES-HOOK: proposal_name='%s'", proposal_name);
    if (yes_votes > no_votes) {
        snprintf(result_buf, sizeof(result_buf),
            loc(SR_COUNCIL_RESULT_PASSED), proposal_name, yes_votes, no_votes);
    } else {
        snprintf(result_buf, sizeof(result_buf),
            loc(SR_COUNCIL_RESULT_FAILED), proposal_name, yes_votes, no_votes);
    }
    sr_debug_log("BUY-VOTES-HOOK: saving result: '%s'", result_buf);
    CouncilHandler::SetProposalResult(result_buf);
    sr_debug_log("BUY-VOTES-HOOK: result yes=%d no=%d", yes_votes, no_votes);

    return 0;
}

// ========== Installation ==========

int sr_install_council_hooks(uint8_t*& tramp_slot) {
    int count = 0;
    void* tramp;

    // Hook call_council at 0x52C880 (Planetary Council)
    tramp = install_inline_hook(0x52C880, (void*)sr_hook_call_council, tramp_slot);
    if (tramp) {
        sr_orig_call_council = (fp_1int)tramp;
        tramp_slot += 32;
        count++;
        sr_hook_log("call_council hooked at 0x52C880");
    }

    // Hook the graphical event loop (0x602600) via inline hook.
    // This function is called from 120+ sites including all council vote screens.
    // We intercept it globally and only act when CouncilHandler::IsActive().
    tramp = install_inline_hook(0x602600, (void*)sr_council_vote_hook, tramp_slot);
    if (tramp) {
        sr_orig_gfx_event_loop = (FGfxEventLoop)tramp;
        tramp_slot += 32;
        count++;
        sr_hook_log("gfx event loop hooked at 0x602600 (council vote + global)");
    }

    // Hook the proposal buy-votes function (0x427230) via inline hook.
    // This function handles proposal vote interaction and uses a different
    // blocking mechanism than 0x602600. For human proposals, we show an
    // accessible YES/NO/ABSTAIN modal and skip the graphical buy-votes screen.
    tramp = install_inline_hook(0x427230, (void*)sr_council_buy_votes_hook, tramp_slot);
    if (tramp) {
        sr_orig_council_buy_votes = (FCouncilBuyVotes)tramp;
        tramp_slot += 32;
        count++;
        sr_hook_log("council buy-votes hooked at 0x427230 (proposal vote)");
    }

    return count;
}
