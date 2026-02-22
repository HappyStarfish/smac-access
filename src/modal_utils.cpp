
#include "modal_utils.h"

void sr_run_modal_pump(bool* want_close) {
    MSG msg;
    while (!*want_close) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }
            TranslateMessage(&msg);
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
