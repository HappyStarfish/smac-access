
#include "menu_handler.h"
#include "engine.h"
#include "gui.h"
#include "screen_reader.h"
#include "localization.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace MenuHandler {

// --- Static state ---

// Menu bar navigation state
static bool sr_menubar_active = false;
static int sr_menubar_index = 0;
static bool sr_submenu_active = false;
static int sr_submenu_index = 0;

// Menu item cache: tracks position via key counting, caches text
static struct {
    char items[32][256];  // cached text per position
    int pos;              // current position (0-based)
    bool active;          // cache is in use
} sr_mcache = {{}, 0, false};


// --- Helper functions ---

/// Strip '&' from menu/item caption for screen reader output.
static void sr_strip_ampersand(char* dst, const char* src, int maxlen) {
    int j = 0;
    for (int i = 0; src[i] && j < maxlen - 1; i++) {
        if (src[i] != '&') {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/// Get number of top-level menus from live game data.
static int sr_get_menu_count() {
    if (!MapWin) return 0;
    int count = MapWin->oMainMenu.iBaseMenuItemCount;
    if (count < 0 || count > 15) return 0;
    return count;
}

/// Get top-level menu name (ampersand-stripped) into buf.
static bool sr_get_menu_name(int index, char* buf, int bufsize) {
    if (!MapWin || index < 0 || index >= sr_get_menu_count()) return false;
    const char* caption = (const char*)MapWin->oMainMenu.aMainMenuItems[index].pszCaption;
    if (!caption) return false;
    sr_strip_ampersand(buf, caption, bufsize);
    return true;
}

/// Get the CMenu submenu for a top-level menu index. Returns NULL if invalid.
static CMenu* sr_get_submenu(int index) {
    if (!MapWin || index < 0 || index >= sr_get_menu_count()) return NULL;
    return MapWin->oMainMenu.aMainMenuItems[index].poSubMenu;
}

/// Get the number of VISIBLE items in a submenu.
static int sr_get_item_count(int menu_index) {
    CMenu* sub = sr_get_submenu(menu_index);
    if (!sub) return 0;
    int count = sub->iVisibleItemCount;
    if (count <= 0 || count > 64) count = sub->iMenuItemCount;
    if (count < 0 || count > 64) count = 0;
    return count;
}

/// Get submenu item caption (ampersand-stripped) into buf.
static bool sr_get_item_name(int menu_index, int item_index, char* buf, int bufsize) {
    CMenu* sub = sr_get_submenu(menu_index);
    if (!sub || item_index < 0 || item_index >= sr_get_item_count(menu_index)) return false;
    const char* caption = (const char*)sub->aMenuItems[item_index].pszCaption;
    if (!caption) return false;
    sr_strip_ampersand(buf, caption, bufsize);
    return true;
}

/// Get submenu item hotkey string into buf.
static bool sr_get_item_hotkey(int menu_index, int item_index, char* buf, int bufsize) {
    CMenu* sub = sr_get_submenu(menu_index);
    if (!sub || item_index < 0 || item_index >= sr_get_item_count(menu_index)) return false;
    const char* hk = (const char*)sub->aMenuItems[item_index].pszHotKey;
    if (!hk || !hk[0]) { buf[0] = '\0'; return false; }
    strncpy(buf, hk, bufsize - 1);
    buf[bufsize - 1] = '\0';
    return true;
}

/// Activate a submenu item by calling the game's menu handler callback.
static bool sr_activate_menu_item(int menu_index, int item_index) {
    CMenu* sub = sr_get_submenu(menu_index);
    if (!sub || item_index < 0 || item_index >= sr_get_item_count(menu_index)) return false;
    int menu_id = sub->aMenuItems[item_index].iMenuID;
    MENU_HANDLER_CB_F handler = MapWin->oMainMenu.pMenuHandlerCB;
    if (!handler) {
        debug("sr_activate_menu_item: no handler callback\n");
        return false;
    }
    sr_debug_log("MENU-ACTIVATE menu=%d item=%d id=%d", menu_index, item_index, menu_id);
    handler(menu_id);
    return true;
}


// --- Public API ---

bool IsActive() {
    return sr_menubar_active;
}

void OnLeaveWorldMap() {
    if (sr_menubar_active) {
        sr_menubar_active = false;
        sr_submenu_active = false;
    }
}

void InvalidateCache() {
    if (sr_mcache.active) {
        sr_debug_log("MCACHE invalidated");
        sr_mcache.active = false;
    }
}

bool IsCacheActive() {
    return sr_mcache.active;
}

void CacheStore(int pos, const char* text) {
    if (sr_mcache.active && pos >= 0 && pos < 32 && text) {
        strncpy(sr_mcache.items[pos], text, sizeof(sr_mcache.items[0]) - 1);
        sr_mcache.items[pos][255] = '\0';
    }
}

void CacheStoreAt(int pos, const char* text) {
    if (sr_mcache.active && pos >= 0 && pos < 32 && text
        && sr_mcache.items[pos][0] == '\0') {
        strncpy(sr_mcache.items[pos], text, sizeof(sr_mcache.items[0]) - 1);
        sr_mcache.items[pos][255] = '\0';
    }
}

int CacheGetPos() {
    return sr_mcache.pos;
}

void McLog(const char* fmt, ...) {
    FILE* f = fopen("mcache_debug.txt", "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

bool OnArrowKey(WPARAM dir, DWORD now, bool on_world_map, GameWinState cur_win,
                bool& sr_arrow_active, WPARAM& sr_arrow_dir, DWORD& sr_arrow_time) {
    if (on_world_map || cur_win == GW_Base || cur_win == GW_Design) {
        return false;
    }

    // Activate cache on first arrow press
    if (!sr_mcache.active) {
        memset(&sr_mcache, 0, sizeof(sr_mcache));
        sr_mcache.pos = 15;
        sr_mcache.active = true;
        McLog("MCACHE activated at pos=15");
    }

    // Update position
    if (dir == VK_DOWN) {
        sr_mcache.pos++;
        if (sr_mcache.pos >= 32) sr_mcache.pos = 31;
    } else {
        sr_mcache.pos--;
        if (sr_mcache.pos < 0) sr_mcache.pos = 0;
    }

    // Check cache for instant announce
    if (sr_mcache.items[sr_mcache.pos][0] != '\0') {
        McLog("HIT pos=%d: %s", sr_mcache.pos, sr_mcache.items[sr_mcache.pos]);
        sr_output(sr_mcache.items[sr_mcache.pos], true);
        sr_arrow_active = false;
        sr_arrow_dir = 0;
        sr_arrow_time = 0;
    } else {
        // Cache miss: fall through to Trigger 2
        sr_arrow_active = true;
        sr_arrow_dir = dir;
        sr_arrow_time = now;
        McLog("MISS pos=%d, waiting for T2", sr_mcache.pos);
    }
    sr_items_clear();
    sr_clear_text();
    sr_snapshot_consume();
    McLog("ARROW %s pos=%d", dir == VK_DOWN ? "DOWN" : "UP", sr_mcache.pos);

    return true;  // always consumed (cache hit = announce done, miss = Trigger 2 will handle)
}


bool HandleKey(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)hwnd; (void)lParam;
    if (!sr_is_available()) return false;
    if (msg != WM_KEYDOWN) return false;

    GameWinState cur_win = current_window();
    bool on_world_map_or_popup = (cur_win == GW_World
        || (cur_win == GW_None && *PopupDialogState >= 2));

    // Ctrl+F2 activates menu bar navigation (world map only)
    if (wParam == VK_F2 && ctrl_key_down()
        && sr_get_menu_count() > 0 && on_world_map_or_popup) {
        sr_menubar_active = true;
        sr_submenu_active = false;
        sr_menubar_index = 0;
        sr_submenu_index = 0;
        char buf[256], mname[64];
        sr_get_menu_name(0, mname, sizeof(mname));
        snprintf(buf, sizeof(buf), loc(SR_MENU_ENTRY_FMT),
            mname, sr_get_item_count(0));
        sr_output(buf, true);
        return true;
    }

    // Menu bar/submenu navigation while active
    if (!sr_menubar_active) return false;
    if (wParam != VK_LEFT && wParam != VK_RIGHT && wParam != VK_UP
        && wParam != VK_DOWN && wParam != VK_RETURN && wParam != VK_ESCAPE) {
        return false;
    }

    char buf[256], mname[64], iname[128], hkbuf[64];
    int menu_count = sr_get_menu_count();
    if (menu_count <= 0) { sr_menubar_active = false; return true; }

    // Helper: announce a submenu item at position
    auto announce_item = [&](int mi, int ii) {
        int ic = sr_get_item_count(mi);
        sr_get_item_name(mi, ii, iname, sizeof(iname));
        if (sr_get_item_hotkey(mi, ii, hkbuf, sizeof(hkbuf))) {
            char inner[200];
            snprintf(inner, sizeof(inner), loc(SR_MENU_ITEM_FMT), iname, hkbuf);
            snprintf(buf, sizeof(buf), loc(SR_MENU_NAV_FMT), ii + 1, ic, inner);
        } else {
            snprintf(buf, sizeof(buf), loc(SR_MENU_NAV_FMT), ii + 1, ic, iname);
        }
    };

    // Helper: announce menu header + first item
    auto announce_menu_and_first = [&](int mi) {
        sr_get_menu_name(mi, mname, sizeof(mname));
        int ic = sr_get_item_count(mi);
        char header[128];
        snprintf(header, sizeof(header), loc(SR_MENU_ENTRY_FMT), mname, ic);
        if (ic > 0) {
            sr_get_item_name(mi, 0, iname, sizeof(iname));
            if (sr_get_item_hotkey(mi, 0, hkbuf, sizeof(hkbuf))) {
                char inner[200];
                snprintf(inner, sizeof(inner), loc(SR_MENU_ITEM_FMT), iname, hkbuf);
                snprintf(buf, sizeof(buf), "%s. %s", header, inner);
            } else {
                snprintf(buf, sizeof(buf), "%s. %s", header, iname);
            }
        } else {
            snprintf(buf, sizeof(buf), "%s", header);
        }
    };

    if (sr_submenu_active) {
        // Level 2: Submenu item navigation
        int item_count = sr_get_item_count(sr_menubar_index);
        if (item_count <= 0) { sr_submenu_active = false; return true; }

        if (wParam == VK_DOWN) {
            sr_submenu_index = (sr_submenu_index + 1) % item_count;
            announce_item(sr_menubar_index, sr_submenu_index);
            sr_output(buf, true);
            return true;
        } else if (wParam == VK_UP) {
            sr_submenu_index = (sr_submenu_index - 1 + item_count) % item_count;
            announce_item(sr_menubar_index, sr_submenu_index);
            sr_output(buf, true);
            return true;
        } else if (wParam == VK_RIGHT) {
            sr_menubar_index = (sr_menubar_index + 1) % menu_count;
            sr_submenu_index = 0;
            announce_menu_and_first(sr_menubar_index);
            sr_output(buf, true);
            return true;
        } else if (wParam == VK_LEFT) {
            sr_menubar_index = (sr_menubar_index - 1 + menu_count) % menu_count;
            sr_submenu_index = 0;
            announce_menu_and_first(sr_menubar_index);
            sr_output(buf, true);
            return true;
        } else if (wParam == VK_ESCAPE) {
            sr_submenu_active = false;
            sr_get_menu_name(sr_menubar_index, mname, sizeof(mname));
            snprintf(buf, sizeof(buf), loc(SR_MENU_ENTRY_FMT),
                mname, sr_get_item_count(sr_menubar_index));
            sr_output(buf, true);
            return true;
        } else if (wParam == VK_RETURN) {
            sr_get_item_name(sr_menubar_index, sr_submenu_index, iname, sizeof(iname));
            sr_menubar_active = false;
            sr_submenu_active = false;
            sr_output(iname, true);
            sr_activate_menu_item(sr_menubar_index, sr_submenu_index);
            return true;
        }
    } else {
        // Level 1: Menu bar navigation
        if (wParam == VK_RIGHT) {
            sr_menubar_index = (sr_menubar_index + 1) % menu_count;
            sr_get_menu_name(sr_menubar_index, mname, sizeof(mname));
            snprintf(buf, sizeof(buf), loc(SR_MENU_ENTRY_FMT),
                mname, sr_get_item_count(sr_menubar_index));
            sr_output(buf, true);
            return true;
        } else if (wParam == VK_LEFT) {
            sr_menubar_index = (sr_menubar_index - 1 + menu_count) % menu_count;
            sr_get_menu_name(sr_menubar_index, mname, sizeof(mname));
            snprintf(buf, sizeof(buf), loc(SR_MENU_ENTRY_FMT),
                mname, sr_get_item_count(sr_menubar_index));
            sr_output(buf, true);
            return true;
        } else if (wParam == VK_DOWN || wParam == VK_RETURN) {
            int ic = sr_get_item_count(sr_menubar_index);
            if (ic > 0) {
                sr_submenu_active = true;
                sr_submenu_index = 0;
                announce_item(sr_menubar_index, 0);
                sr_output(buf, true);
            }
            return true;
        } else if (wParam == VK_UP) {
            return true;
        } else if (wParam == VK_ESCAPE) {
            sr_menubar_active = false;
            sr_submenu_active = false;
            sr_output(loc(SR_MENU_CLOSED), true);
            return true;
        }
    }

    return false;
}

} // namespace MenuHandler
