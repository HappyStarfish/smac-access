#pragma once

#include "main.h"

namespace CouncilHandler {

/// Returns true if a council session is currently active.
bool IsActive();

/// Handle key events during council. Returns true if key was consumed.
bool HandleKey(UINT msg, WPARAM wParam);

/// Called from inline hook when call_council starts.
void OnCouncilOpen(int faction_id);

/// Called from inline hook when call_council returns.
void OnCouncilClose();

/// Store proposal result text to announce during OnCouncilClose.
void SetProposalResult(const char* result);

/// Mark that a game popup (COUNCILISSUES, COUNCILRECENTPROP, etc.) was just opened.
/// The GFX hook checks this to pass through popups instead of intercepting them.
void OnPopupOpened();

/// Check and clear the popup flag. Returns true if a popup was pending.
bool ConsumePopupFlag();

/// Returns true if the calling faction is human (player called the council).
bool IsCallerHuman();

/// Called when CALLSCOUNCIL popup detected — AI faction called the council.
/// Activates council tracking so hooks work for the human voter.
void OnAICouncilCalled();

/// Announce vote counts for all living factions.
void AnnounceVoteSummary();

/// Ctrl+V on world map: can we call the council? Who is governor?
void AnnouncePreCouncilInfo();

/// Called from mod_BasePop_start when GOVVOTE label detected.
/// Builds popup_list with Abstain + eligible faction candidates.
void OnGovVotePopup();

/// Called from gui.cpp timer trigger when council is active.
/// Scans captured text for vote result keywords and announces outcome.
void CheckAndAnnounceResults();

/// Called from gui.cpp timer when council active and items stable.
/// Detects vote screen, builds popup_list from faction columns.
void TryBuildVoteList();

/// Returns true if we are currently inside a buy_council_vote call.
/// Used by the 0x602600 hook to detect BUYVOTEMENU context.
bool InBuyVotes();

/// Set/clear the buy-votes flag. Called around buy_council_vote calls.
void SetBuyVotes(bool active);

/// Mark that a BUYVOTEMENU popup (actual vote-buying menu) is pending.
/// Set in mod_BasePop_start when label starts with "BUYVOTE".
void SetBuyMenuPending(bool v);

/// Check and clear the buy-menu flag. Returns true if a buy menu was pending.
bool ConsumeBuyMenuFlag();

}
