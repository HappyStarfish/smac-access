/*
 * FactionSelectHandler — Accessibility for faction selection screen.
 *
 * The faction selection screen shows available factions in the current group.
 * Up/Down arrows navigate (handled by game), each step fires a BLURB popup.
 *
 * This handler:
 *   - Builds announcements: "FactionName. BLURB text"
 *   - Suppresses arrow nav / snapshot triggers (prevents announcement chaos)
 *   - Provides keyboard shortcuts for top buttons
 *
 * Keys:
 *   Enter  = play selected group
 *   G      = edit group
 *   I      = faction info
 *   R      = random group
 *   Escape = cancel (handled by game, passed through)
 *   Ctrl+F1 = help
 *
 * Button discovery: On first activation, scans captured items for known button
 * labels, saves their Buffer pointers, and uses Buffer->poOwner Win rect
 * to find screen coordinates for click simulation.
 */

#include "faction_select_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "engine_win.h"

namespace FactionSelectHandler {

static bool _active = false;
static char _lastBlurbFile[64] = {};
static char _prevBlurbFile[64] = {};

// Known button IDs
enum ButtonId {
    BTN_PLAY = 0,   // "GEWÄHLTE GRUPPE SPIELEN" / "OK"
    BTN_EDIT,        // "GRUPPE BEARBEITEN"
    BTN_RANDOM,      // "ZUFALLSGRUPPE SPIELEN"
    BTN_CANCEL,      // "ABBRECHEN"
    BTN_COUNT
};

// Cached button Win pointers (from Buffer->poOwner)
static Win* _buttonWin[BTN_COUNT] = {};

bool IsActive() {
    return _active;
}

/// Read faction name from the faction text file.
static bool ReadFactionName(const char* filename, char* name, int namesize) {
    if (!filename || !filename[0]) return false;

    char path[MAX_PATH];
    char exepath[MAX_PATH];
    GetModuleFileNameA(NULL, exepath, MAX_PATH);
    char* last_slash = strrchr(exepath, '\\');
    if (last_slash) *(last_slash + 1) = '\0';

    snprintf(path, sizeof(path), "%s%s.txt", exepath, filename);

    FILE* f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    bool found_section = false;
    char section_tag[64];
    snprintf(section_tag, sizeof(section_tag), "#%s", filename);

    while (fgets(line, sizeof(line), f)) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (_strnicmp(line, section_tag, strlen(section_tag)) == 0) {
            found_section = true;
            continue;
        }
        if (found_section && line[0] != ';' && line[0] != '\0') {
            char* comma = strchr(line, ',');
            if (comma) {
                int nlen = comma - line;
                if (nlen >= namesize) nlen = namesize - 1;
                strncpy(name, line, nlen);
                name[nlen] = '\0';
                while (nlen > 0 && name[nlen-1] == ' ') name[--nlen] = '\0';
            } else {
                strncpy(name, line, namesize - 1);
                name[namesize - 1] = '\0';
            }
            fclose(f);
            char utf8[256];
            sr_ansi_to_utf8(name, utf8, sizeof(utf8));
            strncpy(name, utf8, namesize - 1);
            name[namesize - 1] = '\0';
            return true;
        }
    }
    fclose(f);
    return false;
}

/// Match a captured item text to a known button ID.
/// Returns -1 if not a button.
static int MatchButton(const char* text) {
    if (!text) return -1;
    // German labels (from log capture)
    if (strstr(text, "GEWÄHLTE") || strstr(text, "GEW\xC3\x84HLTE")
        || strstr(text, "SELECTED")) return BTN_PLAY;
    if (strstr(text, "BEARBEITEN") || strstr(text, "EDIT")) return BTN_EDIT;
    if (strstr(text, "ZUFALLS") || strstr(text, "RANDOM")) return BTN_RANDOM;
    if (strstr(text, "ABBRECHEN") || strstr(text, "CANCEL")) return BTN_CANCEL;
    return -1;
}

/// Scan captured items for button labels and save their Buffer->poOwner Win pointers.
static void ScanButtons() {
    for (int b = 0; b < BTN_COUNT; b++) _buttonWin[b] = NULL;

    int count = sr_item_count();
    sr_debug_log("FACTION btn scan: %d items", count);

    for (int i = 0; i < count; i++) {
        const char* text = sr_item_get(i);
        if (!text) continue;

        int btn = MatchButton(text);
        if (btn < 0) continue;

        void* raw_buf = sr_item_get_buf(i);
        if (!raw_buf) {
            sr_debug_log("FACTION btn[%d] '%s': no buffer", btn, text);
            continue;
        }

        Buffer* buf = (Buffer*)raw_buf;
        if (IsBadReadPtr(buf, sizeof(Buffer))) {
            sr_debug_log("FACTION btn[%d] '%s': buffer unreadable", btn, text);
            continue;
        }

        Win* owner = buf->poOwner;
        if (!owner || IsBadReadPtr(owner, sizeof(Win))) {
            sr_debug_log("FACTION btn[%d] '%s': owner unreadable", btn, text);
            continue;
        }

        _buttonWin[btn] = owner;

        // Log the Win's screen rect for diagnostics
        sr_debug_log("FACTION btn[%d] '%s': buf=%p owner=%p rect=(%d,%d,%d,%d)",
            btn, text, raw_buf, (void*)owner,
            owner->rRect1.left, owner->rRect1.top,
            owner->rRect1.right, owner->rRect1.bottom);
    }

    int found = 0;
    for (int b = 0; b < BTN_COUNT; b++) if (_buttonWin[b]) found++;
    sr_debug_log("FACTION btn scan complete: %d/%d buttons found", found, BTN_COUNT);
}

/// Click in the center of a button's Win screen rect.
static bool ClickButton(HWND hwnd, ButtonId btn) {
    if (btn < 0 || btn >= BTN_COUNT) return false;
    Win* w = _buttonWin[btn];
    if (!w || IsBadReadPtr(w, sizeof(Win))) {
        sr_debug_log("FACTION click btn[%d]: no Win cached", btn);
        return false;
    }

    RECT r = w->rRect1;
    int cx = (r.left + r.right) / 2;
    int cy = (r.top + r.bottom) / 2;

    // Sanity check
    if (cx <= 0 || cy <= 0 || cx > 3000 || cy > 3000) {
        sr_debug_log("FACTION click btn[%d]: bad coords (%d,%d)", btn, cx, cy);
        return false;
    }

    LPARAM lp = MAKELPARAM(cx, cy);
    PostMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    PostMessage(hwnd, WM_LBUTTONUP, 0, lp);
    sr_debug_log("FACTION click btn[%d] at (%d,%d) rect=(%d,%d,%d,%d)",
        btn, cx, cy, r.left, r.top, r.right, r.bottom);
    return true;
}

/// Read faction info (DATALINKS1 + DATALINKS2) directly from faction file
/// and output via screen reader, bypassing the game's INFO popup.
static void ReadFactionInfo(const char* filename) {
    if (!filename || !filename[0]) {
        sr_output(loc(SR_FACTION_NO_INFO), true);
        return;
    }

    char buf1[4096] = {};
    char buf2[4096] = {};
    bool has1 = sr_read_popup_text(filename, "DATALINKS1", buf1, sizeof(buf1));
    bool has2 = sr_read_popup_text(filename, "DATALINKS2", buf2, sizeof(buf2));

    if (!has1 && !has2) {
        sr_output(loc(SR_FACTION_NO_INFO), true);
        sr_debug_log("FACTION info: no DATALINKS sections in %s", filename);
        return;
    }

    char combined[8192];
    if (has1 && has2) {
        snprintf(combined, sizeof(combined), "%s %s", buf1, buf2);
    } else if (has1) {
        strncpy(combined, buf1, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
    } else {
        strncpy(combined, buf2, sizeof(combined) - 1);
        combined[sizeof(combined) - 1] = '\0';
    }

    sr_ansi_to_utf8(combined, buf1, sizeof(buf1));
    sr_output(buf1, true);
    sr_debug_log("FACTION info: read from %s (%d chars)", filename, (int)strlen(buf1));
}

bool OnBlurbDetected(const char* filename, const char* blurb_text,
    char* buf, int bufsize)
{
    if (!filename) return false;

    bool first_activation = false;
    if (!_active) {
        _active = true;
        _prevBlurbFile[0] = '\0';
        first_activation = true;
        sr_debug_log("FACTION activated");
    }

    // Rescan buttons every time — Win pointers may change after screen transitions
    ScanButtons();

    strncpy(_lastBlurbFile, filename, sizeof(_lastBlurbFile) - 1);
    _lastBlurbFile[sizeof(_lastBlurbFile) - 1] = '\0';

    // Skip duplicate announcements (same faction as last time)
    if (!first_activation && strcmp(_lastBlurbFile, _prevBlurbFile) == 0) {
        sr_debug_log("FACTION skip dup: %s", filename);
        return false;
    }
    strncpy(_prevBlurbFile, _lastBlurbFile, sizeof(_prevBlurbFile) - 1);
    _prevBlurbFile[sizeof(_prevBlurbFile) - 1] = '\0';

    // Build announcement: "FactionName. BLURB text"
    char fname[128];
    if (ReadFactionName(filename, fname, sizeof(fname))) {
        if (blurb_text && blurb_text[0]) {
            snprintf(buf, bufsize, "%s. %s", fname, blurb_text);
        } else {
            snprintf(buf, bufsize, "%s", fname);
        }
    } else if (blurb_text && blurb_text[0]) {
        snprintf(buf, bufsize, "%s", blurb_text);
    } else {
        return false;
    }

    sr_debug_log("FACTION announce: %s (file=%s)", fname, filename);
    return true;
}

void OnNonBlurbPopup() {
    if (_active) {
        sr_debug_log("FACTION deactivated");
        _active = false;
        _lastBlurbFile[0] = '\0';
        _prevBlurbFile[0] = '\0';
        for (int b = 0; b < BTN_COUNT; b++) _buttonWin[b] = NULL;
    }
}

bool HandleKey(HWND hwnd, UINT msg, WPARAM wParam) {
    if (!_active || !sr_is_available()) return false;
    if (msg != WM_KEYDOWN) return false;

    bool ctrl = ctrl_key_down();
    bool shift = shift_key_down();
    bool alt = alt_key_down();

    // Ctrl+F1 = help
    if (wParam == VK_F1 && ctrl && !shift && !alt) {
        sr_output(loc(SR_FACTION_HELP), true);
        return true;
    }

    // Don't intercept modified keys
    if (ctrl || shift || alt) return false;

    // G = edit group
    if (wParam == 'G') {
        if (ClickButton(hwnd, BTN_EDIT)) return true;
        sr_debug_log("FACTION G: edit button not found");
        return false;
    }

    // I = faction info (read directly from file, no popup)
    if (wParam == 'I') {
        ReadFactionInfo(_lastBlurbFile);
        return true;
    }

    // R = random group
    if (wParam == 'R') {
        if (ClickButton(hwnd, BTN_RANDOM)) return true;
        sr_debug_log("FACTION R: random button not found");
        return false;
    }

    // Don't consume other keys
    return false;
}

} // namespace FactionSelectHandler
