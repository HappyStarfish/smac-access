
#include "message_handler.h"
#include "engine.h"
#include "gui.h"
#include "screen_reader.h"
#include "localization.h"
#include "world_map_handler.h"

namespace MessageHandler {

struct SrMessage {
    char text[512];
    char label[64];
    int x, y;
    int base_id;
    int turn;
};

static const int MSG_BUFFER_SIZE = 50;
static SrMessage _buffer[MSG_BUFFER_SIZE];
static int _writeIndex = 0;
static int _count = 0;
static bool _browserActive = false;

static void capture_coordinates(int& out_x, int& out_y, int& out_base_id) {
    out_x = -1;
    out_y = -1;
    out_base_id = -1;

    // Try current base first
    if (*CurrentBaseID >= 0 && *CurrentBaseID < MaxBaseNum) {
        BASE* b = &Bases[*CurrentBaseID];
        out_x = b->x;
        out_y = b->y;
        out_base_id = *CurrentBaseID;
        return;
    }
    // Try current unit
    if (MapWin && MapWin->iUnit >= 0 && MapWin->iUnit < *VehCount) {
        VEH* v = &Vehs[MapWin->iUnit];
        out_x = v->x;
        out_y = v->y;
        return;
    }
    // Fallback: map center tile
    if (MapWin) {
        out_x = MapWin->iTileX;
        out_y = MapWin->iTileY;
    }
}

static const SrMessage* get_message(int index) {
    if (index < 0 || index >= _count) return NULL;
    int buf_idx = (_writeIndex - _count + index + MSG_BUFFER_SIZE) % MSG_BUFFER_SIZE;
    return &_buffer[buf_idx];
}

bool OnMessage(const char* label, const char* filename) {
    if (!sr_is_available() || !label) return false;

    char buf[512];
    const char* file = filename ? filename : "script";
    if (!sr_read_popup_text(file, label, buf, sizeof(buf)) || !buf[0]) {
        return false;
    }
    sr_substitute_game_vars(buf, sizeof(buf));

    // Store in ring buffer
    SrMessage& msg = _buffer[_writeIndex];
    strncpy(msg.text, buf, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';
    strncpy(msg.label, label, sizeof(msg.label) - 1);
    msg.label[sizeof(msg.label) - 1] = '\0';
    capture_coordinates(msg.x, msg.y, msg.base_id);
    msg.turn = *CurrentTurn;

    _writeIndex = (_writeIndex + 1) % MSG_BUFFER_SIZE;
    if (_count < MSG_BUFFER_SIZE) _count++;

    // Auto-announce (queued, non-interrupting)
    char announce[600];
    snprintf(announce, sizeof(announce), loc(SR_MSG_NOTIFICATION), buf);
    sr_output(announce, false);

    sr_debug_log("MSG label=%s x=%d y=%d text=%.60s", label, msg.x, msg.y, buf);
    return true;
}

static void announce_item(int index) {
    const SrMessage* msg = get_message(index);
    if (!msg) return;
    char buf[600];
    if (msg->x >= 0 && msg->y >= 0) {
        snprintf(buf, sizeof(buf), loc(SR_MSG_ITEM_LOC),
            index + 1, _count, msg->text, msg->x, msg->y);
    } else {
        snprintf(buf, sizeof(buf), loc(SR_MSG_ITEM),
            index + 1, _count, msg->text);
    }
    sr_output(buf, true);
}

void OpenBrowser() {
    if (_count == 0) {
        sr_output(loc(SR_MSG_EMPTY), true);
        return;
    }

    _browserActive = true;
    int index = _count - 1; // start at newest

    char buf[600];
    snprintf(buf, sizeof(buf), loc(SR_MSG_OPEN), _count);
    sr_output(buf, true);
    announce_item(index);

    bool want_close = false;
    bool jump = false;
    MSG modal_msg;

    while (!want_close) {
        if (PeekMessage(&modal_msg, NULL, 0, 0, PM_REMOVE)) {
            if (modal_msg.message == WM_QUIT) {
                PostQuitMessage((int)modal_msg.wParam);
                break;
            }
            if (modal_msg.message == WM_KEYDOWN) {
                WPARAM k = modal_msg.wParam;
                if (k == VK_ESCAPE) {
                    want_close = true;
                } else if (k == VK_RETURN) {
                    const SrMessage* msg = get_message(index);
                    if (msg && msg->x >= 0 && msg->y >= 0) {
                        want_close = true;
                        jump = true;
                    } else {
                        sr_output(loc(SR_MSG_NO_LOCATION), true);
                    }
                } else if (k == VK_UP) {
                    index = (index - 1 + _count) % _count;
                    announce_item(index);
                } else if (k == VK_DOWN) {
                    index = (index + 1) % _count;
                    announce_item(index);
                } else if (k == VK_HOME) {
                    index = 0;
                    announce_item(index);
                } else if (k == VK_END) {
                    index = _count - 1;
                    announce_item(index);
                } else if (k == 'S' || k == VK_TAB) {
                    snprintf(buf, sizeof(buf), loc(SR_MSG_SUMMARY),
                        _count, *CurrentTurn);
                    sr_output(buf, true);
                } else if (k == VK_F1 && ctrl_key_down()) {
                    sr_output(loc(SR_MSG_HELP), true);
                }
            }
        } else {
            Sleep(10);
        }
    }

    // Drain leftover messages
    MSG drain;
    while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
    while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}

    _browserActive = false;

    if (jump) {
        const SrMessage* msg = get_message(index);
        if (msg && msg->x >= 0 && msg->y >= 0) {
            WorldMapHandler::SetCursor(msg->x, msg->y);
        }
    } else {
        sr_output(loc(SR_MSG_CLOSED), true);
    }
}

int GetCount() {
    return _count;
}

void Clear() {
    _count = 0;
    _writeIndex = 0;
}

bool IsActive() {
    return _browserActive;
}

} // namespace MessageHandler
