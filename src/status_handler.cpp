/*
 * StatusHandler — F4 Base Operations accessible list.
 *
 * Full modal replacement: intercepts F4 before the game, builds a
 * navigable base list from Bases[], runs own PeekMessage loop.
 * No game window involved — all data read directly from memory.
 */

#include "status_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "gui.h"
#include "base.h"
#include "map.h"

static bool _active = false;
static bool _wantClose = false;
static int _baseList[MaxBaseNum];
static int _baseCount = 0;
static int _currentIndex = 0;
static int _gotoBaseId = -1; // base to navigate to after modal closes

/// Build list of player's base IDs.
static void BuildBaseList() {
    _baseCount = 0;
    int owner = MapWin->cOwner;
    for (int i = 0; i < *BaseCount; i++) {
        if (Bases[i].faction_id == owner) {
            if (_baseCount < MaxBaseNum) {
                _baseList[_baseCount++] = i;
            }
        }
    }
}

/// Calculate turns until current production item completes.
static int TurnsToComplete(int base_id, BASE& b) {
    int cost = mineral_cost(base_id, b.item());
    int remaining = cost - b.minerals_accumulated;
    if (remaining <= 0) return 0;
    if (b.mineral_surplus <= 0) return -1; // stalled
    return (remaining + b.mineral_surplus - 1) / b.mineral_surplus;
}

/// Announce a single base at the given list index.
static void AnnounceBase(int index) {
    if (index < 0 || index >= _baseCount) return;
    int base_id = _baseList[index];
    BASE& b = Bases[base_id];

    const char* prod = sr_game_str(prod_name(b.queue_items[0]));
    int turns = TurnsToComplete(base_id, b);

    char buf[512];
    if (turns >= 0) {
        snprintf(buf, sizeof(buf), loc(SR_BASEOPS_BASE_FMT),
                 sr_game_str(b.name),
                 index + 1, _baseCount,
                 (int)b.pop_size,
                 b.nutrient_surplus,
                 b.mineral_surplus,
                 b.energy_surplus,
                 prod, turns);
    } else {
        snprintf(buf, sizeof(buf), loc(SR_BASEOPS_BASE_FMT_STALL),
                 sr_game_str(b.name),
                 index + 1, _baseCount,
                 (int)b.pop_size,
                 b.nutrient_surplus,
                 b.mineral_surplus,
                 b.energy_surplus,
                 prod);
    }
    sr_output(buf, true);
}

/// Announce detailed info for D key.
static void AnnounceDetail(int index) {
    if (index < 0 || index >= _baseCount) return;
    int base_id = _baseList[index];
    BASE& b = Bases[base_id];

    // Status string: golden age, drone riots, nerve staple
    char status[128] = "";
    if (b.state_flags & BSTATE_GOLDEN_AGE_ACTIVE) {
        snprintf(status, sizeof(status), "Golden Age.");
    } else if (b.state_flags & BSTATE_DRONE_RIOTS_ACTIVE) {
        snprintf(status, sizeof(status), "Drone Riots!");
    }
    if (b.nerve_staple_turns_left > 0) {
        char ns[64];
        snprintf(ns, sizeof(ns), " Nerve staple: %d turns.", (int)b.nerve_staple_turns_left);
        strncat(status, ns, sizeof(status) - strlen(status) - 1);
    }

    int elevation = elev_at(b.x, b.y);

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_BASEOPS_DETAIL),
             sr_game_str(b.name),
             b.talent_total,
             b.drone_total,
             b.specialist_total,
             b.economy_total,
             b.psych_total,
             b.labs_total,
             b.eco_damage,
             elevation,
             status);
    sr_output(buf, true);
}

/// Announce summary totals.
static void AnnounceSummary() {
    if (_baseCount == 0) {
        sr_output(loc(SR_BASEOPS_EMPTY), true);
        return;
    }

    int total_pop = 0;
    int total_energy = 0;
    for (int i = 0; i < _baseCount; i++) {
        BASE& b = Bases[_baseList[i]];
        total_pop += b.pop_size;
        total_energy += b.energy_surplus;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_BASEOPS_SUMMARY),
             _baseCount, total_pop, total_energy);
    sr_output(buf, true);
}

namespace StatusHandler {

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return true; // consume WM_CHAR etc.

    switch (wParam) {
    case VK_UP:
        if (_baseCount == 0) return true;
        if (_currentIndex > 0) {
            _currentIndex--;
        } else {
            _currentIndex = _baseCount - 1;
        }
        AnnounceBase(_currentIndex);
        return true;

    case VK_DOWN:
        if (_baseCount == 0) return true;
        if (_currentIndex < _baseCount - 1) {
            _currentIndex++;
        } else {
            _currentIndex = 0;
        }
        AnnounceBase(_currentIndex);
        return true;

    case VK_HOME:
        if (_baseCount == 0) return true;
        _currentIndex = 0;
        AnnounceBase(_currentIndex);
        return true;

    case VK_END:
        if (_baseCount == 0) return true;
        _currentIndex = _baseCount - 1;
        AnnounceBase(_currentIndex);
        return true;

    case VK_RETURN:
        if (_baseCount > 0 && _currentIndex >= 0) {
            _gotoBaseId = _baseList[_currentIndex];
            _wantClose = true;
        }
        return true;

    case VK_ESCAPE:
        _wantClose = true;
        return true;

    case VK_F1:
        if (ctrl_key_down()) {
            sr_output(loc(SR_BASEOPS_HELP), true);
        }
        return true;

    default:
        break;
    }

    // S key = summary
    if (wParam == 'S' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceSummary();
        return true;
    }

    // D key = detail info
    if (wParam == 'D' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceDetail(_currentIndex);
        return true;
    }

    return true; // consume all keys while modal
}

void RunModal() {
    BuildBaseList();
    _active = true;
    _wantClose = false;
    _currentIndex = 0;
    _gotoBaseId = -1;

    if (_baseCount == 0) {
        sr_output(loc(SR_BASEOPS_EMPTY), true);
        _active = false;
        return;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_BASEOPS_OPEN), _baseCount);
    sr_output(buf, true);

    // Announce first base (queued, non-interrupting)
    AnnounceBase(0);

    // Own message pump
    MSG msg;
    while (!_wantClose) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Sleep(10);
        }
    }
    // Drain leftover messages
    MSG drain;
    while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
    while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

    int goto_id = _gotoBaseId;
    _active = false;
    _baseCount = 0;
    _currentIndex = 0;
    _gotoBaseId = -1;

    if (!_wantClose) return; // WM_QUIT

    sr_output(loc(SR_BASEOPS_CLOSED), true);

    // Center map on selected base after modal is fully closed.
    // Use MapWin_focus (pure viewport scroll) instead of Console_focus
    // which triggers additional game logic and can open popups.
    if (goto_id >= 0 && goto_id < *BaseCount) {
        BASE& b = Bases[goto_id];
        MapWin_focus(MapWin, b.x, b.y);
        draw_map(1);
    }
}

} // namespace StatusHandler
