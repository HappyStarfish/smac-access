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

// ========== NETCONNECT Text Input (Create/Join Game) ==========
//
// Function-level hooks for AlphaNet_do_create (0x4E2A30) and
// AlphaNet_do_join (0x4E2E50).
//
// Hypothesis 5 approach: let the original functions run normally (preserving
// all state setup: Popup object, editbox registration, prefs, connect, lobby),
// but pre-fill the name buffers and hook X_pop to return 0 immediately so the
// popup dialog is skipped. The Popup constructor/destructor cycle runs fully
// (X_pop returning 0 means "used normally"), avoiding heap corruption.

// Global buffers used by AlphaNet_do_create / AlphaNet_do_join
static char* const NETCONNECT_BUF1 = (char*)0x9bb5e8; // game name (CREATE) or player name (JOIN)
static char* const NETCONNECT_BUF2 = (char*)0x9bb6e8; // player name (CREATE only)
static const int NETCONNECT_MAXLEN = 31;

// X_pop skip mechanism: when set, sr_xpop_stub returns 0 immediately
// and restores our pre-filled values to the global buffers (which the
// original editbox setup code may have overwritten with defaults).
static bool sr_xpop_skip = false;
static char sr_xpop_buf1[64] = {};
static char sr_xpop_buf2[64] = {};

/// Run a blocking text input modal. Shows prompt, accepts keyboard input,
/// echoes characters via screen reader. Returns true on Enter, false on Escape.
/// Text is written to outbuf (max maxlen chars + NUL).
static bool sr_text_input_modal(const char* prompt, char* outbuf, int maxlen,
                                const char* default_text) {
    int len = 0;

    // Initialize with default text
    if (default_text && default_text[0]) {
        strncpy(outbuf, default_text, maxlen);
        outbuf[maxlen] = '\0';
        len = (int)strlen(outbuf);
    } else {
        outbuf[0] = '\0';
    }

    // Announce prompt and current value
    char buf[256];
    if (len > 0) {
        snprintf(buf, sizeof(buf), "%s: %s", prompt, sr_game_str(outbuf));
    } else {
        snprintf(buf, sizeof(buf), "%s", prompt);
    }
    sr_output(buf, true);

    bool confirmed = false;
    bool done = false;

    // Own PeekMessage loop with TranslateMessage (needed for WM_CHAR)
    MSG msg;
    while (!done) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }

            if (msg.message == WM_KEYDOWN) {
                if (msg.wParam == VK_RETURN) {
                    confirmed = true;
                    done = true;
                    continue;
                }
                if (msg.wParam == VK_ESCAPE) {
                    confirmed = false;
                    done = true;
                    continue;
                }
                if (msg.wParam == VK_BACK) {
                    if (len > 0) {
                        len--;
                        outbuf[len] = '\0';
                        if (len > 0) {
                            sr_output(sr_game_str(outbuf), true);
                        } else {
                            sr_output(loc(SR_INPUT_TEXT_EMPTY), true);
                        }
                    }
                    continue;
                }
                if (msg.wParam == 'R' && ctrl_key_down()) {
                    if (len > 0) {
                        sr_output(sr_game_str(outbuf), true);
                    } else {
                        sr_output(loc(SR_INPUT_TEXT_EMPTY), true);
                    }
                    continue;
                }
                if (sr_modal_handle_utility_key(msg.wParam)) {
                    continue;
                }
                // Translate to generate WM_CHAR for printable keys
                TranslateMessage(&msg);
                continue;
            }

            if (msg.message == WM_CHAR) {
                unsigned char ch = (unsigned char)msg.wParam;
                if (ch >= 32 && ch != 127 && len < maxlen) {
                    outbuf[len++] = (char)ch;
                    outbuf[len] = '\0';
                    // Echo single character (ANSI → UTF-8 for screen reader)
                    char ansi[2] = { (char)ch, '\0' };
                    char utf8[8];
                    sr_ansi_to_utf8(ansi, utf8, sizeof(utf8));
                    sr_output(utf8, true);
                }
                continue;
            }

            // Non-keyboard messages: dispatch normally
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

    return confirmed;
}

/*
X_pop stub for NETCONNECT dialogs. When sr_xpop_skip is set, restores
pre-filled name values to the global buffers and returns 0 (= "OK pressed").
The original do_create/do_join editbox setup overwrites our buffers with
defaults, so we must restore them here (X_pop runs after editbox setup
but before validation).
*/
int __cdecl sr_xpop_stub(const char* filename, const char* label,
                          int maxlen, int flags, int extra) {
    if (sr_xpop_skip) {
        strncpy(NETCONNECT_BUF1, sr_xpop_buf1, NETCONNECT_MAXLEN);
        NETCONNECT_BUF1[NETCONNECT_MAXLEN] = '\0';
        strncpy(NETCONNECT_BUF2, sr_xpop_buf2, NETCONNECT_MAXLEN);
        NETCONNECT_BUF2[NETCONNECT_MAXLEN] = '\0';
        sr_debug_log("NETCONNECT: X_pop skipped for %s (buffers restored)", label);
        return 0;
    }
    typedef int (__cdecl *FXPop)(const char*, const char*, int, int, int);
    return ((FXPop)0x5BF3C0)(filename, label, maxlen, flags, extra);
}

/*
Hook for AlphaNet_do_create (0x4E2A30).
Hypothesis 5: do accessible text input first, then call the ORIGINAL
do_create with X_pop hooked to return 0 immediately. This preserves all
original state setup (Popup object, editbox registration, prefs save,
DirectPlay connect, lobby event loop).
Return 0 = success, non-zero = cancel.
*/
int __thiscall sr_hook_do_create(void* This) {
    if (!sr_is_available()) {
        typedef int (__thiscall *FDoCreate)(void*);
        return ((FDoCreate)0x4E2A30)(This);
    }

    char gamename[64] = {};
    char playername[64] = {};

    // Pre-populate from current buffer values
    strncpy(gamename, NETCONNECT_BUF1, NETCONNECT_MAXLEN);
    gamename[NETCONNECT_MAXLEN] = '\0';
    strncpy(playername, NETCONNECT_BUF2, NETCONNECT_MAXLEN);
    playername[NETCONNECT_MAXLEN] = '\0';

    sr_output(loc(SR_NETCONNECT_CREATE_TITLE), true);

    // Input loop: require both names non-empty (matches original validation)
    for (;;) {
        if (!sr_text_input_modal(loc(SR_NETCONNECT_GAME_NAME),
                gamename, NETCONNECT_MAXLEN, gamename)) {
            sr_output(loc(SR_NETCONNECT_CANCELLED), true);
            return 1;
        }
        if (!gamename[0]) {
            sr_output(loc(SR_NETCONNECT_NEED_GAME), true);
            continue;
        }

        if (!sr_text_input_modal(loc(SR_NETCONNECT_PLAYER_NAME),
                playername, NETCONNECT_MAXLEN, playername)) {
            sr_output(loc(SR_NETCONNECT_CANCELLED), true);
            return 1;
        }
        if (!playername[0]) {
            sr_output(loc(SR_NETCONNECT_NEED_PLAYER), true);
            continue;
        }
        break;
    }

    sr_debug_log("NETCONNECT: CREATE game=[%s] player=[%s]", gamename, playername);

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_NETCONNECT_CONFIRMED),
        sr_game_str(gamename), sr_game_str(playername));
    sr_output(buf, true);

    // Save values for X_pop stub to restore after editbox setup overwrites
    strncpy(sr_xpop_buf1, gamename, sizeof(sr_xpop_buf1) - 1);
    sr_xpop_buf1[sizeof(sr_xpop_buf1) - 1] = '\0';
    strncpy(sr_xpop_buf2, playername, sizeof(sr_xpop_buf2) - 1);
    sr_xpop_buf2[sizeof(sr_xpop_buf2) - 1] = '\0';

    // Call original do_create with X_pop skip active.
    // Original handles: Popup lifecycle, editbox setup, prefs save,
    // do_create_dialog_and_connect, add_player + lobby event loop.
    sr_xpop_skip = true;
    typedef int (__thiscall *FDoCreate)(void*);
    int result = ((FDoCreate)0x4E2A30)(This);
    sr_xpop_skip = false;

    sr_debug_log("NETCONNECT: original do_create returned: %d", result);
    return result;
}

/*
Hook for AlphaNet_do_join (0x4E2E50).
Same hypothesis 5 approach as do_create: accessible text input first,
then call original do_join with X_pop returning 0 immediately.
Return 0 = success, non-zero = cancel.
*/
int __thiscall sr_hook_do_join(void* This) {
    if (!sr_is_available()) {
        typedef int (__thiscall *FDoJoin)(void*);
        return ((FDoJoin)0x4E2E50)(This);
    }

    char playername[64] = {};
    strncpy(playername, NETCONNECT_BUF1, NETCONNECT_MAXLEN);
    playername[NETCONNECT_MAXLEN] = '\0';

    sr_output(loc(SR_NETCONNECT_JOIN_TITLE), true);

    if (!sr_text_input_modal(loc(SR_NETCONNECT_PLAYER_NAME),
            playername, NETCONNECT_MAXLEN, playername)) {
        sr_output(loc(SR_NETCONNECT_CANCELLED), true);
        return 1;
    }

    sr_debug_log("NETCONNECT: JOIN player=[%s]", playername);

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_NETCONNECT_JOIN_CONFIRMED),
        sr_game_str(playername));
    sr_output(buf, true);

    // Save value for X_pop stub to restore after editbox setup
    strncpy(sr_xpop_buf1, playername, sizeof(sr_xpop_buf1) - 1);
    sr_xpop_buf1[sizeof(sr_xpop_buf1) - 1] = '\0';
    sr_xpop_buf2[0] = '\0'; // not used for join

    // Call original do_join with X_pop skip active.
    // Original handles: Popup lifecycle, editbox setup, prefs save,
    // player info init, do_join_dialog_and_connect.
    sr_xpop_skip = true;
    typedef int (__thiscall *FDoJoin)(void*);
    int result = ((FDoJoin)0x4E2E50)(This);
    sr_xpop_skip = false;

    sr_debug_log("NETCONNECT: original do_join returned: %d", result);
    return result;
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
