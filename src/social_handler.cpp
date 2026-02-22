/*
 * Social Engineering screen accessibility handler.
 * Replaces the game's SE dialog with a keyboard-navigable, screen-reader
 * accessible modal loop. Reads SE data directly from game memory and
 * announces current settings and effects via screen reader.
 */

#include "social_handler.h"
#include "engine.h"
#include "gui.h"
#include "modal_utils.h"
#include "screen_reader.h"
#include "localization.h"

namespace SocialEngHandler {

static bool _active = false;
static bool _wantClose = false;
static bool _confirmed = false;
static int _currentCat = 0;   // 0-3: Politics, Economics, Values, Future
static int _currentModel = 0; // 0-3: model within current category

// Effect names matching CSocialEffect field order
static const char* effect_names[] = {
    "Economy", "Efficiency", "Support", "Talent", "Morale",
    "Police", "Growth", "Planet", "Probe", "Industry", "Research"
};

static int get_faction_id() {
    return *CurrentPlayerFaction;
}

// Get current SE model for a category (from pending if dialog is open)
static int get_current_model(int cat) {
    Faction* f = &Factions[get_faction_id()];
    return (&f->SE_Politics_pending)[cat];
}

// Build effects string for a given category and model
static void build_effects_string(int cat, int model, char* buf, int bufsize) {
    CSocialEffect* eff = &SocialField[cat].soc_effect[model];
    int* vals = &eff->economy;
    char tmp[512] = "";
    int pos = 0;
    bool any = false;

    for (int i = 0; i < MaxSocialEffectNum; i++) {
        if (vals[i] != 0) {
            if (any) {
                pos += snprintf(tmp + pos, sizeof(tmp) - pos, ", ");
            }
            pos += snprintf(tmp + pos, sizeof(tmp) - pos, "%s %+d",
                effect_names[i], vals[i]);
            any = true;
        }
    }

    if (!any) {
        snprintf(buf, bufsize, "%s", loc(SR_SOCENG_NO_EFFECT));
    } else {
        snprintf(buf, bufsize, loc(SR_SOCENG_EFFECTS), tmp);
    }
}

// Check why a model is unavailable: 0=available, 1=no tech, 2=opposition
static int check_unavailable_reason(int cat, int model) {
    int faction_id = get_faction_id();
    if (MFactions[faction_id].soc_opposition_category == cat
    && MFactions[faction_id].soc_opposition_model == model) {
        return 2; // faction opposition
    }
    int preq = SocialField[cat].soc_preq_tech[model];
    if (preq == TECH_None) {
        return 0; // no tech needed, available
    }
    if (preq == TECH_Disable) {
        return 1; // disabled
    }
    if (!has_tech(preq, faction_id)) {
        return 1; // missing tech
    }
    return 0;
}

// Announce current model in the current category
static void announce_model() {
    int cat = _currentCat;
    int model = _currentModel;
    char buf[512];
    char effects[256];

    const char* model_name = SocialField[cat].soc_name[model];
    if (!model_name) model_name = "???";

    // Count available models for "X of Y" display
    int avail_count = MaxSocialModelNum;

    // Format: "N of M: ModelName"
    snprintf(buf, sizeof(buf), loc(SR_SOCENG_MODEL_FMT),
        model + 1, avail_count, model_name);

    // Check availability
    int reason = check_unavailable_reason(cat, model);
    if (reason == 1) {
        // Missing tech
        int preq = SocialField[cat].soc_preq_tech[model];
        const char* tech_name_str = "???";
        if (preq >= 0 && Tech) {
            tech_name_str = Tech[preq].name;
            if (!tech_name_str) tech_name_str = "???";
        }
        char unavail[256];
        snprintf(unavail, sizeof(unavail), loc(SR_SOCENG_UNAVAILABLE_TECH), tech_name_str);
        strncat(buf, ". ", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, unavail, sizeof(buf) - strlen(buf) - 1);
    } else if (reason == 2) {
        strncat(buf, ". ", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, loc(SR_SOCENG_UNAVAILABLE_OPPOSITION), sizeof(buf) - strlen(buf) - 1);
    } else {
        // Available: show effects
        build_effects_string(cat, model, effects, sizeof(effects));
        strncat(buf, ". ", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, effects, sizeof(buf) - strlen(buf) - 1);
    }

    // Mark if this is the current/pending selection
    int current = get_current_model(cat);
    if (model == current) {
        strncat(buf, ". ", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, loc(SR_SOCENG_CURRENT), sizeof(buf) - strlen(buf) - 1);
    }

    sr_output(buf, true);
}

// Announce current category with its current model
static void announce_category() {
    int cat = _currentCat;
    int current = get_current_model(cat);
    const char* cat_name = SocialField[cat].field_name;
    const char* model_name = "???";

    if (current >= 0 && current < MaxSocialModelNum) {
        model_name = SocialField[cat].soc_name[current];
        if (!model_name) model_name = "???";
    }
    if (!cat_name) cat_name = "???";

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_SOCENG_CATEGORY_FMT),
        cat + 1, cat_name, model_name);
    sr_output(buf, true);
}

// Announce full SE summary (all 4 categories)
static void announce_summary() {
    int faction_id = get_faction_id();
    Faction* f = &Factions[faction_id];
    char buf[1024] = "";
    int pos = 0;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s. ", loc(SR_SOCENG_TITLE));

    for (int cat = 0; cat < MaxSocialCatNum; cat++) {
        int model = get_current_model(cat);
        const char* cat_name = SocialField[cat].field_name;
        const char* model_name = "???";
        if (model >= 0 && model < MaxSocialModelNum) {
            model_name = SocialField[cat].soc_name[model];
            if (!model_name) model_name = "???";
        }
        if (!cat_name) cat_name = "???";

        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_SOCENG_SUMMARY_FMT), cat_name, model_name);
        if (cat < MaxSocialCatNum - 1) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
        } else {
            pos += snprintf(buf + pos, sizeof(buf) - pos, ".");
        }
    }

    // Calculate upheaval cost
    CSocialCategory pending;
    pending.politics = (&f->SE_Politics_pending)[0];
    pending.economics = (&f->SE_Politics_pending)[1];
    pending.values = (&f->SE_Politics_pending)[2];
    pending.future = (&f->SE_Politics_pending)[3];
    int cost = social_upheaval(faction_id, &pending);
    if (cost > 0) {
        char cost_str[128];
        snprintf(cost_str, sizeof(cost_str), " %s",
            loc(SR_SOCENG_UPHEAVAL_COST));
        // Format the cost into the string
        char cost_buf[128];
        snprintf(cost_buf, sizeof(cost_buf), cost_str, cost);
        strncat(buf, cost_buf, sizeof(buf) - strlen(buf) - 1);
    }

    sr_output(buf, true);
}

// If current model is available, select it as pending for the current category
static void select_if_available() {
    if (check_unavailable_reason(_currentCat, _currentModel) == 0) {
        int faction_id = get_faction_id();
        Faction* f = &Factions[faction_id];
        (&f->SE_Politics_pending)[_currentCat] = _currentModel;
    }
}

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return false;

    bool ctrl = ctrl_key_down();

    switch (wParam) {
    case VK_UP:
        if (ctrl) return false; // let game handle
        _currentCat = (_currentCat + MaxSocialCatNum - 1) % MaxSocialCatNum;
        _currentModel = get_current_model(_currentCat);
        if (_currentModel < 0) _currentModel = 0;
        announce_category();
        return true;

    case VK_DOWN:
        if (ctrl) return false;
        _currentCat = (_currentCat + 1) % MaxSocialCatNum;
        _currentModel = get_current_model(_currentCat);
        if (_currentModel < 0) _currentModel = 0;
        announce_category();
        return true;

    case VK_LEFT:
        if (ctrl) return false;
        _currentModel = (_currentModel + MaxSocialModelNum - 1) % MaxSocialModelNum;
        // Immediately select if available
        select_if_available();
        announce_model();
        return true;

    case VK_RIGHT:
        if (ctrl) return false;
        _currentModel = (_currentModel + 1) % MaxSocialModelNum;
        // Immediately select if available
        select_if_available();
        announce_model();
        return true;

    case VK_RETURN:
        // Enter: confirm all changes and close
        _wantClose = true;
        _confirmed = true;
        return true;

    case 'I':
        // Ctrl+I: repeat current item
        if (ctrl) {
            announce_model();
            return true;
        }
        return false;

    case 'S':
        // S: summary of all categories
        if (!ctrl) {
            announce_summary();
            return true;
        }
        return false;

    case VK_TAB:
        // Tab: summary
        announce_summary();
        return true;

    case VK_F1:
        // Ctrl+F1: help
        if (ctrl) {
            sr_output(loc(SR_SOCENG_HELP), true);
            return true;
        }
        return false;

    case VK_ESCAPE:
        // Cancel and close
        _wantClose = true;
        _confirmed = false;
        return true;

    default:
        return false;
    }
}

// Run the SE handler as a modal loop, bypassing the game's SE dialog entirely.
// Called when 'E' is pressed on the world map with screen reader active.
void RunModal() {
    int faction_id = get_faction_id();
    Faction* f = &Factions[faction_id];

    // Initialize pending from current SE settings
    for (int i = 0; i < MaxSocialCatNum; i++) {
        (&f->SE_Politics_pending)[i] = (&f->SE_Politics)[i];
    }

    _active = true;
    _wantClose = false;
    _confirmed = false;
    _currentCat = 0;
    _currentModel = get_current_model(0);
    if (_currentModel < 0) _currentModel = 0;

    sr_debug_log("SocialEngHandler::RunModal enter\n");
    announce_summary();

    sr_run_modal_pump(&_wantClose);

    // Apply or discard changes
    if (_confirmed) {
        social_set(faction_id);
        sr_debug_log("SocialEngHandler::RunModal confirm\n");
    } else {
        // Restore pending to current (cancel)
        for (int i = 0; i < MaxSocialCatNum; i++) {
            (&f->SE_Politics_pending)[i] = (&f->SE_Politics)[i];
        }
        sr_debug_log("SocialEngHandler::RunModal cancel\n");
    }

    _active = false;
    sr_output(loc(SR_SOCENG_CLOSED), true);

    // Refresh map display
    draw_map(1);
}

} // namespace SocialEngHandler
