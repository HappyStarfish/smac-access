/*
 * File Browser accessibility handler.
 * Replaces the game's file browser dialog with a keyboard-navigable,
 * screen-reader accessible modal. Scans the saves directory directly
 * and lets the user navigate with arrow keys.
 *
 * Features:
 * - Up/Down: navigate file/folder list
 * - Enter on folder: navigate into it
 * - Enter on .SAV file: select it (returns path)
 * - Backspace: go to parent directory
 * - Escape: cancel
 * - Ctrl+F1: help
 */

#include "file_browser_handler.h"
#include "engine.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"

#include <windows.h>
#include <cstring>
#include <cstdio>

namespace FileBrowserHandler {

// Maximum entries in a directory listing
static const int MAX_ENTRIES = 256;
static const int MAX_PATH_LEN = 260;
static const int MAX_NAME_LEN = 256;

struct Entry {
    char name[MAX_NAME_LEN];  // display name (without extension for .SAV)
    char fname[MAX_NAME_LEN]; // original filename on disk
    bool is_dir;
};

static bool _active = false;
static bool _wantClose = false;
static bool _isSave = false;
static char _selectedPath[MAX_PATH_LEN] = {};  // result path
static char _currentDir[MAX_PATH_LEN] = {};    // absolute path to current dir
static char _savesRoot[MAX_PATH_LEN] = {};     // absolute path to saves/
static Entry _entries[MAX_ENTRIES];
static int _count = 0;
static int _index = 0;

// Simple case-insensitive compare for sorting
static int name_cmp(const char* a, const char* b) {
    return _stricmp(a, b);
}

// Sort entries: directories first, then files, each alphabetically
static void sort_entries() {
    // Simple insertion sort (MAX_ENTRIES is small)
    for (int i = 1; i < _count; i++) {
        Entry tmp = _entries[i];
        int j = i - 1;
        while (j >= 0) {
            bool swap = false;
            if (tmp.is_dir && !_entries[j].is_dir) {
                swap = true; // dirs before files
            } else if (tmp.is_dir == _entries[j].is_dir) {
                if (name_cmp(tmp.name, _entries[j].name) < 0) {
                    swap = true; // alphabetical within group
                }
            }
            if (!swap) break;
            _entries[j + 1] = _entries[j];
            j--;
        }
        _entries[j + 1] = tmp;
    }
}

// Scan _currentDir and populate _entries
static void scan_directory() {
    _count = 0;
    _index = 0;

    char pattern[MAX_PATH_LEN + 4];
    snprintf(pattern, sizeof(pattern), "%s\\*", _currentDir);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        sr_debug_log("FBH-SCAN: FindFirstFile failed for %s", pattern);
        return;
    }

    do {
        if (_count >= MAX_ENTRIES) break;
        // Skip "." and ".."
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        Entry* e = &_entries[_count];
        e->is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        strncpy(e->fname, fd.cFileName, MAX_NAME_LEN - 1);
        e->fname[MAX_NAME_LEN - 1] = '\0';

        // Display name: strip .SAV/.sav extension for files
        strncpy(e->name, fd.cFileName, MAX_NAME_LEN - 1);
        e->name[MAX_NAME_LEN - 1] = '\0';
        if (!e->is_dir) {
            int len = strlen(e->name);
            if (len > 4 && (_stricmp(e->name + len - 4, ".SAV") == 0
                         || _stricmp(e->name + len - 4, ".sav") == 0)) {
                e->name[len - 4] = '\0';
            }
        }

        _count++;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    sort_entries();
    sr_debug_log("FBH-SCAN: %d entries in %s", _count, _currentDir);
}

// Build relative path for game functions (e.g. "saves/auto/file.SAV")
static void build_relative_path(const char* filename, char* buf, int bufsize) {
    // _currentDir is absolute, _savesRoot is the "saves" dir absolute path
    // Game expects paths relative to game dir, like "saves/subfolder/file.SAV"
    // Find where _currentDir differs from game dir (parent of _savesRoot)
    char gameDir[MAX_PATH_LEN] = {};
    strncpy(gameDir, _savesRoot, MAX_PATH_LEN - 1);
    // _savesRoot ends with "saves", find parent
    char* lastSep = strrchr(gameDir, '\\');
    if (!lastSep) lastSep = strrchr(gameDir, '/');
    if (lastSep) *lastSep = '\0';

    // _currentDir should start with gameDir
    int gdLen = strlen(gameDir);
    const char* relDir = _currentDir;
    if (_strnicmp(_currentDir, gameDir, gdLen) == 0) {
        relDir = _currentDir + gdLen;
        if (*relDir == '\\' || *relDir == '/') relDir++;
    }

    if (relDir[0]) {
        snprintf(buf, bufsize, "%s\\%s", relDir, filename);
    } else {
        snprintf(buf, bufsize, "%s", filename);
    }
}

// Check if current dir is the saves root (can't go higher)
static bool is_at_root() {
    return _stricmp(_currentDir, _savesRoot) == 0;
}

// Announce current entry
static void announce_entry() {
    if (_count == 0) {
        sr_output(loc(SR_FILE_EMPTY_DIR), true);
        return;
    }

    Entry* e = &_entries[_index];
    const char* type = e->is_dir ? loc(SR_FILE_FOLDER) : "SAV";
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_FILE_ITEM_FMT),
        _index + 1, _count, e->name, type);
    sr_output(buf, true);
}

// Navigate into a subdirectory
static void enter_directory(const char* dirname) {
    char newPath[MAX_PATH_LEN];
    snprintf(newPath, sizeof(newPath), "%s\\%s", _currentDir, dirname);
    strncpy(_currentDir, newPath, MAX_PATH_LEN - 1);
    _currentDir[MAX_PATH_LEN - 1] = '\0';
    scan_directory();

    char buf[512];
    snprintf(buf, sizeof(buf), "%s, %d", dirname, _count);
    sr_output(buf, true);

    if (_count > 0) {
        announce_entry();
    }
}

// Go to parent directory
static void go_parent() {
    if (is_at_root()) {
        sr_output(loc(SR_FILE_AT_ROOT), true);
        return;
    }
    char* lastSep = strrchr(_currentDir, '\\');
    if (!lastSep) lastSep = strrchr(_currentDir, '/');
    if (lastSep && lastSep > _currentDir) {
        *lastSep = '\0';
    }
    scan_directory();

    sr_output(loc(SR_FILE_PARENT_DIR), true);
    if (_count > 0) {
        announce_entry();
    }
}

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return false;

    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    switch (wParam) {
    case VK_UP:
        if (_count > 0) {
            _index = (_index + _count - 1) % _count;
            announce_entry();
        }
        return true;

    case VK_DOWN:
        if (_count > 0) {
            _index = (_index + 1) % _count;
            announce_entry();
        }
        return true;

    case VK_RETURN:
        if (_count > 0) {
            Entry* e = &_entries[_index];
            if (e->is_dir) {
                enter_directory(e->fname);
            } else {
                // File selected — build path and close
                build_relative_path(e->fname, _selectedPath, sizeof(_selectedPath));
                sr_debug_log("FBH-SELECT: %s", _selectedPath);
                _wantClose = true;
            }
        }
        return true;

    case VK_BACK:
        go_parent();
        return true;

    case VK_ESCAPE:
        _selectedPath[0] = '\0';
        _wantClose = true;
        return true;

    case VK_HOME:
        if (_count > 0) {
            _index = 0;
            announce_entry();
        }
        return true;

    case VK_END:
        if (_count > 0) {
            _index = _count - 1;
            announce_entry();
        }
        return true;

    case VK_F1:
        if (ctrl) {
            sr_output(loc(SR_FILE_HELP), true);
            return true;
        }
        return false;

    default:
        return false;
    }
}

const char* RunModal(bool is_save) {
    _active = true;
    _wantClose = false;
    _isSave = is_save;
    _selectedPath[0] = '\0';

    // Build saves directory path from game EXE location
    char exepath[MAX_PATH_LEN] = {};
    GetModuleFileNameA(NULL, exepath, sizeof(exepath) - 1);
    char* lastSep = strrchr(exepath, '\\');
    if (!lastSep) lastSep = strrchr(exepath, '/');
    if (lastSep) *(lastSep + 1) = '\0';

    snprintf(_savesRoot, sizeof(_savesRoot), "%ssaves", exepath);
    strncpy(_currentDir, _savesRoot, MAX_PATH_LEN - 1);
    _currentDir[MAX_PATH_LEN - 1] = '\0';

    sr_debug_log("FBH-OPEN: saves=%s is_save=%d", _savesRoot, is_save);

    // Scan initial directory
    scan_directory();

    // Announce opening
    const char* title = is_save ? loc(SR_FILE_SAVE_GAME) : loc(SR_FILE_LOAD_GAME);
    char buf[512];
    snprintf(buf, sizeof(buf), "%s, %d. %s",
        title, _count, loc(SR_FILE_NAV_HINT));
    sr_output(buf, true);

    if (_count > 0) {
        announce_entry();
    }

    // Run modal message pump
    sr_run_modal_pump(&_wantClose);

    _active = false;

    if (_selectedPath[0]) {
        sr_debug_log("FBH-CLOSE: selected=%s", _selectedPath);
    } else {
        sr_debug_log("FBH-CLOSE: cancelled");
        sr_output(is_save ? loc(SR_FILE_SAVE_CANCELLED)
                          : loc(SR_FILE_LOAD_CANCELLED), true);
    }

    return _selectedPath;
}

} // namespace FileBrowserHandler
