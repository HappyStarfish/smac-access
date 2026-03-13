/*
 * Miscellaneous game hooks: tech_achieved, planetfall, file browser, number input.
 * Extracted from screen_reader.cpp.
 */

#include "main.h"
#include "sr_misc_hooks.h"
#include "sr_internal.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "file_browser_handler.h"
#include "plan.h" // reset_state()

// ========== Tech Achieved Hook ==========
// Capture tech_id so popups can resolve $TECH0 correctly.
// Without this, $TECH0 may resolve to a stale base name from mod_random_events.

int sr_current_tech_achieved_id = -1;
static fp_4int sr_orig_tech_achieved = NULL;

static int __cdecl sr_hook_tech_achieved(int faction_id, int tech_id, int a3, int a4) {
    sr_current_tech_achieved_id = tech_id;
    int result = sr_orig_tech_achieved(faction_id, tech_id, a3, a4);
    sr_current_tech_achieved_id = -1;
    return result;
}

// ========== Planetfall Hook ==========

static fp_1int sr_orig_planetfall = NULL;

/*
Simple variable substitution for planetfall text.
Replaces $NAME3, $TITLE2, and $<M1:$FACTIONADJ0> with actual faction data.
*/
static void sr_substitute_vars(char* buf, int bufsize, int faction_id) {
    if (faction_id < 0 || faction_id >= MaxPlayerNum) return;
    MFaction* mf = &MFactions[faction_id];
    char tmp[4096];

    // Replace $TITLE2
    char* p = strstr(buf, "$TITLE2");
    if (p) {
        int prefix = (int)(p - buf);
        snprintf(tmp, sizeof(tmp), "%.*s%s%s", prefix, buf,
            mf->title_leader, p + 7);
        strncpy(buf, tmp, bufsize - 1);
        buf[bufsize - 1] = '\0';
    }

    // Replace $NAME3
    p = strstr(buf, "$NAME3");
    if (p) {
        int prefix = (int)(p - buf);
        snprintf(tmp, sizeof(tmp), "%.*s%s%s", prefix, buf,
            mf->name_leader, p + 6);
        strncpy(buf, tmp, bufsize - 1);
        buf[bufsize - 1] = '\0';
    }

    // Replace $<M1:$FACTIONADJ0> with adj_name_faction
    p = strstr(buf, "$<M1:$FACTIONADJ0>");
    if (p) {
        int prefix = (int)(p - buf);
        snprintf(tmp, sizeof(tmp), "%.*s%s%s", prefix, buf,
            mf->adj_name_faction, p + 18);
        strncpy(buf, tmp, bufsize - 1);
        buf[bufsize - 1] = '\0';
    }

    // Replace $<F1:$FACTIONADJ0> (feminine form)
    p = strstr(buf, "$<F1:$FACTIONADJ0>");
    if (p) {
        int prefix = (int)(p - buf);
        snprintf(tmp, sizeof(tmp), "%.*s%s%s", prefix, buf,
            mf->adj_name_faction, p + 18);
        strncpy(buf, tmp, bufsize - 1);
        buf[bufsize - 1] = '\0';
    }

    // Replace naked $FACTIONADJ0 (used in German script text)
    p = strstr(buf, "$FACTIONADJ0");
    if (p) {
        int prefix = (int)(p - buf);
        snprintf(tmp, sizeof(tmp), "%.*s%s%s", prefix, buf,
            mf->adj_name_faction, p + 12);
        strncpy(buf, tmp, bufsize - 1);
        buf[bufsize - 1] = '\0';
    }
}

/*
Wrapper for planetfall: reads intro text from script file, resolves
variable placeholders ($NAME3, $TITLE2, etc.), and announces the full text.
Cha Dawn (FUNGBOY) uses #PLANETFALLF, all other factions use #PLANETFALL.
*/
static int __cdecl sr_hook_planetfall(int faction_id) {
    if (sr_is_available() && faction_id >= 0 && faction_id < MaxPlayerNum) {
        char buf[4096];
        bool announced = false;
        MFaction* mf = &MFactions[faction_id];

        // Cha Dawn (FUNGBOY) uses #PLANETFALLF (born on Planet, not from Unity)
        // Skip game's parse_string (substitute=false) — it gets $FACTIONADJ0
        // wrong for custom factions. We do our own substitution instead.
        if (_stricmp(mf->filename, "FUNGBOY") == 0) {
            announced = sr_read_popup_text(ScriptFile, "PLANETFALLF",
                buf, sizeof(buf), false);
        }
        if (!announced) {
            announced = sr_read_popup_text(ScriptFile, "PLANETFALL",
                buf, sizeof(buf), false);
        }
        if (announced) {
            sr_substitute_vars(buf, sizeof(buf), faction_id);
            sr_output(buf, false);
        }
    }
    sr_popup_set_active(true);
    int result = sr_orig_planetfall(faction_id);
    sr_popup_set_active(false);
    sr_clear_text();
    return result;
}

// ========== File Browser (Load/Save Dialog) ==========

static fp_2int sr_orig_load_game = NULL;
static fp_1int sr_orig_save_game = NULL;

bool sr_file_browser_active() {
    return FileBrowserHandler::IsActive();
}

// Unused — kept for API compatibility, handler does its own announcements
void sr_fb_on_text_captured(const char* text) {
    (void)text;
}

static int __cdecl sr_hook_load_game(int a, int b) {
    if (sr_is_available()) {
        const char* path = FileBrowserHandler::RunModal(false);
        if (path && path[0]) {
            sr_debug_log("FB-LOAD: loading %s", path);
            reset_state();
            return load_daemon(path, 0);
        }
        // Cancelled — return non-zero to tell the TOPMENU caller to loop
        // back to the menu. The caller's dispatch: non-zero → jne to menu
        // loop; zero → check 0x9B2074 flag and either init game or exit.
        // Returning 0 with flag=0 causes the caller to EXIT the menu
        // function entirely, which crashes (no valid game state).
        sr_debug_log("FB-LOAD: cancelled, returning to menu");
        return -1;
    }
    // No screen reader — use original game file browser
    return sr_orig_load_game(a, b);
}

static int __cdecl sr_hook_save_game(int a) {
    if (sr_is_available()) {
        const char* path = FileBrowserHandler::RunModal(true);
        if (path && path[0]) {
            sr_debug_log("FB-SAVE: saving to %s", path);
            save_daemon(path);
            return 1;
        }
        sr_debug_log("FB-SAVE: cancelled");
        return 0;
    }
    return sr_orig_save_game(a);
}

// ========== Number Input Dialog Hook ==========

static Fpop_ask_number sr_orig_pop_ask_number = NULL;

// Number input modal state
static bool sr_number_input_active = false;
static bool sr_number_input_close = false;
static bool sr_number_input_confirmed = false;
static char sr_number_buf[12] = {};  // up to 11 digits + NUL
static int sr_number_len = 0;
static int sr_number_default = 0;

bool sr_is_number_input_active() { return sr_number_input_active; }

// Process keys during modal number input (called from ModWinProc)
void sr_number_input_update(UINT msg, WPARAM wParam) {
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            sr_number_input_confirmed = true;
            sr_number_input_close = true;
            if (sr_number_len > 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), loc(SR_INPUT_NUMBER_DONE), atoi(sr_number_buf));
                sr_output(buf, true);
            }
            return;
        }
        if (wParam == VK_ESCAPE) {
            sr_number_input_confirmed = false;
            sr_number_input_close = true;
            return;
        }
        if (wParam == VK_BACK) {
            if (sr_number_len > 0) {
                sr_number_len--;
                sr_number_buf[sr_number_len] = '\0';
                if (sr_number_len > 0) {
                    sr_output(sr_number_buf, true);
                } else {
                    sr_output(loc(SR_INPUT_NUMBER_EMPTY), true);
                }
            }
            return;
        }
        if (wParam == 'R' && ctrl_key_down()) {
            if (sr_number_len > 0) {
                sr_output(sr_number_buf, true);
            } else {
                sr_output(loc(SR_INPUT_NUMBER_EMPTY), true);
            }
            return;
        }
    }
    if (msg == WM_CHAR) {
        char ch = (char)wParam;
        if (ch >= '0' && ch <= '9' && sr_number_len < 11) {
            sr_number_buf[sr_number_len++] = ch;
            sr_number_buf[sr_number_len] = '\0';
            char digit[4];
            snprintf(digit, sizeof(digit), "%c", ch);
            sr_output(digit, true);
        }
    }
}

/*
Hook for pop_ask_number: runs our own modal loop with digit echo
instead of the game's native dialog. Returns 0 for OK (value in
ParseNumTable[0]) or non-zero for cancel.
*/
static int __cdecl sr_hook_pop_ask_number(const char* filename, const char* label,
                                           int value, int a4) {
    if (!sr_is_available()) {
        return sr_orig_pop_ask_number(filename, label, value, a4);
    }

    // Announce the number input prompt
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_INPUT_NUMBER), value);
    sr_output(buf, true);

    // Initialize modal state
    sr_number_default = value;
    sr_number_len = 0;
    sr_number_buf[0] = '\0';
    sr_number_input_confirmed = false;
    sr_number_input_close = false;
    sr_number_input_active = true;

    // Run modal pump (keys dispatched through ModWinProc -> sr_number_input_update)
    sr_run_modal_pump(&sr_number_input_close);

    sr_number_input_active = false;

    if (sr_number_input_confirmed && sr_number_len > 0) {
        ParseNumTable[0] = atoi(sr_number_buf);
        return 0; // OK
    }
    // Cancel or empty: return default via ParseNumTable, signal cancel
    ParseNumTable[0] = sr_number_default;
    return -1; // cancel
}

// ========== Installation ==========

int sr_install_misc_hooks(uint8_t*& tramp_slot) {
    int count = 0;
    void* tramp;

    // Hook tech_achieved at 0x5BB000 (tech discovery popups)
    tramp = install_inline_hook(0x5BB000, (void*)sr_hook_tech_achieved, tramp_slot);
    if (tramp) {
        sr_orig_tech_achieved = (fp_4int)tramp;
        tramp_slot += 32;
        count++;
        sr_hook_log("tech_achieved hooked at 0x5BB000");
    }

    // Hook planetfall at 0x589180 (intro screen on game start)
    tramp = install_inline_hook(0x589180, (void*)sr_hook_planetfall, tramp_slot);
    if (tramp) {
        sr_orig_planetfall = (fp_1int)tramp;
        planetfall = sr_orig_planetfall;
        tramp_slot += 32;
        count++;
        sr_hook_log("planetfall hooked at 0x589180");
    }

    // Hook load_game at 0x5AAAB0 (file browser for loading saves)
    tramp = install_inline_hook(0x5AAAB0, (void*)sr_hook_load_game, tramp_slot);
    if (tramp) {
        sr_orig_load_game = (fp_2int)tramp;
        tramp_slot += 32;
        count++;
        sr_hook_log("load_game hooked at 0x5AAAB0");
    }

    // Hook save_game at 0x5A9EB0 (file browser for saving)
    tramp = install_inline_hook(0x5A9EB0, (void*)sr_hook_save_game, tramp_slot);
    if (tramp) {
        sr_orig_save_game = (fp_1int)tramp;
        tramp_slot += 32;
        count++;
        sr_hook_log("save_game hooked at 0x5A9EB0");
    }

    // Hook pop_ask_number via function pointer redirect (no inline hook needed)
    sr_orig_pop_ask_number = pop_ask_number;
    pop_ask_number = sr_hook_pop_ask_number;
    count++;
    sr_hook_log("pop_ask_number redirected to sr_hook_pop_ask_number");

    return count;
}
