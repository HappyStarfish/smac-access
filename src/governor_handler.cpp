/*
 * GovernorHandler - Accessible governor configuration for the base screen.
 * Modal loop pattern (same as SocialEngHandler/PrefsHandler).
 * Ctrl+G opens, Up/Down navigate, Space toggles, Enter saves, Escape cancels.
 */

#include "gui.h"
#include "base.h"
#include "base_handler.h"
#include "governor_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"

static bool _active = false;
static bool _wantClose = false;
static bool _confirmed = false;
static int _currentIndex = 0;
static uint32_t _workingFlags = 0;  // working copy of governor_flags

// 21 governor options matching BaseGovOptions[] order in engine_base.h
// Each has an English and German name.
struct GovOptionDef {
    uint32_t flag;      // GOV_* flag bit
    const char* name_en;
    const char* name_de;
};

static const GovOptionDef GovOptions[] = {
    { GOV_ACTIVE,               "Governor active",          "Gouverneur aktiv" },
    { GOV_MULTI_PRIORITIES,     "Multiple priorities",      "Mehrfache Prioritäten" },
    { GOV_NEW_VEH_FULLY_AUTO,   "New units fully automated","Neue Einheiten voll automatisiert" },
    { GOV_MANAGE_CITIZENS,      "Manage citizens",          "Bürger verwalten" },
    { GOV_MANAGE_PRODUCTION,    "Manage production",        "Produktion verwalten" },
    { GOV_MAY_PROD_EXPLORE_VEH, "May produce scouts",       "Darf Kundschafter bauen" },
    { GOV_MAY_PROD_LAND_COMBAT, "May produce land combat",  "Darf Landkampf bauen" },
    { GOV_MAY_PROD_NAVAL_COMBAT,"May produce naval combat", "Darf Seekampf bauen" },
    { GOV_MAY_PROD_AIR_COMBAT,  "May produce air combat",   "Darf Luftkampf bauen" },
    { GOV_MAY_PROD_NATIVE,      "May produce native units", "Darf einheimische Einheiten bauen" },
    { GOV_MAY_PROD_LAND_DEFENSE,"May produce land defense", "Darf Landverteidigung bauen" },
    { GOV_MAY_PROD_AIR_DEFENSE, "May produce air defense",  "Darf Luftverteidigung bauen" },
    { GOV_MAY_PROD_PROTOTYPE,   "May produce prototypes",   "Darf Prototypen bauen" },
    { GOV_MAY_PROD_TRANSPORT,   "May produce transports",   "Darf Transporter bauen" },
    { GOV_MAY_PROD_PROBES,      "May produce probe teams",  "Darf Sondierteams bauen" },
    { GOV_MAY_PROD_TERRAFORMERS,"May produce terraformers", "Darf Terraformer bauen" },
    { GOV_MAY_PROD_COLONY_POD,  "May produce colony pods",  "Darf Kolonieschoten bauen" },
    { GOV_MAY_PROD_FACILITIES,  "May produce facilities",   "Darf Einrichtungen bauen" },
    { GOV_MAY_FORCE_PSYCH,      "May force psych",          "Darf Psyche erzwingen" },
    { GOV_MAY_PROD_SP,          "May produce secret projects", "Darf Geheimprojekte bauen" },
    { GOV_MAY_HURRY_PRODUCTION, "May hurry production",     "Darf Produktion beschleunigen" },
};

static const int GOV_OPTION_COUNT = sizeof(GovOptions) / sizeof(GovOptions[0]);
// Total items: 1 priority selector + toggle options
static const int GOV_TOTAL_ITEMS = GOV_OPTION_COUNT + 1;
static const int GOV_PRIORITY_INDEX = 0; // first item

// Priority flags in cycle order
static const uint32_t PriorityFlags[] = {
    0, // None
    GOV_PRIORITY_EXPLORE,
    GOV_PRIORITY_DISCOVER,
    GOV_PRIORITY_BUILD,
    GOV_PRIORITY_CONQUER,
};
static const SrStr PriorityNames[] = {
    SR_GOV_PRIORITY_NONE,
    SR_GOV_PRIORITY_EXPLORE,
    SR_GOV_PRIORITY_DISCOVER,
    SR_GOV_PRIORITY_BUILD,
    SR_GOV_PRIORITY_CONQUER,
};
static const int PRIORITY_COUNT = 5;

// Detect German by checking if the "on" string matches German translation
static bool is_german() {
    const char* on = loc(SR_GOV_ON);
    return (on && strcmp(on, "an") == 0);
}

static const char* option_name(int idx) {
    if (idx < 0 || idx >= GOV_OPTION_COUNT) return "???";
    return is_german() ? GovOptions[idx].name_de : GovOptions[idx].name_en;
}

static int get_priority_index() {
    for (int i = 1; i < PRIORITY_COUNT; i++) {
        if (_workingFlags & PriorityFlags[i]) return i;
    }
    return 0; // None
}

static void set_priority(int pri_idx) {
    // Clear all priority bits
    _workingFlags &= ~(GOV_PRIORITY_EXPLORE|GOV_PRIORITY_DISCOVER|
                        GOV_PRIORITY_BUILD|GOV_PRIORITY_CONQUER);
    if (pri_idx > 0 && pri_idx < PRIORITY_COUNT) {
        _workingFlags |= PriorityFlags[pri_idx];
    }
}

static void announce_priority(bool interrupt = true) {
    int pri = get_priority_index();
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_GOV_PRIORITY_FMT), loc(PriorityNames[pri]));
    sr_output(buf, interrupt);
}

static void announce_option(int idx, bool interrupt = true) {
    if (idx == GOV_PRIORITY_INDEX) {
        announce_priority(interrupt);
        return;
    }
    // Toggle options start at index 1, map to GovOptions[0..20]
    int opt = idx - 1;
    if (opt < 0 || opt >= GOV_OPTION_COUNT) return;
    bool on = (_workingFlags & GovOptions[opt].flag) != 0;
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_GOV_OPTION_FMT),
        idx + 1, GOV_TOTAL_ITEMS, option_name(opt),
        on ? loc(SR_GOV_ON) : loc(SR_GOV_OFF));
    sr_output(buf, interrupt);
}

static void announce_summary() {
    int enabled = 0;
    for (int i = 0; i < GOV_OPTION_COUNT; i++) {
        if (_workingFlags & GovOptions[i].flag) enabled++;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_GOV_SUMMARY_FMT), enabled, GOV_OPTION_COUNT);
    // Append priority info
    int pri = get_priority_index();
    char pri_buf[128];
    snprintf(pri_buf, sizeof(pri_buf), " %s: %s.",
        is_german() ? "Priorit\xc3\xa4t" : "Priority", loc(PriorityNames[pri]));
    strncat(buf, pri_buf, sizeof(buf) - strlen(buf) - 1);
    sr_output(buf, true);
}

namespace GovernorHandler {

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return false;

    switch (wParam) {
    case VK_UP:
        _currentIndex = (_currentIndex + GOV_TOTAL_ITEMS - 1) % GOV_TOTAL_ITEMS;
        announce_option(_currentIndex);
        return true;

    case VK_DOWN:
        _currentIndex = (_currentIndex + 1) % GOV_TOTAL_ITEMS;
        announce_option(_currentIndex);
        return true;

    case VK_LEFT:
        if (_currentIndex == GOV_PRIORITY_INDEX) {
            int pri = get_priority_index();
            pri = (pri + PRIORITY_COUNT - 1) % PRIORITY_COUNT;
            set_priority(pri);
            announce_priority();
            return true;
        }
        return false;

    case VK_RIGHT:
        if (_currentIndex == GOV_PRIORITY_INDEX) {
            int pri = get_priority_index();
            pri = (pri + 1) % PRIORITY_COUNT;
            set_priority(pri);
            announce_priority();
            return true;
        }
        return false;

    case VK_SPACE:
        if (_currentIndex == GOV_PRIORITY_INDEX) {
            // Space cycles forward on priority
            int pri = get_priority_index();
            pri = (pri + 1) % PRIORITY_COUNT;
            set_priority(pri);
            announce_priority();
            return true;
        }
        _workingFlags ^= GovOptions[_currentIndex - 1].flag;
        announce_option(_currentIndex);
        return true;

    case 'S':
    case VK_TAB:
        announce_summary();
        return true;

    case 'I':
        if (ctrl_key_down()) {
            announce_option(_currentIndex);
            return true;
        }
        return false;

    case VK_F1:
        if (ctrl_key_down()) {
            sr_output(loc(SR_GOV_HELP), true);
            return true;
        }
        return false;

    case VK_RETURN:
        _confirmed = true;
        _wantClose = true;
        return true;

    case VK_ESCAPE:
        _confirmed = false;
        _wantClose = true;
        return true;

    default:
        return true; // consume all other keys
    }
}

void RunModal() {
    if (*CurrentBaseID < 0 || *CurrentBaseID >= *BaseCount) return;
    BASE* base = &Bases[*CurrentBaseID];
    if (base->faction_id != MapWin->cOwner) return;

    // Initialize working copy from base's current governor flags
    _workingFlags = base->governor_flags;
    _active = true;
    _wantClose = false;
    _confirmed = false;
    _currentIndex = 0;

    // Announce title
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_GOV_TITLE), GOV_TOTAL_ITEMS);
    sr_output(buf, true);
    announce_option(0, false);
    sr_debug_log("GovernorHandler::RunModal enter");

    sr_run_modal_pump(&_wantClose);

    if (_confirmed) {
        int base_id = *CurrentBaseID;
        // Apply all flags including priority
        base->governor_flags = _workingFlags;

        // Propagate to faction default
        Factions[base->faction_id].base_governor_adv = base->governor_flags;

        // Post-change logic (matches gui.cpp governor save)
        if (base->governor_flags & GOV_ACTIVE) {
            if (base->governor_flags & GOV_MANAGE_PRODUCTION) {
                base->state_flags &= ~BSTATE_UNK_80000000;
                base->queue_size = 0;
                mod_base_reset(base_id, 1);
            }
            if (base->governor_flags & GOV_MANAGE_CITIZENS) {
                base->worked_tiles = 0;
                base->specialist_total = 0;
                base->specialist_adjust = 0;
                base_compute(1);
            }
        }
        if (!(base->governor_flags & GOV_ACTIVE) || !(base->governor_flags & GOV_MANAGE_PRODUCTION)) {
            draw_radius(base->x, base->y, 2, 2);
        }

        GraphicWin_redraw(BaseWin);
        GraphicWin_redraw(MainWin);
        sr_output(loc(SR_GOV_SAVED), true);
        sr_debug_log("GovernorHandler::RunModal confirm, flags=0x%X", base->governor_flags);
    } else {
        sr_output(loc(SR_GOV_CANCELLED), true);
        sr_debug_log("GovernorHandler::RunModal cancel");
    }

    _active = false;
}

} // namespace GovernorHandler
