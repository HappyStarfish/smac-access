/*
 * ProjectsHandler — F5 Secret Projects accessible screen.
 *
 * Full modal replacement: intercepts F5 before the game, reads
 * SecretProjects[] array and Facility names, runs own PeekMessage loop.
 */

#include "projects_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "gui.h"
#include "faction.h"

static bool _active = false;
static bool _wantClose = false;

// Project list: facility IDs that are secret projects
static int _projectIds[SP_ID_Last - SP_ID_First + 1];
static int _projectCount = 0;
static int _currentIndex = 0;

/// Build list of valid secret project IDs.
static void BuildProjectList() {
    _projectCount = 0;
    for (int id = SP_ID_First; id <= SP_ID_Last; id++) {
        if (Facility[id].name != NULL) {
            _projectIds[_projectCount++] = id;
        }
    }
}

/// Announce a single project at the given list index.
static void AnnounceProject(int index) {
    if (index < 0 || index >= _projectCount) return;
    int fac_id = _projectIds[index];
    int sp_idx = fac_id - SP_ID_First;
    int base_id = SecretProjects[sp_idx];

    const char* name = sr_game_str(Facility[fac_id].name);
    char buf[512];

    if (base_id >= 0 && base_id < *BaseCount) {
        // Built at a base
        int owner = Bases[base_id].faction_id;
        snprintf(buf, sizeof(buf), loc(SR_PROJECTS_BUILT),
                 name, index + 1, _projectCount,
                 sr_game_str(MFactions[owner].noun_faction),
                 sr_game_str(Bases[base_id].name));
    } else if (base_id == SP_Destroyed) {
        snprintf(buf, sizeof(buf), loc(SR_PROJECTS_DESTROYED),
                 name, index + 1, _projectCount);
    } else {
        snprintf(buf, sizeof(buf), loc(SR_PROJECTS_UNBUILT),
                 name, index + 1, _projectCount);
    }
    sr_output(buf, true);
}

/// Announce summary: counts of built/unbuilt/destroyed.
static void AnnounceSummary() {
    int built = 0, unbuilt = 0, destroyed = 0;
    for (int i = 0; i < _projectCount; i++) {
        int sp_idx = _projectIds[i] - SP_ID_First;
        int base_id = SecretProjects[sp_idx];
        if (base_id >= 0) built++;
        else if (base_id == SP_Destroyed) destroyed++;
        else unbuilt++;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_PROJECTS_SUMMARY),
             _projectCount, built, unbuilt, destroyed);
    sr_output(buf, true);
}

namespace ProjectsHandler {

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return true;

    switch (wParam) {
    case VK_UP:
        if (_projectCount == 0) return true;
        if (_currentIndex > 0) {
            _currentIndex--;
        } else {
            _currentIndex = _projectCount - 1;
        }
        AnnounceProject(_currentIndex);
        return true;

    case VK_DOWN:
        if (_projectCount == 0) return true;
        if (_currentIndex < _projectCount - 1) {
            _currentIndex++;
        } else {
            _currentIndex = 0;
        }
        AnnounceProject(_currentIndex);
        return true;

    case VK_HOME:
        if (_projectCount == 0) return true;
        _currentIndex = 0;
        AnnounceProject(_currentIndex);
        return true;

    case VK_END:
        if (_projectCount == 0) return true;
        _currentIndex = _projectCount - 1;
        AnnounceProject(_currentIndex);
        return true;

    case VK_ESCAPE:
        _wantClose = true;
        return true;

    case VK_F1:
        if (ctrl_key_down()) {
            sr_output(loc(SR_PROJECTS_HELP), true);
        }
        return true;

    default:
        break;
    }

    if (wParam == 'S' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceSummary();
        return true;
    }

    return true;
}

void RunModal() {
    BuildProjectList();
    _active = true;
    _wantClose = false;
    _currentIndex = 0;

    if (_projectCount == 0) {
        sr_output(loc(SR_PROJECTS_EMPTY), true);
        _active = false;
        return;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_PROJECTS_OPEN), _projectCount);
    sr_output(buf, true);

    AnnounceProject(0);

    sr_run_modal_pump(&_wantClose);

    _active = false;
    _projectCount = 0;
    sr_output(loc(SR_PROJECTS_CLOSED), true);
}

} // namespace ProjectsHandler
