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

}
