/*
 * Screen reader integration for Thinker Accessibility Mod.
 * Loads Tolk.dll dynamically and provides text capture hooks.
 */

#include "main.h"
#include "screen_reader.h"
#include "game_log.h"
#include "localization.h"
#include "modal_utils.h"
#include "council_handler.h"
#include <stdarg.h>
#include <stdlib.h>

// Tolk function pointer types
typedef void    (__cdecl *FTolk_Load)();
typedef void    (__cdecl *FTolk_Unload)();
typedef bool    (__cdecl *FTolk_IsLoaded)();
typedef bool    (__cdecl *FTolk_Output)(const wchar_t* str, bool interrupt);
typedef bool    (__cdecl *FTolk_Speak)(const wchar_t* str, bool interrupt);
typedef bool    (__cdecl *FTolk_Silence)();
typedef const wchar_t* (__cdecl *FTolk_DetectScreenReader)();
typedef void    (__cdecl *FTolk_TrySAPI)(bool trySAPI);

// Tolk function pointers (loaded at runtime)
static HMODULE hTolk = NULL;
static FTolk_Load           pTolk_Load = NULL;
static FTolk_Unload         pTolk_Unload = NULL;
static FTolk_IsLoaded       pTolk_IsLoaded = NULL;
static FTolk_Output         pTolk_Output = NULL;
static FTolk_Speak          pTolk_Speak = NULL;
static FTolk_Silence        pTolk_Silence = NULL;
static FTolk_DetectScreenReader pTolk_DetectScreenReader = NULL;
static FTolk_TrySAPI        pTolk_TrySAPI = NULL;

// Text capture buffer
static const int SR_TEXT_BUF_SIZE = 8192;
static char sr_text_buf[SR_TEXT_BUF_SIZE];
static int sr_text_pos = 0;

// Individual item tracking (captured text as separate items)
static const int SR_MAX_ITEMS = 96;
static const int SR_MAX_ITEM_LEN = 256;
static char sr_items[SR_MAX_ITEMS][SR_MAX_ITEM_LEN];
static int sr_items_x[SR_MAX_ITEMS];   // X coordinate per item (-1 if unknown)
static int sr_items_y[SR_MAX_ITEMS];   // Y coordinate per item (-1 if unknown)
static void* sr_items_buf[SR_MAX_ITEMS]; // Buffer pointer per item (NULL if unknown)
static int sr_items_count = 0;

// Snapshot: completed draw cycle saved here when buffer is auto-cleared.
// Allows gui.cpp to announce screens that transition quickly.
static char sr_snap_items[SR_MAX_ITEMS][SR_MAX_ITEM_LEN];
static int sr_snap_items_x[SR_MAX_ITEMS];
static int sr_snap_items_y[SR_MAX_ITEMS];
static void* sr_snap_items_buf[SR_MAX_ITEMS];
static int sr_snap_count = 0;
static bool sr_snap_ready = false;

// Timestamp of last recorded text (for auto-clear between draw cycles)
static DWORD sr_last_record_time = 0;

// Trampoline storage for hooked functions (executable memory)
static uint8_t* trampoline_mem = NULL;

// Original function trampolines (callable pointers to original code)
static FBuffer_write_l          orig_write_l = NULL;
static FBuffer_write_l2         orig_write_l2 = NULL;
static FBuffer_write_cent_l     orig_write_cent_l = NULL;
static FBuffer_write_cent_l2    orig_write_cent_l2 = NULL;
static FBuffer_write_cent_l3    orig_write_cent_l3 = NULL;
static FBuffer_write_cent_l4    orig_write_cent_l4 = NULL;
static FBuffer_write_right_l    orig_write_right_l = NULL;
static FBuffer_write_right_l2   orig_write_right_l2 = NULL;
static FBuffer_write_right_l3   orig_write_right_l3 = NULL;
static FBuffer_wrap             orig_wrap = NULL;
static FBuffer_wrap2            orig_wrap2 = NULL;
static FBuffer_wrap_cent        orig_wrap_cent = NULL;
static FBuffer_wrap_cent3       orig_wrap_cent3 = NULL;
static FBuffer_write_strings    orig_write_strings = NULL;

// Trace mode: when triggered, log ALL hook calls for a few seconds
static DWORD sr_trace_until = 0; // GetTickCount() deadline for trace logging

// Popup suppress: while a popup function is executing, suppress sr_record_text
// so the popup's own UI rendering doesn't trigger the deferred announce timer
static bool sr_popup_active = false;
// Timestamp of last tutorial popup announcement (for suppressing worldmap trigger)
DWORD sr_tutorial_announce_time = 0;

// Defer announce: text capture continues but gui.cpp should not fire announce triggers.
// Used by blocking hooks (planetfall) that collect rendered text and announce after return.
static bool sr_defer_flag = false;

bool sr_defer_active() { return sr_defer_flag; }
bool sr_popup_is_active() { return sr_popup_active; }
void sr_popup_set_active(bool active) { sr_popup_active = active; }
void sr_defer_set(bool active) { sr_defer_flag = active; }

// Netsetup coordinate diagnostic: set by gui.cpp when in multiplayer setup
bool sr_netsetup_log_coords = false;

// Multiplayer no-dedup: skip strstr dedup so repeated names are captured
bool sr_mp_no_dedup = false;
bool sr_net_start_requested = false;

// Pending setup rules from accessible RULES modal
uint32_t sr_pending_setup_rules = 0;
bool sr_pending_setup_rules_set = false;

// Returns path to Logs/ subdirectory in game folder (trailing backslash).
// Creates the directory on first call. Returns empty string on failure.
const char* sr_get_log_dir() {
    static char log_dir[MAX_PATH] = {};
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        char exepath[MAX_PATH];
        if (GetModuleFileNameA(NULL, exepath, MAX_PATH) > 0) {
            char* last_sep = strrchr(exepath, '\\');
            if (!last_sep) last_sep = strrchr(exepath, '/');
            if (last_sep) *(last_sep + 1) = '\0';
            snprintf(log_dir, sizeof(log_dir), "%sLogs\\", exepath);
            CreateDirectoryA(log_dir, NULL);
        }
    }
    return log_dir;
}

// Format a line timestamp prefix like "[14:30:45] "
static void sr_timestamp(char* buf, int bufsize) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, bufsize, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
}

// Format a file timestamp suffix like "_2026-03-07_14-30-45"
static void sr_file_timestamp(char* buf, int bufsize) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, bufsize, "_%04d-%02d-%02d_%02d-%02d-%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

// Write a line with timestamp to a file in the Logs/ subdirectory
static void sr_write_log(const char* filename, const char* msg) {
    const char* dir = sr_get_log_dir();
    if (dir[0]) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s%s", dir, filename);
        FILE* f = fopen(path, "a");
        if (f) {
            char ts[32];
            sr_timestamp(ts, sizeof(ts));
            fprintf(f, "%s%s\n", ts, msg);
            fclose(f);
        }
    }
}

// Session log file: access_log_TIMESTAMP.log, created once per game launch
static char sr_session_logfile[64] = {};

static const char* sr_get_session_logfile() {
    if (!sr_session_logfile[0]) {
        char fts[32];
        sr_file_timestamp(fts, sizeof(fts));
        snprintf(sr_session_logfile, sizeof(sr_session_logfile),
                 "access_log%s.log", fts);
    }
    return sr_session_logfile;
}

static void sr_log(const char* msg) { sr_write_log(sr_get_session_logfile(), msg); }

// Debug state (defined here so sr_trace_call can reference them)
static bool sr_debug = false;

static void sr_trace_call(const char* func, const char* text) {
    if (GetTickCount() < sr_trace_until) {
        char logbuf[512];
        snprintf(logbuf, sizeof(logbuf), "TRACE %s: %.200s", func, text ? text : "(null)");
        sr_log(logbuf);
    }
}

// Persistent hook installation log (NOT cleared by debug toggle)
static void sr_hook_log(const char* msg) { sr_write_log("access_hooks.log", msg); }

/*
Load Tolk.dll and resolve all function pointers.
Returns true if Tolk loaded and a screen reader was detected.
*/
bool sr_init() {
    sr_log("sr_init: starting");
    hTolk = LoadLibraryA("Tolk.dll");
    if (!hTolk) {
        char buf[128];
        snprintf(buf, sizeof(buf), "sr_init: Failed to load Tolk.dll (error %lu)", GetLastError());
        sr_log(buf);
        debug("sr_init: Failed to load Tolk.dll (error %lu)\n", GetLastError());
        return false;
    }
    sr_log("sr_init: Tolk.dll loaded");

    pTolk_Load = (FTolk_Load)GetProcAddress(hTolk, "Tolk_Load");
    pTolk_Unload = (FTolk_Unload)GetProcAddress(hTolk, "Tolk_Unload");
    pTolk_IsLoaded = (FTolk_IsLoaded)GetProcAddress(hTolk, "Tolk_IsLoaded");
    pTolk_Output = (FTolk_Output)GetProcAddress(hTolk, "Tolk_Output");
    pTolk_Speak = (FTolk_Speak)GetProcAddress(hTolk, "Tolk_Speak");
    pTolk_Silence = (FTolk_Silence)GetProcAddress(hTolk, "Tolk_Silence");
    pTolk_DetectScreenReader = (FTolk_DetectScreenReader)GetProcAddress(hTolk, "Tolk_DetectScreenReader");
    pTolk_TrySAPI = (FTolk_TrySAPI)GetProcAddress(hTolk, "Tolk_TrySAPI");

    if (!pTolk_Load || !pTolk_Output || !pTolk_Unload) {
        debug("sr_init: Failed to resolve Tolk functions\n");
        FreeLibrary(hTolk);
        hTolk = NULL;
        return false;
    }

    // Enable SAPI as fallback if no screen reader is running
    if (pTolk_TrySAPI) {
        pTolk_TrySAPI(true);
    }

    pTolk_Load();

    if (pTolk_DetectScreenReader) {
        const wchar_t* sr = pTolk_DetectScreenReader();
        if (sr) {
            debug("sr_init: Screen reader detected: %ls\n", sr);
        } else {
            debug("sr_init: No screen reader detected (SAPI fallback)\n");
        }
    }

    sr_log("sr_init: Tolk initialized successfully");
    debug("sr_init: Tolk loaded successfully\n");
    return true;
}

void sr_shutdown() {
    if (pTolk_Unload) {
        pTolk_Unload();
    }
    if (hTolk) {
        FreeLibrary(hTolk);
        hTolk = NULL;
    }
    if (trampoline_mem) {
        VirtualFree(trampoline_mem, 0, MEM_RELEASE);
        trampoline_mem = NULL;
    }
    debug("sr_shutdown: Tolk unloaded\n");
}

static bool sr_disabled_flag = false;

// Set by no_hooks.txt at startup — permanent, cannot be toggled
static bool sr_permanent_disabled = false;

bool sr_all_disabled() {
    return sr_disabled_flag || sr_permanent_disabled;
}

void sr_set_disabled(bool disabled) {
    sr_disabled_flag = disabled;
}

bool sr_get_disabled() {
    return sr_disabled_flag;
}

bool sr_is_available() {
    if (sr_disabled_flag || sr_permanent_disabled) return false;
    return hTolk != NULL && pTolk_Output != NULL;
}

// Forward declarations for speech history (defined below)
static bool sr_hist_browsing = false;
void sr_history_add(const char* text);

/*
Convert ANSI string to wide string and output to screen reader.
*/
bool sr_output(const char* text, bool interrupt) {
    if (!text || !text[0] || !pTolk_Output) {
        return false;
    }
    if (!sr_hist_browsing) sr_history_add(text);
    int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (len <= 0) return false;

    wchar_t* wtext = (wchar_t*)alloca(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, len);
    return pTolk_Output(wtext, interrupt);
}

bool sr_output_w(const wchar_t* text, bool interrupt) {
    if (!text || !text[0] || !pTolk_Output) {
        return false;
    }
    return pTolk_Output(text, interrupt);
}

bool sr_silence() {
    if (pTolk_Silence) {
        return pTolk_Silence();
    }
    return false;
}

// === Speech history ===
// Circular buffer of recent announcements for Shift+F11/F12 browsing.

static const int SR_HIST_MAX = 100;
static const int SR_HIST_LEN = 512;
static char sr_hist_buf[100][512];
static int sr_hist_write = 0;   // next write position
static int sr_hist_count = 0;   // total entries stored

void sr_history_add(const char* text) {
    if (!text || !text[0]) return;
    // Skip duplicates of most recent entry
    if (sr_hist_count > 0) {
        int last = (sr_hist_write - 1 + SR_HIST_MAX) % SR_HIST_MAX;
        if (strcmp(sr_hist_buf[last], text) == 0) return;
    }
    strncpy(sr_hist_buf[sr_hist_write], text, SR_HIST_LEN - 1);
    sr_hist_buf[sr_hist_write][SR_HIST_LEN - 1] = '\0';
    sr_hist_write = (sr_hist_write + 1) % SR_HIST_MAX;
    if (sr_hist_count < SR_HIST_MAX) sr_hist_count++;
}

int sr_history_count() {
    return sr_hist_count;
}

const char* sr_history_get(int offset) {
    if (offset < 0 || offset >= sr_hist_count) return NULL;
    int idx = (sr_hist_write - 1 - offset + SR_HIST_MAX) % SR_HIST_MAX;
    return sr_hist_buf[idx];
}

void sr_history_set_browsing(bool active) {
    sr_hist_browsing = active;
}

/*
Convert Windows-1252 (ANSI) text to UTF-8.
Game text files and in-memory strings (base names, facility names, etc.)
use Windows-1252 encoding. Our sr_output() expects UTF-8.
*/
void sr_ansi_to_utf8(const char* src, char* dst, int dstsize) {
    if (!src || !dst || dstsize < 1) return;
    // First convert ANSI -> wide (UTF-16)
    int wlen = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
    if (wlen <= 0) {
        dst[0] = '\0';
        return;
    }
    wchar_t* wbuf = (wchar_t*)alloca(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, src, -1, wbuf, wlen);
    // Then convert wide -> UTF-8
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, NULL, 0, NULL, NULL);
    if (ulen <= 0) {
        dst[0] = '\0';
        return;
    }
    if (ulen > dstsize) {
        // UTF-8 result too large for buffer. Truncate the wide string
        // to fit, preserving valid UTF-8 (no broken multi-byte sequences).
        // Binary search for max wide chars that fit in dstsize.
        int lo = 0, hi = wlen - 1, best = 0;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            int needed = WideCharToMultiByte(CP_UTF8, 0, wbuf, mid, NULL, 0, NULL, NULL);
            if (needed > 0 && needed <= dstsize - 1) {
                best = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        if (best > 0) {
            WideCharToMultiByte(CP_UTF8, 0, wbuf, best, dst, dstsize, NULL, NULL);
            dst[dstsize - 1] = '\0';
        } else {
            dst[0] = '\0';
        }
        return;
    }
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, dst, dstsize, NULL, NULL);
}

/*
Convenience wrapper with rotating buffers for use in snprintf.
Supports up to 8 concurrent game strings per call.
*/
const char* sr_game_str(const char* ansi_text) {
    static char bufs[8][512];
    static int idx = 0;
    if (!ansi_text || !ansi_text[0]) return "";
    char* buf = bufs[idx];
    idx = (idx + 1) & 7;
    sr_ansi_to_utf8(ansi_text, buf, 512);
    // Strip $LINK<text=id> markers in-place, keeping just the display text.
    // These appear in facility effects, tech descriptions, etc.
    char* p = buf;
    while ((p = strstr(p, "$LINK<")) != NULL) {
        char* link_start = p;
        p += 6; // skip "$LINK<"
        char* eq = NULL;
        char* close = NULL;
        for (char* s = p; *s; s++) {
            if (*s == '=' && !eq) eq = s;
            if (*s == '>') { close = s; break; }
        }
        if (!close) break;
        int text_len = (int)((eq ? eq : close) - p);
        memmove(link_start + text_len, close + 1, strlen(close + 1) + 1);
        memmove(link_start, p, text_len);
    }
    return buf;
}

/*
Check if a string is known noise that should be filtered out.
*/
static bool sr_is_junk(const char* text) {
    if (strncmp(text, "Host ID:", 8) == 0) return true;
    if (strncmp(text, "Local ID:", 9) == 0) return true;
    // DirectPlay debug stats rendered on NetWin
    if (strncmp(text, "Guaranteed count:", 17) == 0) return true;
    if (strncmp(text, "Sending:", 8) == 0) return true;
    if (strncmp(text, "Average reply time:", 19) == 0) return true;
    if (strncmp(text, "Last time:", 10) == 0) return true;
    if (strncmp(text, "Flags:", 6) == 0) return true;
    // TURN COMPLETE floods the buffer during AI turns, preventing other announces
    if (strcmp(text, "TURN COMPLETE") == 0) return true;
    if (strcmp(text, "ZUG BEENDET") == 0) return true;
    if (strcmp(text, "RUNDE BEENDET") == 0) return true;
    // Diplomacy status strings that leak during menu navigation
    if (strncmp(text, "No Contact", 10) == 0) return true;
    if (strncmp(text, "no contact", 10) == 0) return true;
    if (strncmp(text, "Kein Kontakt", 12) == 0) return true;
    // Filter pure numbers (coordinates, counters)
    bool all_digits = true;
    for (const char* p = text; *p; p++) {
        if (*p != ' ' && *p != '-' && *p != '.' && (*p < '0' || *p > '9')) {
            all_digits = false;
            break;
        }
    }
    if (all_digits) return true;
    return false;
}

/*
Try to match rendered text against tutorial popup captions in tutor.txt.
Tutorial popups often appear nested inside other popups (e.g., the
research tutorial during tech selection). Since sr_popup_active blocks
normal text capture, this function provides a bypass: it reads the
body text directly from tutor.txt and announces it.
*/
static void sr_try_tutorial_announce(const char* text) {
    if (!text || strlen(text) < 5) return;
    if (!sr_is_available()) return;

    // Debounce: don't re-announce the same tutorial
    static char last_tutorial[256] = "";
    if (_stricmp(last_tutorial, text) == 0) return;

    // Open tutor.txt from game working directory
    FILE* f = fopen("tutor.txt", "r");
    if (!f) {
        sr_debug_log("TUTOR: cannot open tutor.txt");
        return;
    }

    // Scan for #caption matching the rendered text (case-insensitive)
    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
               || line[len-1] == ' '))
            line[--len] = '\0';
        if (strncmp(line, "#caption ", 9) == 0 && _stricmp(line + 9, text) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        fclose(f);
        return;
    }

    // Read body text lines after the caption (reuses sr_read_popup_text logic)
    char body[2048];
    int pos = 0;
    bool has_body = false;

    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
               || line[len-1] == ' '))
            line[--len] = '\0';

        // Next section starts
        if (len > 1 && line[0] == '#' && line[1] >= 'A' && line[1] <= 'Z') break;
        // Skip directives
        if (line[0] == '#') continue;
        // Skip comments
        if (line[0] == ';') continue;
        // Empty line = paragraph break
        if (len == 0) {
            if (has_body && pos > 0 && pos < (int)sizeof(body) - 2)
                body[pos++] = ' ';
            continue;
        }
        // Strip {curly} formatting markers
        char clean[512];
        int cpos = 0;
        for (int i = 0; i < len && cpos < (int)sizeof(clean) - 1; i++) {
            if (line[i] == '{' || line[i] == '}') continue;
            clean[cpos++] = line[i];
        }
        clean[cpos] = '\0';
        if (cpos == 0) continue;

        if (pos > 0 && pos < (int)sizeof(body) - 2) body[pos++] = ' ';
        int copy_len = cpos;
        if (pos + copy_len >= (int)sizeof(body) - 1)
            copy_len = (int)sizeof(body) - 1 - pos;
        if (copy_len > 0) {
            memcpy(body + pos, clean, copy_len);
            pos += copy_len;
            has_body = true;
        }
    }
    body[pos] = '\0';
    fclose(f);

    // Substitute game variables ($TECH0, $NUM0, etc.) before UTF-8 conversion
    sr_substitute_game_vars(body, sizeof(body));

    // Convert tutorial body from Windows-1252 to UTF-8
    {
        char utf8tmp[2048];
        sr_ansi_to_utf8(body, utf8tmp, sizeof(utf8tmp));
        strncpy(body, utf8tmp, sizeof(body) - 1);
        body[sizeof(body) - 1] = '\0';
    }

    if (has_body) {
        strncpy(last_tutorial, text, sizeof(last_tutorial) - 1);
        last_tutorial[sizeof(last_tutorial) - 1] = '\0';
        char announcement[2560];
        snprintf(announcement, sizeof(announcement), "%s. %s", text, body);
        sr_output(announcement, true);
        sr_tutorial_announce_time = GetTickCount();
        sr_debug_log("TUTORIAL-POPUP: %.200s", announcement);
    }
}

/*
Record text drawn by the game into the capture buffer.
Auto-clears when a new draw cycle starts (>100ms gap between calls).
Deduplicates against entire buffer within the same cycle.
*/
void sr_record_text(const char* text, int x, int y, void* buf) {
    if (!text || !text[0]) return;

    // Convert game text from Windows-1252 (ANSI) to UTF-8.
    // Game engine and language patches use Win-1252; our pipeline uses UTF-8.
    char utf8buf[1024];
    sr_ansi_to_utf8(text, utf8buf, sizeof(utf8buf));
    text = utf8buf;

    // During popup display, don't accumulate normal text.
    // But check for tutorial popup captions (nested inside other popups).
    if (sr_popup_active) {
        sr_try_tutorial_announce(text);
        return;
    }

    int len = strlen(text);
    // During file browser, allow 2-char strings through (e.g. "OK" button)
    int min_len = sr_file_browser_active() ? 2 : 3;
    if (len < min_len) {
        sr_debug_log("FILTERED short(%d): %s", len, text);
        return;
    }

    // Filter known junk
    if (sr_is_junk(text)) {
        sr_debug_log("FILTERED junk: %s", text);
        return;
    }

    // Auto-clear buffer when a new draw cycle starts (gap > 100ms)
    // Save previous items as snapshot so gui.cpp can announce fast transitions
    DWORD now = GetTickCount();
    if (sr_last_record_time > 0 && (now - sr_last_record_time) > 100) {
        if (sr_items_count > 0) {
            sr_snap_count = sr_items_count;
            for (int i = 0; i < sr_items_count; i++) {
                strncpy(sr_snap_items[i], sr_items[i], SR_MAX_ITEM_LEN - 1);
                sr_snap_items[i][SR_MAX_ITEM_LEN - 1] = '\0';
                sr_snap_items_x[i] = sr_items_x[i];
                sr_snap_items_y[i] = sr_items_y[i];
                sr_snap_items_buf[i] = sr_items_buf[i];
            }
            sr_snap_ready = true;
        }
        sr_text_pos = 0;
        sr_text_buf[0] = '\0';
        sr_items_count = 0;
    }
    sr_last_record_time = now;

    // Deduplicate: skip if this string is already anywhere in the buffer.
    // When sr_mp_no_dedup is set, allow duplicates (needed for MP player list
    // where "Computer" appears at multiple y-coordinates).
    if (!sr_mp_no_dedup && sr_text_pos > 0 && strstr(sr_text_buf, text) != NULL) {
        sr_debug_log("FILTERED dedup x=%d y=%d: %s", x, y, text);
        return;
    }

    // Append to buffer with newline separator
    int needed = len + 1; // +1 for newline
    if (sr_text_pos + needed < SR_TEXT_BUF_SIZE - 1) {
        memcpy(sr_text_buf + sr_text_pos, text, len);
        sr_text_pos += len;
        sr_text_buf[sr_text_pos++] = '\n';
        sr_text_buf[sr_text_pos] = '\0';
    }

    // Council popup description column filter: the COUNCILISSUES popup renders
    // proposal names (x~3) and descriptions (x>1000) side by side. Filter out
    // the description column so only proposal names appear as navigable items.
    if (CouncilHandler::IsActive() && x > 1000) {
        sr_debug_log("FILTERED council-desc x=%d y=%d: %s", x, y, text);
        return;
    }

    // Capture as individual item with prefix dedup:
    // If new text starts with the last item's text, replace it (same string growing).
    // Skip this when sr_mp_no_dedup is set (MP lobby needs all items at different y).
    if (!sr_mp_no_dedup && sr_items_count > 0) {
        const char* last = sr_items[sr_items_count - 1];
        int last_len = strlen(last);
        if (len >= last_len && strncmp(text, last, last_len) == 0) {
            strncpy(sr_items[sr_items_count - 1], text, SR_MAX_ITEM_LEN - 1);
            sr_items[sr_items_count - 1][SR_MAX_ITEM_LEN - 1] = '\0';
            if (x >= 0) sr_items_x[sr_items_count - 1] = x;
            if (y >= 0) sr_items_y[sr_items_count - 1] = y;
            if (buf) sr_items_buf[sr_items_count - 1] = buf;
            sr_debug_log("CAPTURE update[%d]: %s", sr_items_count - 1, text);
            return;
        }
    }
    if (sr_items_count < SR_MAX_ITEMS) {
        strncpy(sr_items[sr_items_count], text, SR_MAX_ITEM_LEN - 1);
        sr_items[sr_items_count][SR_MAX_ITEM_LEN - 1] = '\0';
        sr_items_x[sr_items_count] = x;
        sr_items_y[sr_items_count] = y;
        sr_items_buf[sr_items_count] = buf;
        sr_items_count++;
        sr_debug_log("CAPTURE new[%d] x=%d y=%d: %s", sr_items_count - 1, x, y, text);
    }
}

const char* sr_get_last_text() {
    return sr_text_buf;
}

void sr_clear_text() {
    sr_text_pos = 0;
    sr_text_buf[0] = '\0';
    sr_last_record_time = 0;
}

int sr_item_count() {
    return sr_items_count;
}

const char* sr_item_get(int index) {
    if (index >= 0 && index < sr_items_count) {
        return sr_items[index];
    }
    return NULL;
}

int sr_item_get_x(int index) {
    if (index >= 0 && index < sr_items_count) {
        return sr_items_x[index];
    }
    return -1;
}

int sr_item_get_y(int index) {
    if (index >= 0 && index < sr_items_count) {
        return sr_items_y[index];
    }
    return -1;
}

void* sr_item_get_buf(int index) {
    if (index >= 0 && index < sr_items_count) {
        return sr_items_buf[index];
    }
    return NULL;
}

void sr_items_clear() {
    sr_items_count = 0;
}

DWORD sr_get_last_record_time() {
    return sr_last_record_time;
}

bool sr_snapshot_ready() {
    return sr_snap_ready;
}

int sr_snapshot_count() {
    return sr_snap_count;
}

const char* sr_snapshot_get(int index) {
    if (index >= 0 && index < sr_snap_count) {
        return sr_snap_items[index];
    }
    return NULL;
}

int sr_snapshot_get_y(int index) {
    if (index >= 0 && index < sr_snap_count) {
        return sr_snap_items_y[index];
    }
    return -1;
}

void* sr_snapshot_get_buf(int index) {
    if (index >= 0 && index < sr_snap_count) {
        return sr_snap_items_buf[index];
    }
    return NULL;
}

void sr_snapshot_consume() {
    sr_snap_ready = false;
}

void sr_force_snapshot() {
    // Save current items to snapshot
    sr_snap_count = sr_items_count;
    for (int i = 0; i < sr_items_count; i++) {
        strncpy(sr_snap_items[i], sr_items[i], SR_MAX_ITEM_LEN - 1);
        sr_snap_items[i][SR_MAX_ITEM_LEN - 1] = '\0';
        sr_snap_items_x[i] = sr_items_x[i];
        sr_snap_items_y[i] = sr_items_y[i];
        sr_snap_items_buf[i] = sr_items_buf[i];
    }
    sr_snap_ready = true;
    // Clear current items and text buffer for fresh capture
    sr_text_pos = 0;
    sr_text_buf[0] = '\0';
    sr_items_count = 0;
    sr_last_record_time = 0;
}

// ========== Debug Logging ==========

bool sr_debug_active() {
    return sr_debug;
}

void sr_debug_toggle() {
    sr_debug = !sr_debug;
    if (sr_debug) {
        sr_log("=== SR DEBUG ENABLED ===");
        sr_output(loc(SR_DEBUG_ON), true);
    } else {
        sr_log("=== SR DEBUG DISABLED ===");
        sr_output(loc(SR_DEBUG_OFF), true);
    }
}

void sr_debug_log(const char* fmt, ...) {
    if (!sr_debug) return;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sr_log(buf);
}

// ========== Popup List Navigation ==========

SrPopupList sr_popup_list = {{}, {}, 0, 0, false};

void sr_popup_list_clear() {
    sr_popup_list.active = false;
    sr_popup_list.count = 0;
    sr_popup_list.index = 0;
    sr_popup_list.label[0] = '\0';
}

/*
Parse selectable options from a script file popup section.
Options are lines after the first blank line (or after #itemlist).
Skips directive lines (#), comment lines (;), and empty lines before options.
Strips ^ paragraph markers and {curly} braces from option text.
*/
int sr_popup_list_parse(const char* filename, const char* label) {
    sr_popup_list_clear();
    if (!filename || !label) return 0;

    char filepath[MAX_PATH];
    // Game sometimes passes filename with .txt extension already
    int flen = strlen(filename);
    if (flen > 4 && _stricmp(filename + flen - 4, ".txt") == 0) {
        snprintf(filepath, sizeof(filepath), "%s", filename);
    } else {
        snprintf(filepath, sizeof(filepath), "%s.txt", filename);
    }

    FILE* f = fopen(filepath, "r");
    if (!f) return 0;

    // Find the #LABEL line
    char line[512];
    char search[256];
    snprintf(search, sizeof(search), "#%s", label);
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        // Strip trailing whitespace
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
               || line[len-1] == ' ')) {
            line[--len] = '\0';
        }
        if (_stricmp(line, search) == 0) {
            found = true;
            break;
        }
    }
    if (!found) { fclose(f); return 0; }

    // Read lines: skip directives, find blank line separator, then options
    bool past_separator = false;
    bool has_itemlist = false;
    int count = 0;

    while (fgets(line, sizeof(line), f) && count < SR_POPUP_LIST_MAX) {
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
               || line[len-1] == ' ')) {
            line[--len] = '\0';
        }

        // Stop at next section label
        if (len > 1 && line[0] == '#' && isupper((unsigned char)line[1])) {
            break;
        }

        // Skip comment lines
        if (line[0] == ';') continue;

        // Check for #itemlist directive
        if (_strnicmp(line, "#itemlist", 9) == 0) {
            has_itemlist = true;
            past_separator = true;
            continue;
        }

        // Skip other directive lines
        if (line[0] == '#') continue;

        // Blank line = separator between description and options
        if (len == 0) {
            if (!has_itemlist) {
                past_separator = true;
            }
            continue;
        }

        // Only collect lines after the separator
        if (!past_separator) continue;

        // Strip ^ paragraph markers and {curly} braces
        char clean[256];
        int ci = 0;
        for (int i = 0; i < len && ci < 254; i++) {
            if (line[i] == '^') continue;
            if (line[i] == '{' || line[i] == '}') continue;
            clean[ci++] = line[i];
        }
        clean[ci] = '\0';

        // Skip empty results
        if (ci < 1) continue;

        // Skip sub-items (lines starting with __ are indented sub-options)
        // Keep them but strip the underscores
        char* text = clean;
        while (*text == '_') text++;

        // Substitute game variables ($TITLE3, $NAME4, $TECH0, $<N:...>, etc.)
        char subst[512];
        strncpy(subst, text, sizeof(subst) - 1);
        subst[sizeof(subst) - 1] = '\0';
        sr_substitute_game_vars(subst, sizeof(subst));
        text = subst;

        // Convert from Windows-1252 to UTF-8
        char utf8tmp[256];
        sr_ansi_to_utf8(text, utf8tmp, sizeof(utf8tmp));
        strncpy(sr_popup_list.items[count], utf8tmp, 255);
        sr_popup_list.items[count][255] = '\0';
        count++;
    }

    fclose(f);

    if (count > 1) {
        sr_popup_list.count = count;
        sr_popup_list.index = 0;
        sr_popup_list.active = true;
        strncpy(sr_popup_list.label, label, sizeof(sr_popup_list.label) - 1);
        sr_popup_list.label[sizeof(sr_popup_list.label) - 1] = '\0';
        sr_debug_log("POPUP-LIST parsed %d options from %s#%s",
            count, filename, label);
        for (int i = 0; i < count; i++) {
            sr_debug_log("  option[%d]: %s", i, sr_popup_list.items[i]);
        }
    }
    return count;
}

// ========== Game Variable Substitution ==========

/*
Substitute game variables in a text buffer.
Delegates to the game's native parse_string (0x625880) which handles ALL
variable types: $TITLE#, $NAME#, $BASENAME#, $VOKI#, $SHIMODA#, $NUM#,
$TECH#, $ABIL#, $TERRAFORM#, $<N:form0:form1:...> gender/plurality, etc.
Called from sr_read_popup_text and sr_popup_list_parse.
*/
void sr_substitute_game_vars(char* buf, int bufsize) {
    if (!buf || bufsize < 2) return;
    // parse_string does not take a size parameter; use a large temp buffer
    char tmp[8192];
    tmp[0] = '\0';
    parse_string(buf, tmp);
    strncpy(buf, tmp, bufsize - 1);
    buf[bufsize - 1] = '\0';

    // Strip $LINK<text=id> markers, keeping just the text part.
    // These are datalink hyperlinks the game renders as clickable UI elements.
    char* p = buf;
    while ((p = strstr(p, "$LINK<")) != NULL) {
        char* link_start = p;
        p += 6; // skip "$LINK<"
        // Find '=' or '>' to extract the display text
        char* eq = NULL;
        char* close = NULL;
        for (char* s = p; *s; s++) {
            if (*s == '=' && !eq) eq = s;
            if (*s == '>') { close = s; break; }
        }
        if (!close) break; // malformed, stop
        int text_len = (int)((eq ? eq : close) - p);
        // Replace "$LINK<text=id>" with just "text" in-place
        memmove(link_start + text_len, close + 1, strlen(close + 1) + 1);
        memmove(link_start, p, text_len);
    }
}

// ========== Popup Text Capture (popp/popb hooks) ==========

// Saved original function pointers
static Fpopp sr_orig_popp = NULL;
static Fpopb sr_orig_popb = NULL;

/*
Read popup body text from a game text file.
Opens "filename.txt", seeks to "#LABEL", reads body text lines.
Skips directive lines (#xs, #caption, #button, #wave, etc.)
and strips {curly} formatting markers.
Returns true if any body text was found.
*/
bool sr_read_popup_text(const char* filename, const char* label,
                        char* buf, int bufsize, bool substitute) {
    if (!filename || !label || !buf || bufsize < 2) return false;

    char filepath[MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%s.txt", filename);

    FILE* f = fopen(filepath, "r");
    if (!f) {
        // Try without .txt extension (filename might already include it)
        f = fopen(filename, "r");
        if (!f) {
            char logbuf[256];
            snprintf(logbuf, sizeof(logbuf), "POPUP file not found: %s (also tried: %s)",
                filepath, filename);
            sr_log(logbuf);
            return false;
        }
    }

    // Build search target: "#LABEL"
    char target[128];
    snprintf(target, sizeof(target), "#%s", label);
    int target_len = strlen(target);

    // Seek to the label
    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        // Strip trailing whitespace
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
               || line[len-1] == ' ')) {
            line[--len] = '\0';
        }
        // Check if this line matches "#LABEL" exactly
        if (line[0] == '#' && len == target_len
            && strncmp(line, target, target_len) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        fclose(f);
        char logbuf[256];
        snprintf(logbuf, sizeof(logbuf), "POPUP label not found: #%s in %s", label, filepath);
        sr_log(logbuf);
        return false;
    }

    // Read body text lines after the label
    buf[0] = '\0';
    int pos = 0;
    bool has_body = false;

    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
               || line[len-1] == ' ')) {
            line[--len] = '\0';
        }

        // Next label starts - stop reading
        if (len > 1 && line[0] == '#' && line[1] >= 'A' && line[1] <= 'Z') {
            break;
        }

        // Extract #caption text and prepend to body
        if (strncmp(line, "#caption ", 9) == 0) {
            const char* cap = line + 9;
            int clen = strlen(cap);
            if (clen > 0 && pos + clen + 3 < bufsize) {
                memcpy(buf + pos, cap, clen);
                pos += clen;
                buf[pos++] = '.';
                buf[pos++] = ' ';
                buf[pos] = '\0';
                has_body = true;
            }
            continue;
        }

        // Skip other directive lines (#xs, #button, #wave, etc.)
        if (line[0] == '#') {
            continue;
        }

        // Skip comment lines
        if (line[0] == ';') {
            continue;
        }

        // Empty line = paragraph break (only if we already have text)
        if (len == 0) {
            if (has_body) {
                // Add space as paragraph separator
                if (pos > 0 && pos < bufsize - 2) {
                    buf[pos++] = ' ';
                }
            }
            continue;
        }

        // Handle ^ paragraph marker: strip leading ^ but keep text after
        if (line[0] == '^') {
            if (pos > 0 && pos < bufsize - 2) {
                buf[pos++] = ' ';
            }
            // Strip leading ^ characters and whitespace
            char* rest = line;
            while (*rest == '^') rest++;
            while (*rest == ' ' || *rest == '\t') rest++;
            if (*rest == '\0') continue; // Only ^ markers, no text
            // Shift remaining text into line for brace stripping below
            int rest_len = strlen(rest);
            memmove(line, rest, rest_len + 1);
            len = rest_len;
            // Fall through to brace stripping
        }

        // Strip {curly} formatting markers from the line
        // Copy characters, skipping { and } but keeping content between them
        char clean[512];
        int cpos = 0;
        for (int i = 0; i < len && cpos < (int)sizeof(clean) - 1; i++) {
            if (line[i] == '{' || line[i] == '}') {
                continue;
            }
            clean[cpos++] = line[i];
        }
        clean[cpos] = '\0';

        if (cpos == 0) continue;

        // Append to buffer with space separator
        if (pos > 0 && pos < bufsize - 2) {
            buf[pos++] = ' ';
        }
        int copy_len = cpos;
        if (pos + copy_len >= bufsize - 1) {
            copy_len = bufsize - 1 - pos;
        }
        if (copy_len > 0) {
            memcpy(buf + pos, clean, copy_len);
            pos += copy_len;
            has_body = true;
        }
    }

    buf[pos] = '\0';
    fclose(f);

    // Substitute game variables (unless caller handles substitution)
    if (substitute) {
        sr_substitute_game_vars(buf, bufsize);
    }

    // Convert the entire buffer from Windows-1252 to UTF-8.
    // Game text files use Windows-1252; our output pipeline expects UTF-8.
    // Use large buffer to avoid truncation of long texts (interludes, etc.)
    {
        int tmpsize = bufsize + 1024; // UTF-8 can be ~1.5x larger than ANSI
        char* utf8tmp = (char*)alloca(tmpsize);
        sr_ansi_to_utf8(buf, utf8tmp, tmpsize);
        strncpy(buf, utf8tmp, bufsize - 1);
        buf[bufsize - 1] = '\0';
    }

    {
        char logbuf[512];
        snprintf(logbuf, sizeof(logbuf), "POPUP read [%s#%s]: %.200s", filename, label, buf);
        sr_log(logbuf);
    }
    return has_body;
}

// ========== Setup Rules Checkbox Popup Replacement ==========

// Flag mapping for the 16 rules in SCRIPT.txt#RULES order.
// This matches the order the game displays them in the setup popup.
static const uint32_t sr_setup_rules_flags[16] = {
    RULES_VICTORY_TRANSCENDENCE,  // 0: Höheres Ziel
    RULES_VICTORY_CONQUEST,       // 1: Totaler Krieg
    RULES_VICTORY_DIPLOMATIC,     // 2: Friedenszeiten
    RULES_VICTORY_ECONOMIC,       // 3: Alles meins
    RULES_VICTORY_COOPERATIVE,    // 4: Einer für alle
    RULES_DO_OR_DIE,              // 5: Leben oder Tod
    RULES_LOOK_FIRST,             // 6: Zuerst schauen
    RULES_TECH_STAGNATION,        // 7: Tech-Stagnation
    RULES_SPOILS_OF_WAR,          // 8: Kriegsbeute
    RULES_BLIND_RESEARCH,         // 9: Ziellose Forschung
    RULES_INTENSE_RIVALRY,        // 10: Starke Rivalität
    RULES_NO_UNITY_SURVEY,        // 11: Keine Unity-Vermessung
    RULES_NO_UNITY_SCATTERING,    // 12: Keine Unity-Verteilung
    RULES_BELL_CURVE,             // 13: Glockenkurve
    RULES_TIME_WARP,              // 14: Zeitsprung
    RULES_IRONMAN,                // 15: Iron Man
};

/// Convert RULES_* game flags to item-position bitmask (bit N = item N checked).
static uint32_t sr_rules_to_items(uint32_t rules_flags) {
    uint32_t items = 0;
    for (int i = 0; i < 16; i++) {
        if (rules_flags & sr_setup_rules_flags[i])
            items |= (1u << i);
    }
    return items;
}

/// Convert item-position bitmask back to RULES_* game flags.
static uint32_t sr_items_to_rules(uint32_t items) {
    uint32_t flags = 0;
    for (int i = 0; i < 16; i++) {
        if (items & (1u << i))
            flags |= sr_setup_rules_flags[i];
    }
    return flags;
}

/// Replace the game's mouse-only RULES checkbox popup with an accessible
/// keyboard-driven modal loop. Takes RULES_* flags as initial state.
/// Returns RULES_* flags on Enter, or -1 on Escape (cancel).
static int sr_handle_rules_checkbox(uint32_t initial_rules_flags) {
    // Copy popup list items locally, then deactivate the popup list
    // so gui.cpp's arrow key handler doesn't fire during our modal loop.
    char items[16][256];
    int count = sr_popup_list.count;
    if (count <= 0 || count > 16) {
        sr_debug_log("RULES-CB: bad count %d, falling through", count);
        return -1;
    }
    for (int i = 0; i < count; i++) {
        strncpy(items[i], sr_popup_list.items[i], 255);
        items[i][255] = '\0';
    }
    sr_popup_list_clear();

    // Convert RULES_* flags to item-position bitmask for internal tracking
    uint32_t bits = sr_rules_to_items(initial_rules_flags);
    int index = 0;
    bool want_close = false;
    bool accepted = false;

    sr_debug_log("RULES-CB: starting modal, count=%d rules=0x%X items=0x%X",
        count, initial_rules_flags, bits);

    // Announce title and first item with on/off
    char buf[512];
    bool on = !!(bits & (1u << 0));
    snprintf(buf, sizeof(buf), "%s. %s: %s",
        loc(SR_TMENU_RULES), items[0],
        on ? loc(SR_TMENU_OPT_ON) : loc(SR_TMENU_OPT_OFF));
    sr_output(buf, true);

    // Append help hint
    sr_output(loc(SR_SETUP_RULES_HELP), false);

    // Modal message loop — keyboard messages are consumed here,
    // NOT dispatched to ModWinProc.
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
                case VK_SPACE:
                    if (index < 16) {
                        bits ^= (1u << index);
                        sr_debug_log("RULES-CB: toggle item %d -> bits=0x%X", index, bits);
                    }
                    announce = true;
                    break;
                case VK_RETURN:
                    accepted = true;
                    want_close = true;
                    break;
                case VK_ESCAPE:
                    want_close = true;
                    break;
                case VK_F1:
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        sr_output(loc(SR_SETUP_RULES_HELP), true);
                    }
                    break;
                case 'S':
                    if (!(GetKeyState(VK_CONTROL) & 0x8000)) {
                        int enabled = 0;
                        for (int i = 0; i < count && i < 16; i++) {
                            if (bits & (1u << i)) enabled++;
                        }
                        snprintf(buf, sizeof(buf), "%d / %d", enabled, count);
                        sr_output(buf, true);
                    }
                    break;
                }

                if (announce && !want_close) {
                    bool item_on = (index < 16) && !!(bits & (1u << index));
                    snprintf(buf, sizeof(buf), "%s: %s",
                        items[index],
                        item_on ? loc(SR_TMENU_OPT_ON) : loc(SR_TMENU_OPT_OFF));
                    sr_output(buf, true);
                }
                continue; // consume keyboard messages, don't dispatch
            }
            if (msg.message == WM_KEYUP || msg.message == WM_CHAR
                || msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) {
                continue; // also consume related keyboard messages
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

    if (accepted) {
        sr_output(loc(SR_TMENU_RULES_SAVED), true);
        sr_debug_log("RULES-CB: accepted, bits=0x%X", bits);
        return (int)bits;
    } else {
        sr_output(loc(SR_TMENU_RULES_CANCELLED), true);
        sr_debug_log("RULES-CB: cancelled");
        return -1;
    }
}

/// Check if a popup call is the RULES checkbox popup during game setup.
static bool sr_is_setup_rules_popup(const char* filename, const char* label) {
    return sr_is_available()
        && label && _stricmp(label, "RULES") == 0
        && filename
        && (_stricmp(filename, "SCRIPT.txt") == 0
            || _stricmp(filename, "SCRIPT") == 0)
        && current_window() == GW_None;
}

/// Handle the RULES popup interception: full modal replacement (Option A).
/// The game only calls X_pop RULES when the user chose "Individuelle Regeln"
/// from the USERULES menu.
///
/// We NEVER call the original X_pop — that would require PostMessage hacks
/// to close the game's mouse-driven popup, which is fragile (Pattern C).
/// Instead we run our own modal and save the chosen RULES_* flags as
/// pending. init_world_config() applies them AFTER the game's own
/// initialization has set GameRules, so we get the last word.
// Defined here (before intercept function) so sr_hook_x_pop can call it.
static FX_pop sr_orig_x_pop = NULL;

static int sr_intercept_rules_popup(const char* filename, const char* label,
                                    int a3, int a4, int a5, int a6) {
    sr_popup_list_parse(filename, label);
    int modal_bits = sr_handle_rules_checkbox(DefaultRules);
    sr_popup_list_clear();

    if (modal_bits < 0) {
        sr_debug_log("RULES-CB: cancelled");
        return -1;
    }

    // Convert item-position bits to RULES_* flags and save as pending.
    // These will be applied in init_world_config() after the game's own
    // initialization, so the game can't overwrite them.
    uint32_t rules_flags = sr_items_to_rules((uint32_t)modal_bits);
    sr_pending_setup_rules = rules_flags;
    sr_pending_setup_rules_set = true;
    sr_debug_log("RULES-CB: accepted, item_bits=0x%X -> pending rules=0x%X",
        modal_bits, rules_flags);

    // Return item-position bits to the calling game code
    return modal_bits;
}

/// Accessible menu modal: replaces startup menus (TOPMENU, MAPMENU, etc.)
/// with a fully keyboard-navigable modal loop.
/// Returns 0-based index of selected item, or -1 if cancelled.
int sr_accessible_menu_modal(const char* title,
    char items[][256], int count)
{
    if (count <= 0 || count > SR_POPUP_LIST_MAX) {
        sr_debug_log("MENU-MODAL: bad count %d", count);
        return -1;
    }

    int index = 0;
    bool want_close = false;
    bool accepted = false;

    // Announce title and first item
    char buf[512];
    snprintf(buf, sizeof(buf), "%s. %s", title, items[0]);
    sr_output(buf, true);

    sr_debug_log("MENU-MODAL: %s, %d items", title, count);

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
                    accepted = true;
                    want_close = true;
                    break;
                case VK_ESCAPE:
                    want_close = true;
                    break;
                case VK_F1:
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        sr_output(loc(SR_TMENU_HELP), true);
                    }
                    break;
                }

                if (announce && !want_close) {
                    sr_output(items[index], true);
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

    if (accepted) {
        sr_debug_log("MENU-MODAL: selected %d (%s)", index, items[index]);
        return index;
    }
    sr_debug_log("MENU-MODAL: cancelled");
    return -1;
}

/*
Wrapper for popp: reads popup text from file and sends to screen reader,
then calls the original game function.
*/
// Interlude hook: capture current interlude ID for story text lookup
static fp_4int sr_orig_interlude = NULL;
static int sr_interlude_id = -1;

// Shared pre/post boilerplate for popup hook wrappers.
static void sr_pre_popup(const char* filename, const char* label) {
    sr_debug_log("PRE-POPUP file=%s label=%s", filename ? filename : "NULL",
        label ? label : "NULL");
    if (sr_is_available() && filename && label) {
        char buf[8192]; // Large buffer for long texts (interludes can be 3000+ chars)
        bool has_text = sr_read_popup_text(filename, label, buf, sizeof(buf));

        // Interlude header popup: read the actual story text instead
        if (_strnicmp(filename, "INTERLUDE", 9) == 0
            && _stricmp(label, "INTERLUDE") == 0
            && sr_interlude_id >= 0) {
            char story_label[32];
            snprintf(story_label, sizeof(story_label), "INTERLUDE%d", sr_interlude_id);
            has_text = sr_read_popup_text(filename, story_label, buf, sizeof(buf));
        }

        // Diplomacy context: prepend "Diplomacy with [faction]" before popup text
        if (*DiploWinState != 0 && *diplo_second_faction >= 0
            && *diplo_second_faction < MaxPlayerNum) {
            char prefix[512];
            snprintf(prefix, sizeof(prefix), loc(SR_DIPLO_OPEN),
                sr_game_str(MFactions[*diplo_second_faction].adj_name_faction));
            int plen = strlen(prefix);
            if (has_text) {
                // Prepend: "Diplomacy with X. [popup text]"
                int tlen = strlen(buf);
                if (plen + 2 + tlen < (int)sizeof(buf)) {
                    memmove(buf + plen + 2, buf, tlen + 1);
                    memcpy(buf, prefix, plen);
                    buf[plen] = '.';
                    buf[plen + 1] = ' ';
                }
            } else {
                // No popup text, just announce the diplomacy context
                strncpy(buf, prefix, sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                has_text = true;
            }
        }

        int options = sr_popup_list_parse(filename, label);
        // For info-only popups (0-1 options), append dismiss hint to text
        if (has_text && options <= 1) {
            int pos = strlen(buf);
            snprintf(buf + pos, sizeof(buf) - pos, " %s", loc(SR_POPUP_CONTINUE));
        }
        if (has_text) {
            sr_output(buf, false);
        } else if (options <= 1) {
            sr_output(loc(SR_POPUP_CONTINUE), false);
        }
    }
    sr_popup_active = true;
}

static void sr_post_popup() {
    sr_popup_active = false;
    if (sr_tutorial_announce_time > 0) {
        sr_tutorial_announce_time = GetTickCount();
    }
    sr_popup_list_clear();
    sr_clear_text();
}

static int __cdecl sr_hook_popp(const char* filename, const char* label,
                                int a3, const char* pcx, int a5) {
    // RULES checkbox popup goes through x_pop, not popp — just pass through.
    sr_pre_popup(filename, label);
    int result = sr_orig_popp(filename, label, a3, pcx, a5);
    sr_post_popup();
    return result;
}

static int __cdecl sr_hook_popb(const char* label, int flags,
                                int sound_id, const char* pcx, int a5) {
    sr_pre_popup("script", label);
    int result = sr_orig_popb(label, flags, sound_id, pcx, a5);
    sr_post_popup();
    return result;
}

// Saved original X_pops function pointer (sr_orig_x_pop declared earlier)
static FX_pops sr_orig_x_pops = NULL;

static int __cdecl sr_hook_x_pop(const char* filename, const char* label,
                                  int a3, int a4, int a5, int a6) {
    // Intercept RULES checkbox popup during game setup
    if (sr_is_setup_rules_popup(filename, label)) {
        sr_debug_log("RULES-CB: intercepted in x_pop, a3=%d a5=0x%X", a3, a5);
        return sr_intercept_rules_popup(filename, label, a3, a4, a5, a6);
    }
    sr_pre_popup(filename, label);
    int result = sr_orig_x_pop(filename, label, a3, a4, a5, a6);
    sr_post_popup();
    return result;
}

static int __cdecl sr_hook_x_pops(const char* filename, const char* label,
                                   int a3, int a4, int a5, int a6,
                                   int a7, int a8, int a9) {
    // Intercept RULES checkbox popup during game setup
    if (sr_is_setup_rules_popup(filename, label)) {
        sr_debug_log("RULES-CB: intercepted in x_pops, a3=%d", a3);
        return sr_intercept_rules_popup(filename, label, a3, a4, a5, a6);
    }
    sr_pre_popup(filename, label);
    int result = sr_orig_x_pops(filename, label, a3, a4, a5, a6, a7, a8, a9);
    sr_post_popup();
    return result;
}

static int __cdecl sr_hook_interlude(int id, int a2, int a3, int a4) {
    sr_interlude_id = id;
    game_log("Interlude %d", id);
    int result = sr_orig_interlude(id, a2, a3, a4);
    sr_interlude_id = -1;
    return result;
}

// Hook for tech_achieved: capture tech_id so popups can resolve $TECH0 correctly.
// Without this, $TECH0 may resolve to a stale base name from mod_random_events.
int sr_current_tech_achieved_id = -1;
static fp_4int sr_orig_tech_achieved = NULL;

static int __cdecl sr_hook_tech_achieved(int faction_id, int tech_id, int a3, int a4) {
    sr_current_tech_achieved_id = tech_id;
    int result = sr_orig_tech_achieved(faction_id, tech_id, a3, a4);
    sr_current_tech_achieved_id = -1;
    return result;
}

// Saved original planetfall function pointer
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
    sr_popup_active = true;
    int result = sr_orig_planetfall(faction_id);
    sr_popup_active = false;
    sr_clear_text();
    return result;
}

// ========== File Browser (Load/Save Dialog) ==========

#include "file_browser_handler.h"
#include "plan.h" // reset_state()

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

// ========== Number Input Dialog Hook ==========

// Saved original pop_ask_number function address
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

// ========== Inline Hook Implementation ==========

/*
Create a trampoline: copy stolen bytes from function start,
then append a JMP back to the original function body.
Returns pointer to callable trampoline, or NULL on failure.
*/
static uint8_t* create_trampoline(uint8_t* target, int stolen_bytes, uint8_t* tramp_slot) {
    // Copy original bytes to trampoline
    memcpy(tramp_slot, target, stolen_bytes);

    // Append JMP back to target + stolen_bytes
    tramp_slot[stolen_bytes] = 0xE9;
    int32_t jmp_offset = (int32_t)(target + stolen_bytes) - (int32_t)(tramp_slot + stolen_bytes) - 5;
    *(int32_t*)(tramp_slot + stolen_bytes + 1) = jmp_offset;

    return tramp_slot;
}

/*
Determine how many bytes to steal from a function prologue.
Decodes x86 instructions until we have >= 5 bytes of complete instructions.
Returns the count, or 0 if an unrecognized instruction is encountered too early.
*/
static int calc_stolen_bytes(uint8_t* func) {
    int pos = 0;

    while (pos < 5) {
        uint8_t op = func[pos];

        // Single-byte instructions
        if (op >= 0x50 && op <= 0x5F) {
            pos += 1; // push/pop reg
        } else if (op == 0x90) {
            pos += 1; // nop
        } else if (op == 0xCC) {
            pos += 1; // int3
        } else if (op == 0xC3) {
            break; // ret - can't steal past this

        // push imm8 / push imm32
        } else if (op == 0x6A) {
            pos += 2; // push imm8
        } else if (op == 0x68) {
            pos += 5; // push imm32

        // sub esp, imm8 / sub esp, imm32
        } else if (op == 0x83 && func[pos+1] == 0xEC) {
            pos += 3; // sub esp, imm8
        } else if (op == 0x81 && func[pos+1] == 0xEC) {
            pos += 6; // sub esp, imm32

        // mov eax, [imm32]
        } else if (op == 0xA1) {
            pos += 5;

        // FS segment prefix (0x64): used in SEH prologues (mov eax, fs:[0])
        } else if (op == 0x64) {
            uint8_t next = func[pos+1];
            if (next == 0xA1) {
                pos += 6; // mov eax, fs:[imm32]
            } else if (next == 0xA3) {
                pos += 6; // mov fs:[imm32], eax
            } else {
                break; // Unknown fs-prefixed instruction
            }

        // Two-byte opcodes with ModR/M: mov, xor, test, add, sub, cmp, and, or, lea
        } else if (op == 0x01 || op == 0x03 || // add
                   op == 0x09 || op == 0x0B || // or
                   op == 0x21 || op == 0x23 || // and
                   op == 0x29 || op == 0x2B || // sub
                   op == 0x31 || op == 0x33 || // xor
                   op == 0x39 || op == 0x3B || // cmp
                   op == 0x85 || op == 0x84 || // test
                   op == 0x89 || op == 0x8B || // mov
                   op == 0x8D) {               // lea
            uint8_t modrm = func[pos+1];
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm = modrm & 7;
            if (mod == 3) {
                pos += 2; // reg, reg
            } else if (mod == 0) {
                if (rm == 4) {
                    // SIB byte present; if SIB.base == 5 (EBP), add disp32
                    uint8_t sib = func[pos+2];
                    if ((sib & 7) == 5) {
                        pos += 7; // [SIB + disp32]
                    } else {
                        pos += 3; // [SIB]
                    }
                } else if (rm == 5) {
                    pos += 6; // [disp32]
                } else {
                    pos += 2; // [reg]
                }
            } else if (mod == 1) {
                pos += (rm == 4 ? 4 : 3); // [reg+disp8] (+SIB)
            } else if (mod == 2) {
                pos += (rm == 4 ? 7 : 6); // [reg+disp32] (+SIB)
            }

        } else {
            break; // Unknown opcode
        }
    }

    if (pos >= 5) {
        return pos;
    }
    return 0;
}

/*
Install an inline hook at target_addr, redirecting to hook_func.
The original function is preserved via a trampoline.
tramp_slot: pointer to pre-allocated executable memory for this trampoline.
Returns the trampoline pointer (callable as the original function), or NULL.
*/
static void* install_inline_hook(uint32_t target_addr, void* hook_func, uint8_t* tramp_slot) {
    uint8_t* target = (uint8_t*)target_addr;

    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf),
        "hook %08X: bytes %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        target_addr,
        target[0], target[1], target[2], target[3],
        target[4], target[5], target[6], target[7],
        target[8], target[9]);
    sr_log(logbuf);
    sr_hook_log(logbuf);

    int stolen = calc_stolen_bytes(target);
    if (stolen == 0) {
        snprintf(logbuf, sizeof(logbuf), "hook %08X: FAILED (unrecognized prologue)", target_addr);
        sr_log(logbuf);
        sr_hook_log(logbuf);
        return NULL;
    }

    snprintf(logbuf, sizeof(logbuf), "hook %08X: OK, stealing %d bytes, tramp=%08X",
        target_addr, stolen, (uint32_t)tramp_slot);
    sr_log(logbuf);
    sr_hook_log(logbuf);

    // Create trampoline with stolen bytes + JMP back
    uint8_t* trampoline = create_trampoline(target, stolen, tramp_slot);

    // Write JMP from target to our hook
    target[0] = 0xE9;
    *(int32_t*)(target + 1) = (int32_t)hook_func - (int32_t)target - 5;

    // NOP remaining stolen bytes
    for (int i = 5; i < stolen; i++) {
        target[i] = 0x90;
    }

    return (void*)trampoline;
}

// ========== Hook Functions ==========
// These replace the game's Buffer_write functions.
// They capture the text, then call the original via trampoline.

static int __thiscall hook_write_l(void* This, LPCSTR lpString, int x, int y, int max_len) {
    sr_trace_call("write_l", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0]) {
        sr_debug_log("NET-XY write_l buf=%p x=%d y=%d: %s", This, x, y, lpString);
    }
    sr_record_text(lpString, x, y, This);
    return orig_write_l((Buffer*)This, lpString, x, y, max_len);
}

static int __thiscall hook_write_l2(void* This, LPCSTR lpString, RECT* rc, int max_len) {
    sr_trace_call("write_l2", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0] && rc) {
        sr_debug_log("NET-XY write_l2 buf=%p x=%d y=%d: %s", This, rc->left, rc->top, lpString);
    }
    sr_record_text(lpString, rc ? rc->left : -1, rc ? rc->top : -1, This);
    return orig_write_l2((Buffer*)This, lpString, rc, max_len);
}

static int __thiscall hook_write_cent_l(void* This, LPCSTR lpString, int x, int y, int w, int max_len) {
    sr_trace_call("cent_l", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0]) {
        sr_debug_log("NET-XY cent_l buf=%p x=%d y=%d w=%d: %s", This, x, y, w, lpString);
    }
    sr_record_text(lpString, x, y, This);
    return orig_write_cent_l((Buffer*)This, lpString, x, y, w, max_len);
}

static int __thiscall hook_write_cent_l2(void* This, LPCSTR lpString, int a3, int a4, int a5, int a6, int max_len) {
    sr_trace_call("cent_l2", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0]) {
        sr_debug_log("NET-XY cent_l2 buf=%p x=%d y=%d: %s", This, a3, a4, lpString);
    }
    sr_record_text(lpString, a3, a4, This);
    return orig_write_cent_l2((Buffer*)This, lpString, a3, a4, a5, a6, max_len);
}

static int __thiscall hook_write_cent_l3(void* This, LPCSTR lpString, RECT* rc, int max_len) {
    sr_trace_call("cent_l3", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0] && rc) {
        sr_debug_log("NET-XY cent_l3 buf=%p x=%d y=%d: %s", This, rc->left, rc->top, lpString);
    }
    sr_record_text(lpString, rc ? rc->left : -1, rc ? rc->top : -1, This);
    return orig_write_cent_l3((Buffer*)This, lpString, rc, max_len);
}

static int __thiscall hook_write_cent_l4(void* This, LPCSTR lpString, int x, int y, int max_len) {
    sr_trace_call("cent_l4", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0]) {
        sr_debug_log("NET-XY cent_l4 buf=%p x=%d y=%d: %s", This, x, y, lpString);
    }
    sr_record_text(lpString, x, y, This);
    return orig_write_cent_l4((Buffer*)This, lpString, x, y, max_len);
}

static int __thiscall hook_write_right_l(void* This, LPCSTR lpString, int x, int y, int a5, int a6) {
    sr_trace_call("right_l", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0]) {
        sr_debug_log("NET-XY right_l buf=%p x=%d y=%d: %s", This, x, y, lpString);
    }
    sr_record_text(lpString, x, y, This);
    return orig_write_right_l((Buffer*)This, lpString, x, y, a5, a6);
}

static int __thiscall hook_write_right_l2(void* This, LPCSTR lpString, int x, int y, int max_len) {
    sr_trace_call("right_l2", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0]) {
        sr_debug_log("NET-XY right_l2 buf=%p x=%d y=%d: %s", This, x, y, lpString);
    }
    sr_record_text(lpString, x, y, This);
    return orig_write_right_l2((Buffer*)This, lpString, x, y, max_len);
}

static int __thiscall hook_write_right_l3(void* This, LPCSTR lpString, int a3, int a4) {
    sr_trace_call("right_l3", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0]) {
        sr_debug_log("NET-XY right_l3 buf=%p x=%d y=%d: %s", This, a3, a4, lpString);
    }
    sr_record_text(lpString, a3, a4, This);
    return orig_write_right_l3((Buffer*)This, lpString, a3, a4);
}

static int __thiscall hook_wrap(void* This, LPCSTR lpString, int a3) {
    sr_trace_call("wrap", lpString);
    sr_record_text(lpString, -1, -1, This);
    return orig_wrap((Buffer*)This, lpString, a3);
}

static int __thiscall hook_wrap2(void* This, LPCSTR lpString, int x, int y, int a5) {
    sr_trace_call("wrap2", lpString);
    if (sr_netsetup_log_coords && lpString && lpString[0]) {
        sr_debug_log("NET-XY wrap2 buf=%p x=%d y=%d: %s", This, x, y, lpString);
    }
    sr_record_text(lpString, x, y, This);
    return orig_wrap2((Buffer*)This, lpString, x, y, a5);
}

static int __thiscall hook_wrap_cent(void* This, LPCSTR lpString, int a3, int y, int a5) {
    sr_trace_call("wrap_cent", lpString);
    sr_record_text(lpString, a3, y, This);
    return orig_wrap_cent((Buffer*)This, lpString, a3, y, a5);
}

static int __thiscall hook_wrap_cent3(void* This, LPCSTR lpString, int a3) {
    sr_trace_call("wrap_cent3", lpString);
    sr_record_text(lpString, -1, -1, This);
    return orig_wrap_cent3((Buffer*)This, lpString, a3);
}

/*
Walk an MSVC6 STL red-black tree to collect all text segments in-order.

Structure (from diagnostic probing):
- a2 struct: DWORD[0-1]=fonts, DWORD[2]=header/sentinel, DWORD[4]=count
- Header node: offset 8=leftmost, offset 12=root, offset 16=rightmost
- Data nodes: offset 8=left child, offset 12=parent, offset 16=right child,
              offset 48=inline null-terminated text
- Header is the sentinel: data nodes link back to it (parent of root, right of rightmost, etc.)

Strips {curly} formatting markers from the output.
*/
// Pointer validity check: range check + memory readability
static inline bool sr_ptr_ok(void* p) {
    return p && (uintptr_t)p > 0x10000 && (uintptr_t)p < 0x7FFF0000;
}

// Check that a tree node is safe to dereference (pointers at offsets 8, 12, 16)
static inline bool sr_node_ok(void* node) {
    return sr_ptr_ok(node) && !IsBadReadPtr(node, 52);
}

static bool sr_walk_text_tree(uint8_t* header, int count, char* buf, int bufsize) {
    if (!sr_node_ok(header) || count <= 0 || !buf || bufsize < 2) return false;

    // Get root from header offset 12
    uint8_t* root = *(uint8_t**)(header + 12);
    if (!sr_node_ok(root) || root == header) return false;

    // Find leftmost node by descending left from root
    uint8_t* node = root;
    for (int safety = 0; safety < 200; safety++) {
        uint8_t* left = *(uint8_t**)(node + 8);
        if (!sr_node_ok(left) || left == header) break;
        node = left;
    }

    // In-order traversal collecting text
    int pos = 0;
    for (int visited = 0; visited < count && visited < 200; visited++) {
        if (!sr_node_ok(node) || node == header) break;

        // Read inline text at offset 48
        const char* src = (const char*)(node + 48);
        if (*src >= 0x20 && *src < 0x7F) {
            if (pos > 0 && pos < bufsize - 2) buf[pos++] = ' ';
            for (int i = 0; src[i] && pos < bufsize - 1 && i < 1024; i++) {
                if (src[i] == '{' || src[i] == '}') continue;
                buf[pos++] = src[i];
            }
        }

        // Move to next in-order successor
        uint8_t* right = *(uint8_t**)(node + 16);
        if (sr_node_ok(right) && right != header) {
            // Has right child: go right, then all the way left
            node = right;
            for (int s = 0; s < 200; s++) {
                uint8_t* left = *(uint8_t**)(node + 8);
                if (!sr_node_ok(left) || left == header) break;
                node = left;
            }
        } else {
            // No right child: go up until we come from a left child
            uint8_t* prev = node;
            node = NULL;
            for (int s = 0; s < 200; s++) {
                uint8_t* parent = *(uint8_t**)(prev + 12);
                if (!sr_node_ok(parent) || parent == header) break;
                uint8_t* parents_right = *(uint8_t**)(parent + 16);
                if (prev != parents_right) {
                    node = parent;
                    break;
                }
                prev = parent;
            }
            if (!node) break;
        }
    }

    buf[pos] = '\0';
    return pos > 0;
}

/*
Hook for Buffer_write_strings.
Extracts text from the tree data structure and sends to screen reader.
a2 struct: DWORD[2] = tree header, DWORD[4] = total count (incl. header),
DWORD[5] = data node count (real segments).
*/
static int __thiscall hook_write_strings(void* This, int a2, int a3, int y, int a5, char a6) {
    if (a2 && !IsBadReadPtr((void*)a2, 24)) {
        DWORD* data = (DWORD*)a2;
        uint8_t* header = (uint8_t*)data[2];
        int count = (int)data[4]; // includes header in count

        if (header && count > 0 && !IsBadReadPtr(header, 20)) {
            char buf[2048];
            if (sr_walk_text_tree(header, count, buf, sizeof(buf))) {
                sr_record_text(buf, -1, y, This);
            }
        }
    }
    return orig_write_strings((Buffer*)This, a2, a3, y, a5, a6);
}

/*
Install popup hooks via inline hooks on the actual game functions.
The game EXE calls these functions directly (not through Thinker's pointers),
so we must patch the function prologues at their real addresses.
tramp_slot: pointer to next available trampoline slot.
Returns number of hooks installed (0, 1, or 2).
*/
static int sr_install_popup_hooks(uint8_t* tramp_slot) {
    int count = 0;
    void* tramp;

    // Hook popp at 0x48C0A0
    tramp = install_inline_hook(0x48C0A0, (void*)sr_hook_popp, tramp_slot);
    if (tramp) {
        sr_orig_popp = (Fpopp)tramp;
        popp = sr_orig_popp;
        tramp_slot += 32;
        count++;
    }

    // Hook popb at 0x48C650
    tramp = install_inline_hook(0x48C650, (void*)sr_hook_popb, tramp_slot);
    if (tramp) {
        sr_orig_popb = (Fpopb)tramp;
        popb = sr_orig_popb;
        tramp_slot += 32;
        count++;
    }

    // Hook X_pop at 0x5BF480 (extended popup, used by tutorials)
    tramp = install_inline_hook(0x5BF480, (void*)sr_hook_x_pop, tramp_slot);
    if (tramp) {
        sr_orig_x_pop = (FX_pop)tramp;
        X_pop = sr_orig_x_pop;
        tramp_slot += 32;
        count++;
    }

    // Hook X_pops at 0x5BF930 (extended popup with more params)
    tramp = install_inline_hook(0x5BF930, (void*)sr_hook_x_pops, tramp_slot);
    if (tramp) {
        sr_orig_x_pops = (FX_pops)tramp;
        X_pops = sr_orig_x_pops;
        tramp_slot += 32;
        count++;
    }

    // Hook interlude at 0x5230E0 (story cutscenes)
    // NOTE: Do NOT set interlude = trampoline here. Mod code calls interlude()
    // through the function pointer, so it must hit the hooked address (0x5230E0)
    // to trigger sr_hook_interlude and capture the interlude ID.
    tramp = install_inline_hook(0x5230E0, (void*)sr_hook_interlude, tramp_slot);
    if (tramp) {
        sr_orig_interlude = (fp_4int)tramp;
        tramp_slot += 32;
        count++;
    }

    // Hook tech_achieved at 0x5BB000 (tech discovery popups)
    // Captures the tech_id so $TECH0 can be resolved correctly in popups.
    tramp = install_inline_hook(0x5BB000, (void*)sr_hook_tech_achieved, tramp_slot);
    if (tramp) {
        sr_orig_tech_achieved = (fp_4int)tramp;
        tramp_slot += 32;
        count++;
    }

    // Hook planetfall at 0x589180 (intro screen on game start)
    tramp = install_inline_hook(0x589180, (void*)sr_hook_planetfall, tramp_slot);
    if (tramp) {
        sr_orig_planetfall = (fp_1int)tramp;
        planetfall = sr_orig_planetfall;
        tramp_slot += 32;
        count++;
    }

    char logbuf[128];
    snprintf(logbuf, sizeof(logbuf), "popup hooks: %d of 7 installed", count);
    sr_hook_log(logbuf);
    sr_log(logbuf);
    return count;
}

/*
Install inline hooks on the main text rendering functions.
Must be called while game code section is writable (inside patch_setup scope).
*/
bool sr_install_text_hooks() {
    sr_log("sr_install_text_hooks: starting");
    // Clear persistent hook log on each new startup
    {
        char path[MAX_PATH];
        if (GetTempPathA(MAX_PATH, path)) {
            strncat(path, "thinker_hooks.log", MAX_PATH - strlen(path) - 1);
            FILE* f = fopen(path, "w");
            if (f) fclose(f);
        }
    }

    // Check for disable flag file in game directory
    // When present, disables ALL accessibility code (hooks + gui.cpp)
    FILE* nf = fopen("no_hooks.txt", "r");
    if (nf) {
        fclose(nf);
        sr_permanent_disabled = true;
        sr_hook_log("=== ALL SR DISABLED (no_hooks.txt found) ===");
        sr_log("sr_install_text_hooks: ALL DISABLED (no_hooks.txt)");
        debug("sr_install_text_hooks: ALL DISABLED (no_hooks.txt)\n");
        return true;
    }

    sr_hook_log("=== Hook Installation ===");
    // Allocate executable memory for trampolines
    // Each trampoline needs up to 32 bytes (stolen bytes + JMP)
    const int SR_HOOK_SLOTS = 27; // 25 active + 2 spare — increase when adding hooks
    trampoline_mem = (uint8_t*)VirtualAlloc(NULL, SR_HOOK_SLOTS * 32,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline_mem) {
        sr_log("sr_install_text_hooks: VirtualAlloc FAILED");
        return false;
    }

    uint8_t* slot = trampoline_mem;
    void* tramp;
    int hooked = 0;

    // Hook Buffer_write_l (0x5DCEA0) - left-aligned text
    tramp = install_inline_hook(0x5DCEA0, (void*)hook_write_l, slot);
    if (tramp) { orig_write_l = (FBuffer_write_l)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_l2 (0x5DCF40) - left-aligned text with RECT
    tramp = install_inline_hook(0x5DCF40, (void*)hook_write_l2, slot);
    if (tramp) { orig_write_l2 = (FBuffer_write_l2)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_cent_l (0x5DD020) - centered text
    tramp = install_inline_hook(0x5DD020, (void*)hook_write_cent_l, slot);
    if (tramp) { orig_write_cent_l = (FBuffer_write_cent_l)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_cent_l2 (0x5DD0E0) - centered text variant 2
    tramp = install_inline_hook(0x5DD0E0, (void*)hook_write_cent_l2, slot);
    if (tramp) { orig_write_cent_l2 = (FBuffer_write_cent_l2)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_cent_l3 (0x5DD130) - centered text with RECT
    tramp = install_inline_hook(0x5DD130, (void*)hook_write_cent_l3, slot);
    if (tramp) { orig_write_cent_l3 = (FBuffer_write_cent_l3)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_cent_l4 (0x5DD250) - centered text variant 4
    tramp = install_inline_hook(0x5DD250, (void*)hook_write_cent_l4, slot);
    if (tramp) { orig_write_cent_l4 = (FBuffer_write_cent_l4)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_right_l (0x5DD300) - right-aligned text
    tramp = install_inline_hook(0x5DD300, (void*)hook_write_right_l, slot);
    if (tramp) { orig_write_right_l = (FBuffer_write_right_l)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_right_l2 (0x5DD3B0) - right-aligned text variant 2
    tramp = install_inline_hook(0x5DD3B0, (void*)hook_write_right_l2, slot);
    if (tramp) { orig_write_right_l2 = (FBuffer_write_right_l2)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_right_l3 (0x5DD450) - right-aligned text variant 3
    tramp = install_inline_hook(0x5DD450, (void*)hook_write_right_l3, slot);
    if (tramp) { orig_write_right_l3 = (FBuffer_write_right_l3)tramp; slot += 32; hooked++; }

    // Hook Buffer_wrap (0x5DD530) - word-wrapped text
    tramp = install_inline_hook(0x5DD530, (void*)hook_wrap, slot);
    if (tramp) { orig_wrap = (FBuffer_wrap)tramp; slot += 32; hooked++; }

    // Hook Buffer_wrap2 (0x5DD730) - word-wrapped dialog text
    tramp = install_inline_hook(0x5DD730, (void*)hook_wrap2, slot);
    if (tramp) { orig_wrap2 = (FBuffer_wrap2)tramp; slot += 32; hooked++; }

    // Hook Buffer_wrap_cent (0x5DD920) - centered word-wrapped text
    tramp = install_inline_hook(0x5DD920, (void*)hook_wrap_cent, slot);
    if (tramp) { orig_wrap_cent = (FBuffer_wrap_cent)tramp; slot += 32; hooked++; }

    // Hook Buffer_wrap_cent3 (0x5DDAB0) - centered word-wrapped variant 3
    tramp = install_inline_hook(0x5DDAB0, (void*)hook_wrap_cent3, slot);
    if (tramp) { orig_wrap_cent3 = (FBuffer_wrap_cent3)tramp; slot += 32; hooked++; }

    // Hook Buffer_write_strings (0x5DB040) - formatted multi-segment text (popup bodies)
    tramp = install_inline_hook(0x5DB040, (void*)hook_write_strings, slot);
    if (tramp) { orig_write_strings = (FBuffer_write_strings)tramp; slot += 32; hooked++; }

    // Install popup text capture hooks (inline hooks on popp/popb)
    int popup_hooked = sr_install_popup_hooks(slot);
    hooked += popup_hooked;
    slot += popup_hooked * 32; // Advance past slots used by popup hooks

    // Hook load_game at 0x5AAAB0 (file browser for loading saves)
    tramp = install_inline_hook(0x5AAAB0, (void*)sr_hook_load_game, slot);
    if (tramp) {
        sr_orig_load_game = (fp_2int)tramp;
        slot += 32;
        hooked++;
        sr_hook_log("load_game hooked at 0x5AAAB0");
    }

    // Hook save_game at 0x5A9EB0 (file browser for saving)
    tramp = install_inline_hook(0x5A9EB0, (void*)sr_hook_save_game, slot);
    if (tramp) {
        sr_orig_save_game = (fp_1int)tramp;
        slot += 32;
        hooked++;
        sr_hook_log("save_game hooked at 0x5A9EB0");
    }

    // Hook call_council at 0x52C880 (Planetary Council)
    tramp = install_inline_hook(0x52C880, (void*)sr_hook_call_council, slot);
    if (tramp) {
        sr_orig_call_council = (fp_1int)tramp;
        slot += 32;
        hooked++;
        sr_hook_log("call_council hooked at 0x52C880");
    }

    // Hook the graphical event loop (0x602600) via inline hook.
    // This function is called from 120+ sites including all council vote screens.
    // We intercept it globally and only act when CouncilHandler::IsActive().
    // See docs/council-vote-reversing.md for details.
    tramp = install_inline_hook(0x602600, (void*)sr_council_vote_hook, slot);
    if (tramp) {
        sr_orig_gfx_event_loop = (FGfxEventLoop)tramp;
        slot += 32;
        hooked++;
        sr_hook_log("gfx event loop hooked at 0x602600 (council vote + global)");
    }

    // Hook the proposal buy-votes function (0x427230) via inline hook.
    // This function handles proposal vote interaction and uses a different
    // blocking mechanism than 0x602600. For human proposals, we show an
    // accessible YES/NO/ABSTAIN modal and skip the graphical buy-votes screen.
    tramp = install_inline_hook(0x427230, (void*)sr_council_buy_votes_hook, slot);
    if (tramp) {
        sr_orig_council_buy_votes = (FCouncilBuyVotes)tramp;
        slot += 32;
        hooked++;
        sr_hook_log("council buy-votes hooked at 0x427230 (proposal vote)");
    }

    // Hook pop_ask_number via function pointer redirect (no inline hook needed)
    sr_orig_pop_ask_number = pop_ask_number;
    pop_ask_number = sr_hook_pop_ask_number;
    hooked++;
    sr_hook_log("pop_ask_number redirected to sr_hook_pop_ask_number");

    char logbuf[128];
    snprintf(logbuf, sizeof(logbuf), "sr_install_text_hooks: done (%d of 26 hooked)", hooked);
    sr_log(logbuf);
    sr_hook_log(logbuf);
    return true;
}
