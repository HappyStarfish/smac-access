/*
 * Screen reader integration for Thinker Accessibility Mod.
 * Loads Tolk.dll dynamically and provides text capture hooks.
 */

#include "main.h"
#include "screen_reader.h"
#include "localization.h"
#include <stdarg.h>

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
static const int SR_MAX_ITEMS = 32;
static const int SR_MAX_ITEM_LEN = 256;
static char sr_items[SR_MAX_ITEMS][SR_MAX_ITEM_LEN];
static int sr_items_y[SR_MAX_ITEMS];   // Y coordinate per item (-1 if unknown)
static int sr_items_count = 0;

// Snapshot: completed draw cycle saved here when buffer is auto-cleared.
// Allows gui.cpp to announce screens that transition quickly.
static char sr_snap_items[SR_MAX_ITEMS][SR_MAX_ITEM_LEN];
static int sr_snap_items_y[SR_MAX_ITEMS];
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

// Defer announce: text capture continues but gui.cpp should not fire announce triggers.
// Used by blocking hooks (planetfall) that collect rendered text and announce after return.
static bool sr_defer_flag = false;

bool sr_defer_active() { return sr_defer_flag; }
void sr_defer_set(bool active) { sr_defer_flag = active; }

static void sr_trace_call(const char* func, const char* text) {
    if (GetTickCount() < sr_trace_until) {
        char logbuf[512];
        snprintf(logbuf, sizeof(logbuf), "TRACE %s: %.200s", func, text ? text : "(null)");
        // Write directly to log, bypass sr_log to avoid recursion issues
        char path[MAX_PATH];
        if (GetTempPathA(MAX_PATH, path)) {
            strncat(path, "thinker_sr.log", MAX_PATH - strlen(path) - 1);
            FILE* f = fopen(path, "a");
            if (f) { fprintf(f, "%s\n", logbuf); fclose(f); }
        }
    }
}

// Write diagnostic info to a file accessible without admin rights
static void sr_log(const char* msg) {
    char path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, path)) {
        strncat(path, "thinker_sr.log", MAX_PATH - strlen(path) - 1);
        FILE* f = fopen(path, "a");
        if (f) {
            fprintf(f, "%s\n", msg);
            fclose(f);
        }
    }
}

// Persistent hook installation log (NOT cleared by debug toggle)
static void sr_hook_log(const char* msg) {
    char path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, path)) {
        strncat(path, "thinker_hooks.log", MAX_PATH - strlen(path) - 1);
        FILE* f = fopen(path, "a");
        if (f) {
            fprintf(f, "%s\n", msg);
            fclose(f);
        }
    }
}

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

bool sr_all_disabled() {
    return sr_disabled_flag;
}

bool sr_is_available() {
    if (sr_disabled_flag) return false;
    return hTolk != NULL && pTolk_Output != NULL;
}

/*
Convert ANSI string to wide string and output to screen reader.
*/
bool sr_output(const char* text, bool interrupt) {
    if (!text || !text[0] || !pTolk_Output) {
        return false;
    }
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

/*
Check if a string is known noise that should be filtered out.
*/
static bool sr_is_junk(const char* text) {
    if (strncmp(text, "Host ID:", 8) == 0) return true;
    if (strncmp(text, "Local ID:", 9) == 0) return true;
    // TURN COMPLETE floods the buffer during AI turns, preventing other announces
    if (strcmp(text, "TURN COMPLETE") == 0) return true;
    // Diplomacy status strings that leak during menu navigation
    if (strncmp(text, "No Contact", 10) == 0) return true;
    if (strncmp(text, "no contact", 10) == 0) return true;
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
Record text drawn by the game into the capture buffer.
Auto-clears when a new draw cycle starts (>100ms gap between calls).
Deduplicates against entire buffer within the same cycle.
*/
void sr_record_text(const char* text, int y) {
    if (!text || !text[0]) return;

    // During popup display, don't accumulate text (popup body already spoken)
    if (sr_popup_active) return;

    int len = strlen(text);
    if (len < 3) {
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
                sr_snap_items_y[i] = sr_items_y[i];
            }
            sr_snap_ready = true;
        }
        sr_text_pos = 0;
        sr_text_buf[0] = '\0';
        sr_items_count = 0;
    }
    sr_last_record_time = now;

    // Deduplicate: skip if this string is already anywhere in the buffer
    if (sr_text_pos > 0 && strstr(sr_text_buf, text) != NULL) {
        sr_debug_log("FILTERED dedup: %s", text);
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

    // Capture as individual item with prefix dedup:
    // If new text starts with the last item's text, replace it (same string growing)
    if (sr_items_count > 0) {
        const char* last = sr_items[sr_items_count - 1];
        int last_len = strlen(last);
        if (len >= last_len && strncmp(text, last, last_len) == 0) {
            strncpy(sr_items[sr_items_count - 1], text, SR_MAX_ITEM_LEN - 1);
            sr_items[sr_items_count - 1][SR_MAX_ITEM_LEN - 1] = '\0';
            if (y >= 0) sr_items_y[sr_items_count - 1] = y;
            sr_debug_log("CAPTURE update[%d]: %s", sr_items_count - 1, text);
            return;
        }
    }
    if (sr_items_count < SR_MAX_ITEMS) {
        strncpy(sr_items[sr_items_count], text, SR_MAX_ITEM_LEN - 1);
        sr_items[sr_items_count][SR_MAX_ITEM_LEN - 1] = '\0';
        sr_items_y[sr_items_count] = y;
        sr_items_count++;
        sr_debug_log("CAPTURE new[%d] y=%d: %s", sr_items_count - 1, y, text);
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

int sr_item_get_y(int index) {
    if (index >= 0 && index < sr_items_count) {
        return sr_items_y[index];
    }
    return -1;
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

void sr_snapshot_consume() {
    sr_snap_ready = false;
}

// ========== Debug Logging ==========

static bool sr_debug = false;

bool sr_debug_active() {
    return sr_debug;
}

void sr_debug_toggle() {
    sr_debug = !sr_debug;
    if (sr_debug) {
        // Clear old log file on enable
        char path[MAX_PATH];
        if (GetTempPathA(MAX_PATH, path)) {
            strncat(path, "thinker_sr.log", MAX_PATH - strlen(path) - 1);
            FILE* f = fopen(path, "w");
            if (f) fclose(f);
        }
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

SrPopupList sr_popup_list = {{}, 0, 0, false};

void sr_popup_list_clear() {
    sr_popup_list.active = false;
    sr_popup_list.count = 0;
    sr_popup_list.index = 0;
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

        strncpy(sr_popup_list.items[count], text, 255);
        sr_popup_list.items[count][255] = '\0';
        count++;
    }

    fclose(f);

    if (count > 1) {
        sr_popup_list.count = count;
        sr_popup_list.index = 0;
        sr_popup_list.active = true;
        sr_debug_log("POPUP-LIST parsed %d options from %s#%s",
            count, filename, label);
        for (int i = 0; i < count; i++) {
            sr_debug_log("  option[%d]: %s", i, sr_popup_list.items[i]);
        }
    }
    return count;
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
                        char* buf, int bufsize) {
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

        // Skip directive lines (#xs, #caption, #button, #wave, etc.)
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

        // Handle ^ paragraph separator
        if (line[0] == '^') {
            if (pos > 0 && pos < bufsize - 2) {
                buf[pos++] = ' ';
            }
            continue;
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

    {
        char logbuf[512];
        snprintf(logbuf, sizeof(logbuf), "POPUP read [%s#%s]: %.200s", filename, label, buf);
        sr_log(logbuf);
    }
    return has_body;
}

/*
Wrapper for popp: reads popup text from file and sends to screen reader,
then calls the original game function.
*/
static int __cdecl sr_hook_popp(const char* filename, const char* label,
                                int a3, const char* pcx, int a5) {
    if (sr_is_available() && filename && label) {
        char buf[2048];
        if (sr_read_popup_text(filename, label, buf, sizeof(buf))) {
            sr_output(buf, false); // queue, don't interrupt
        }
        sr_popup_list_parse(filename, label);
    }
    sr_popup_active = true;
    int result = sr_orig_popp(filename, label, a3, pcx, a5);
    sr_popup_active = false;
    sr_popup_list_clear();
    sr_clear_text();
    return result;
}

/*
Wrapper for popb: reads popup text from default script file and sends
to screen reader, then calls the original game function.
popb uses Script.txt as its text source (same as ScriptFile = "script").
*/
static int __cdecl sr_hook_popb(const char* label, int flags,
                                int sound_id, const char* pcx, int a5) {
    if (sr_is_available() && label) {
        char buf[2048];
        if (sr_read_popup_text("script", label, buf, sizeof(buf))) {
            sr_output(buf, false);
        }
        sr_popup_list_parse("script", label);
    }
    sr_popup_active = true;
    int result = sr_orig_popb(label, flags, sound_id, pcx, a5);
    sr_popup_active = false;
    sr_popup_list_clear();
    sr_clear_text();
    return result;
}

// Saved original X_pop/X_pops function pointers
static FX_pop sr_orig_x_pop = NULL;
static FX_pops sr_orig_x_pops = NULL;

/*
Wrapper for X_pop: reads popup text from file and sends to screen reader.
X_pop is the extended popup function used by tutorials and help topics.
*/
static int __cdecl sr_hook_x_pop(const char* filename, const char* label,
                                  int a3, int a4, int a5, int a6) {
    if (sr_is_available() && filename && label) {
        char buf[2048];
        if (sr_read_popup_text(filename, label, buf, sizeof(buf))) {
            sr_output(buf, false);
        }
        sr_popup_list_parse(filename, label);
    }
    sr_popup_active = true;
    int result = sr_orig_x_pop(filename, label, a3, a4, a5, a6);
    sr_popup_active = false;
    sr_popup_list_clear();
    sr_clear_text();
    return result;
}

/*
Wrapper for X_pops: reads popup text from file and sends to screen reader.
X_pops is the extended popup function with more formatting parameters.
*/
static int __cdecl sr_hook_x_pops(const char* filename, const char* label,
                                   int a3, int a4, int a5, int a6,
                                   int a7, int a8, int a9) {
    if (sr_is_available() && filename && label) {
        char buf[2048];
        if (sr_read_popup_text(filename, label, buf, sizeof(buf))) {
            sr_output(buf, false);
        }
        sr_popup_list_parse(filename, label);
    }
    sr_popup_active = true;
    int result = sr_orig_x_pops(filename, label, a3, a4, a5, a6, a7, a8, a9);
    sr_popup_active = false;
    sr_popup_list_clear();
    sr_clear_text();
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
        if (_stricmp(mf->filename, "FUNGBOY") == 0) {
            announced = sr_read_popup_text(ScriptFile, "PLANETFALLF", buf, sizeof(buf));
        }
        if (!announced) {
            announced = sr_read_popup_text(ScriptFile, "PLANETFALL", buf, sizeof(buf));
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
    sr_record_text(lpString, y);
    return orig_write_l((Buffer*)This, lpString, x, y, max_len);
}

static int __thiscall hook_write_l2(void* This, LPCSTR lpString, RECT* rc, int max_len) {
    sr_trace_call("write_l2", lpString);
    sr_record_text(lpString, rc ? rc->top : -1);
    return orig_write_l2((Buffer*)This, lpString, rc, max_len);
}

static int __thiscall hook_write_cent_l(void* This, LPCSTR lpString, int x, int y, int w, int max_len) {
    sr_trace_call("cent_l", lpString);
    sr_record_text(lpString, y);
    return orig_write_cent_l((Buffer*)This, lpString, x, y, w, max_len);
}

static int __thiscall hook_write_cent_l2(void* This, LPCSTR lpString, int a3, int a4, int a5, int a6, int max_len) {
    sr_trace_call("cent_l2", lpString);
    sr_record_text(lpString, a4);
    return orig_write_cent_l2((Buffer*)This, lpString, a3, a4, a5, a6, max_len);
}

static int __thiscall hook_write_cent_l3(void* This, LPCSTR lpString, RECT* rc, int max_len) {
    sr_trace_call("cent_l3", lpString);
    sr_record_text(lpString, rc ? rc->top : -1);
    return orig_write_cent_l3((Buffer*)This, lpString, rc, max_len);
}

static int __thiscall hook_write_cent_l4(void* This, LPCSTR lpString, int x, int y, int max_len) {
    sr_trace_call("cent_l4", lpString);
    sr_record_text(lpString, y);
    return orig_write_cent_l4((Buffer*)This, lpString, x, y, max_len);
}

static int __thiscall hook_write_right_l(void* This, LPCSTR lpString, int x, int y, int a5, int a6) {
    sr_trace_call("right_l", lpString);
    sr_record_text(lpString, y);
    return orig_write_right_l((Buffer*)This, lpString, x, y, a5, a6);
}

static int __thiscall hook_write_right_l2(void* This, LPCSTR lpString, int x, int y, int max_len) {
    sr_trace_call("right_l2", lpString);
    sr_record_text(lpString, y);
    return orig_write_right_l2((Buffer*)This, lpString, x, y, max_len);
}

static int __thiscall hook_write_right_l3(void* This, LPCSTR lpString, int a3, int a4) {
    sr_trace_call("right_l3", lpString);
    sr_record_text(lpString, a4);
    return orig_write_right_l3((Buffer*)This, lpString, a3, a4);
}

static int __thiscall hook_wrap(void* This, LPCSTR lpString, int a3) {
    sr_trace_call("wrap", lpString);
    sr_record_text(lpString, -1);
    return orig_wrap((Buffer*)This, lpString, a3);
}

static int __thiscall hook_wrap2(void* This, LPCSTR lpString, int x, int y, int a5) {
    sr_trace_call("wrap2", lpString);
    sr_record_text(lpString, y);
    return orig_wrap2((Buffer*)This, lpString, x, y, a5);
}

static int __thiscall hook_wrap_cent(void* This, LPCSTR lpString, int a3, int y, int a5) {
    sr_trace_call("wrap_cent", lpString);
    sr_record_text(lpString, y);
    return orig_wrap_cent((Buffer*)This, lpString, a3, y, a5);
}

static int __thiscall hook_wrap_cent3(void* This, LPCSTR lpString, int a3) {
    sr_trace_call("wrap_cent3", lpString);
    sr_record_text(lpString, -1);
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
                sr_record_text(buf, y);
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

    // Hook planetfall at 0x589180 (intro screen on game start)
    tramp = install_inline_hook(0x589180, (void*)sr_hook_planetfall, tramp_slot);
    if (tramp) {
        sr_orig_planetfall = (fp_1int)tramp;
        planetfall = sr_orig_planetfall;
        tramp_slot += 32;
        count++;
    }

    char logbuf[128];
    snprintf(logbuf, sizeof(logbuf), "popup hooks: %d of 5 installed", count);
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
        sr_disabled_flag = true;
        sr_hook_log("=== ALL SR DISABLED (no_hooks.txt found) ===");
        sr_log("sr_install_text_hooks: ALL DISABLED (no_hooks.txt)");
        debug("sr_install_text_hooks: ALL DISABLED (no_hooks.txt)\n");
        return true;
    }

    sr_hook_log("=== Hook Installation ===");
    // Allocate executable memory for trampolines
    // Each trampoline needs up to 32 bytes (stolen bytes + JMP)
    // Allocate space for 19 trampolines (13 text + 5 popup/planetfall + 1 write_strings)
    trampoline_mem = (uint8_t*)VirtualAlloc(NULL, 20 * 32,
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

    char logbuf[128];
    snprintf(logbuf, sizeof(logbuf), "sr_install_text_hooks: done (%d of 19 hooked)", hooked);
    sr_log(logbuf);
    sr_hook_log(logbuf);
    return true;
}
