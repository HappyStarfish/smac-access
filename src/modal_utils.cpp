
#include "modal_utils.h"
#include "screen_reader.h"

bool sr_modal_handle_utility_key(WPARAM key) {
    // Ctrl+F12: toggle debug logging
    if (key == VK_F12 && (GetKeyState(VK_CONTROL) & 0x8000)) {
        sr_debug_toggle();
        return true;
    }
    return false;
}

void sr_run_modal_pump(bool* want_close) {
    MSG msg;
    while (!*want_close) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
            // Skip TranslateMessage for keyboard messages to prevent
            // WM_CHAR generation that screen readers read as characters
            if (msg.message != WM_KEYDOWN && msg.message != WM_KEYUP
                && msg.message != WM_SYSKEYDOWN && msg.message != WM_SYSKEYUP) {
                TranslateMessage(&msg);
            }
            DispatchMessage(&msg);
        } else {
            Sleep(10);
        }
    }

    // Drain leftover WM_CHAR/WM_KEYUP from the last key press to prevent
    // them from reaching the game's WinProc after we return
    MSG drain;
    while (PeekMessage(&drain, NULL, WM_CHAR, WM_CHAR, PM_REMOVE)) {}
    while (PeekMessage(&drain, NULL, WM_KEYUP, WM_KEYUP, PM_REMOVE)) {}
}
