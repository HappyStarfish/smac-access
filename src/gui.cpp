
#include "gui.h"
#include "gui_base.h"
#include "base_handler.h"
#include "social_handler.h"
#include "file_browser_handler.h"
#include "prefs_handler.h"
#include "specialist_handler.h"
#include "design_handler.h"
#include "diplo_handler.h"
#include "governor_handler.h"
#include "message_handler.h"
#include "multiplayer_handler.h"
#include "council_handler.h"
#include "status_handler.h"
#include "labs_handler.h"
#include "projects_handler.h"
#include "orbital_handler.h"
#include "military_handler.h"
#include "score_handler.h"
#include "monument_handler.h"
#include "datalinks_handler.h"
#include "faction_select_handler.h"
#include "game_settings_handler.h"
#include "netsetup_settings_handler.h"
#include "localization.h"
#include "world_map_handler.h"
#include "menu_handler.h"
#include "thinker_menu_handler.h"

const int32_t MainWinHandle = (int32_t)(&MapWin->oMainWin.oWinBase.field_4); // 0x939444

char label_pop_size[StrBufLen] = "Pop: %d / %d / %d / %d";
char label_pop_boom[StrBufLen] = "Population Boom";
char label_nerve_staple[StrBufLen] = "Nerve Staple: %d turns";
char label_captured_base[StrBufLen] = "Captured Base: %d turns";
char label_stockpile_energy[StrBufLen] = "Stockpile: %d per turn";
char label_sat_nutrient[StrBufLen] = "N +%d";
char label_sat_mineral[StrBufLen] = "M +%d";
char label_sat_energy[StrBufLen] = "E +%d";
char label_eco_damage[StrBufLen] = "Eco-Damage: %d%%";
char label_base_surplus[StrBufLen] = "Surplus: %d / %d / %d";
char label_unit_reactor[4][StrBufLen] = {};

std::string video_player_path = "";
std::string video_player_args = "";


struct ConsoleState {
    const int ScrollMin = 1;
    const int ScrollMax = 20;
    const int ListScrollDelta = 1;
    POINT ScreenSize;
    bool MouseOverTileInfo = true;
    bool Scrolling = false;
    bool RightButtonDown = false;
    bool ScrollDragging = false;
    double ScrollOffsetX = 0.0;
    double ScrollOffsetY = 0.0;
    POINT ScrollDragPos = {0, 0};
} CState;

/*
The following lists contain definitions copied directly from PRACX (e.g. functions with _F suffix).
These are mostly provided for reference and using them should be avoided because the names should be
converted to the actual names reversed from the SMACX binary (add F prefix for function prototypes).
*/

typedef int(__stdcall *START_F)(HINSTANCE, HINSTANCE, LPSTR, int);
typedef int(__thiscall *CCANVAS_CREATE_F)(Buffer* This);
typedef int(__stdcall *WNDPROC_F)(HWND, int, WPARAM, LPARAM);
typedef int(__thiscall *CMAIN_ZOOMPROCESSING_F)(Console* This);
typedef int(__stdcall *PROC_ZOOM_KEY_F)(int iZoomType, int iZero);
typedef int(__thiscall *CMAIN_TILETOPT_F)(Console* This, int iTileX, int iTileY, long* piX, long* piY);
typedef int(__thiscall *CMAIN_MOVEMAP_F)(Console* This, int iXPos, int iYPos, int a4);
typedef int(__thiscall *CMAIN_REDRAWMAP_F)(Console* This, int a2);
typedef int(__thiscall *CMAIN_DRAWMAP_F)(Console* This, int iOwner, int fUnitsOnly);
typedef int(__thiscall *CMAIN_PTTOTILE_F)(Console* This, POINT p, long* piTileX, long* piTileY);
typedef int(__thiscall *CINFOWIN_DRAWTILEINFO_F)(CInfoWin* This);
typedef int(__cdecl *PAINTHANDLER_F)(RECT *prRect, int a2);
typedef int(__cdecl *PAINTMAIN_F)(RECT *pRect);
typedef int(__thiscall *CSPRITE_FROMCANVASRECTTRANS_F)(Sprite* This, Buffer *poCanvas,
    int iTransparentIndex, int iLeft, int iTop, int iWidth, int iHeight, int a8);
typedef int(__thiscall *CCANVAS_DESTROY4_F)(Buffer* This);
typedef int (__thiscall *CSPRITE_STRETCHCOPYTOCANVAS_F)
    (Sprite* This, Buffer *poCanvasDest, int cTransparentIndex, int iLeft, int iTop);
typedef int(__thiscall *CSPRITE_STRETCHCOPYTOCANVAS1_F)
    (Sprite* This, Buffer *poCanvasDest, int cTransparentIndex, int iLeft, int iTop, int iDestScale, int iSourceScale);
typedef int (__thiscall *CSPRITE_STRETCHDRAWTOCANVAS2_F)
    (Sprite* This, Buffer *poCanvas, int a1, int a2, int a3, int a4, int a7, int a8);
typedef int(__thiscall *CWINBASE_ISVISIBLE_F)(Win* This);
typedef int(__thiscall *CTIMER_STARTTIMER_F)(Time* This, int a2, int a3, int iDelay, int iElapse, int uResolution);
typedef int(__thiscall *CTIMER_STOPTIMER_F)(Time* This);
typedef int(__thiscall *DRAWCITYMAP_F)(Win* This, int a2);
typedef int(__cdecl *GETFOODCOUNT_F)(int iForFaction, int a2, int iTileX, int iTileY, bool fWithFarm);
typedef int(__cdecl *GETPRODCOUNT_F)(int iForFaction, int a2, int iTileX, int iTileY, bool fWithMine);
typedef int(__cdecl *GETENERGYCOUNT_F)(int iForFaction, int a2, int iTileX, int iTileY, bool fWithSolar);
typedef int(__thiscall *IMAGEFROMCANVAS_F)(CImage* This, Buffer *poCanvasSource, int iLeft, int iTop, int iWidth, int iHeight, int a7);
typedef int(__cdecl *GETELEVATION_F)(int iTileX, int iTileY);
typedef int(__thiscall *CIMAGE_COPYTOCANVAS2_F)(CImage* This, Buffer *poCanvasDest, int x, int y, int a5, int a6, int a7);
typedef int(__thiscall *CMAINMENU_ADDSUBMENU_F)(Menu* This, int iMenuID, int iMenuItemID, char *lpString);
typedef int(__thiscall *CMAINMENU_ADDBASEMENU_F)(Menu* This, int iMenuItemID, const char *pszCaption, int a4);
typedef int(__thiscall *CMAINMENU_ADDSEPARATOR_F)(Menu* This, int iMenuItemID, int iSubMenuItemID);
typedef int(__thiscall *CMAINMENU_UPDATEVISIBLE_F)(Menu* This, int a2);
typedef int(__thiscall *CMAINMENU_RENAMEMENUITEM_F)(Menu* This, int iMenuItemID, int iSubMenuItemID, const char *pszCaption);
typedef long(__thiscall *CMAP_GETCORNERYOFFSET_F)(Console* This, int iTileX, int iTileY, int iCorner);

START_F                        pfncWinMain =                    (START_F                       )0x45F950;
HDC*                           phDC =                           (HDC*                          )0x9B7B2C;
HWND*                          phWnd =                          (HWND*                         )0x9B7B28;
HPALETTE*                      phPallete =                      (HPALETTE*                     )0x9B8178;
HINSTANCE*                     phInstance =                     (HINSTANCE*                    )0x9B7B14;
WNDPROC_F                      pfncWinProc =                    (WNDPROC_F                     )0x5F0650;
CMAIN_ZOOMPROCESSING_F         pfncZoomProcessing =             (CMAIN_ZOOMPROCESSING_F        )0x462980;
Console*                       pMain =                          (Console*                      )0x9156B0;
int*                           piMaxTileX =                     (int*                          )0x949870;
int*                           piMaxTileY =                     (int*                          )0x949874;
PROC_ZOOM_KEY_F                pfncProcZoomKey =                (PROC_ZOOM_KEY_F               )0x5150D0;
CMAIN_TILETOPT_F               pfncTileToPoint =                (CMAIN_TILETOPT_F              )0x462F00;
RECT*                          prVisibleTiles =                 (RECT*                         )0x7D3C28;
int*                           piMapFlags =                     (int*                          )0x94988C;
CMAIN_MOVEMAP_F                pfncMoveMap =                    (CMAIN_MOVEMAP_F               )0x46B1F0;
CMAIN_REDRAWMAP_F              pfncRedrawMap =                  (CMAIN_REDRAWMAP_F             )0x46A550;
CMAIN_DRAWMAP_F                pfncDrawMap =                    (CMAIN_DRAWMAP_F               )0x469CA0;
CMAIN_PTTOTILE_F               pfncPtToTile =                   (CMAIN_PTTOTILE_F              )0x463040;
CInfoWin*                      pInfoWin =                       (CInfoWin*                     )0x8C5568;
CINFOWIN_DRAWTILEINFO_F        pfncDrawTileInfo =               (CINFOWIN_DRAWTILEINFO_F       )0x4B8890; // Fixed
Console**                      ppMain =                         (Console**                     )0x7D3C3C;
PAINTHANDLER_F                 pfncPaintHandler =               (PAINTHANDLER_F                )0x5F7320;
PAINTMAIN_F                    pfncPaintMain =                  (PAINTMAIN_F                   )0x5EFD20;
CSPRITE_FROMCANVASRECTTRANS_F  pfncSpriteFromCanvasRectTrans =  (CSPRITE_FROMCANVASRECTTRANS_F )0x5E39A0;
Buffer*                        poLoadingCanvas =                (Buffer*                       )0x798668;
CCANVAS_DESTROY4_F             pfncCanvasDestroy4 =             (CCANVAS_DESTROY4_F            )0x5D7470;
CSPRITE_STRETCHCOPYTOCANVAS_F  pfncSpriteStretchCopyToCanvas =  (CSPRITE_STRETCHCOPYTOCANVAS_F )0x5E4B9A;
CSPRITE_STRETCHCOPYTOCANVAS1_F pfncSpriteStretchCopyToCanvas1 = (CSPRITE_STRETCHCOPYTOCANVAS1_F)0x5E4B4A;
CSPRITE_STRETCHDRAWTOCANVAS2_F pfncSpriteStretchDrawToCanvas2 = (CSPRITE_STRETCHDRAWTOCANVAS2_F)0x5E3E00;
Sprite*                        pSprResourceIcons =              (Sprite*                       )0x7A72A0;
Win*                           pCityWindow =                    (Win*                          )0x6A7628;
Win*                           pAnotherWindow =                 (Win*                          )0x8A6270;
Win*                           pAnotherWindow2 =                (Win*                          )0x8C6E68;
int*                           pfGameNotStarted =               (int*                          )0x68F21C;
CWINBASE_ISVISIBLE_F           pfncWinIsVisible =               (CWINBASE_ISVISIBLE_F          )0x5F7E90;
CTIMER_STARTTIMER_F            pfncStartTimer =                 (CTIMER_STARTTIMER_F           )0x616350;
CTIMER_STOPTIMER_F             pfncStopTimer =                  (CTIMER_STOPTIMER_F            )0x616780;
DRAWCITYMAP_F                  pfncDrawCityMap =                (DRAWCITYMAP_F                 )0x40F0F0;
int*                           piZoomNum =                      (int*                          )0x691E6C;
int*                           piZoomDenom =                    (int*                          )0x691E70;
int*                           piSourceScale =                  (int*                          )0x696D1C;
int*                           piDestScale =                    (int*                          )0x696D18;
int*                           piResourceExtra =                (int*                          )0x90E998;
int*                           piTilesPerRow =                  (int*                          )0x68FAF0;
CTile**                        paTiles =                        (CTile**                       )0x94A30C;
GETFOODCOUNT_F                 pfncGetFoodCount =               (GETFOODCOUNT_F                )0x4E6E50;
GETPRODCOUNT_F                 pfncGetProdCount =               (GETPRODCOUNT_F                )0x4E7310;
GETENERGYCOUNT_F               pfncGetEnergyCount =             (GETENERGYCOUNT_F              )0x4E7750;
IMAGEFROMCANVAS_F              pfncImageFromCanvas =            (IMAGEFROMCANVAS_F             )0x619710;
GETELEVATION_F                 pfncGetElevation =               (GETELEVATION_F                )0x5919C0;
CIMAGE_COPYTOCANVAS2_F         pfncImageCopytoCanvas2 =         (CIMAGE_COPYTOCANVAS2_F        )0x6233C0;
CMAINMENU_ADDSUBMENU_F         pfncMainMenuAddSubMenu =         (CMAINMENU_ADDSUBMENU_F        )0x5FB100;
CMAINMENU_ADDBASEMENU_F        pfncMainMenuAddBaseMenu =        (CMAINMENU_ADDBASEMENU_F       )0x5FAEF0;
CMAINMENU_ADDSEPARATOR_F       pfncMainMenuAddSeparator =       (CMAINMENU_ADDSEPARATOR_F      )0x5FB160;
CMAINMENU_UPDATEVISIBLE_F      pfncMainMenuUpdateVisible =      (CMAINMENU_UPDATEVISIBLE_F     )0x460DD0;
CMAINMENU_RENAMEMENUITEM_F     pfncMainMenuRenameMenuItem =     (CMAINMENU_RENAMEMENUITEM_F    )0x5FB700;
CMAP_GETCORNERYOFFSET_F        pfncMapGetCornerYOffset =        (CMAP_GETCORNERYOFFSET_F       )0x46FE70;

// End of PRACX definitions


bool shift_key_down() {
    return GetAsyncKeyState(VK_SHIFT) < 0;
}

bool ctrl_key_down() {
    return GetAsyncKeyState(VK_CONTROL) < 0;
}

bool alt_key_down() {
    return GetAsyncKeyState(VK_MENU) < 0;
}

bool win_has_focus() {
    return GetFocus() == *phWnd;
}

int __thiscall Win_is_visible(Win* This) {
    bool value = (This->iSomeFlag & WIN_VISIBLE)
        && (!This->poParent || Win_is_visible(This->poParent));
    return value;
}

/*
Returns GW_World only when the world map is visible and has focus
and other large modal windows are not blocking it.
Other modal windows with the exception of BaseWin are already
covered by checking Win_get_key_window condition.
*/
GameWinState current_window() {
    if (!*GameHalted) {
        int state = Win_get_key_window();
        if (state == MainWinHandle) {
            return Win_is_visible(BaseWin) ? GW_Base : GW_World;
        } else if (state == (int)DesignWin) {
            return GW_Design;
        }
    }
    return GW_None;
}

void mouse_over_tile(POINT* p) {
    static POINT ptLastTile = {0, 0};
    POINT ptTile;

    if (CState.MouseOverTileInfo
    && !MapWin->fUnitNotViewMode
    && p->x >= 0 && p->x < CState.ScreenSize.x
    && p->y >= 0 && p->y < (CState.ScreenSize.y - ConsoleHeight)
    && MapWin_pixel_to_tile(MapWin, p->x, p->y, &ptTile.x, &ptTile.y) == 0
    && memcmp(&ptTile, &ptLastTile, sizeof(POINT)) != 0) {

        pInfoWin->iTileX = ptTile.x;
        pInfoWin->iTileY = ptTile.y;
        StatusWin_on_redraw((Win*)pInfoWin);
        memcpy(&ptLastTile, &ptTile, sizeof(POINT));
    }
}

ULONGLONG get_ms_count() {
    static LONGLONG llFrequency = 0;
    static ULONGLONG ullLast = 0;
    static ULONGLONG ullHigh = 0;
    ULONGLONG ullRet;
    if (llFrequency == 0 && !QueryPerformanceFrequency((LARGE_INTEGER*)&llFrequency)) {
        llFrequency = -1LL;
    }
    if (llFrequency > 0) {
        QueryPerformanceCounter((LARGE_INTEGER*)&ullRet);
        ullRet *= 1000;
        ullRet /= (ULONGLONG)llFrequency;
    } else if (llFrequency < 0) {
        ullRet = GetTickCount();
        if (ullRet < ullLast) {
            ullHigh += 0x100000000ULL;
        }
        ullLast = ullRet;
        ullRet += ullHigh;
    }
    return ullRet;
}

bool do_scroll(double x, double y) {
    bool fScrolled = false;
    int mx = *MapAreaX;
    int my = *MapAreaY;
    int i;
    int d;
    if (x && MapWin->iMapTilesEvenX + MapWin->iMapTilesOddX < mx) {
        if (x < 0 && (!map_is_flat() || MapWin->iMapTileLeft > 0)) {
            i = (int)CState.ScrollOffsetX;
            CState.ScrollOffsetX -= x;
            fScrolled = fScrolled || (i != (int)CState.ScrollOffsetX);
            while (CState.ScrollOffsetX >= MapWin->iPixelsPerTileX) {
                CState.ScrollOffsetX -= MapWin->iPixelsPerTileX;
                MapWin->iTileX -= 2;
                if (MapWin->iTileX < 0) {
                    if (map_is_flat()) {
                        MapWin->iTileX = 0;
                        MapWin->iTileY &= ~1;
                        CState.ScrollOffsetX = 0;
                    } else {
                        MapWin->iTileX += mx;
                    }
                }
            }
        } else if (x < 0 && map_is_flat()) {
            fScrolled = true;
            CState.ScrollOffsetX = 0;
        }
        if (x > 0 &&
                (!map_is_flat() ||
                 MapWin->iMapTileLeft +
                 MapWin->iMapTilesEvenX +
                 MapWin->iMapTilesOddX <= mx)) {
            i = (int)CState.ScrollOffsetX;
            CState.ScrollOffsetX -= x;
            fScrolled = fScrolled || (i != (int)CState.ScrollOffsetX);
            while (CState.ScrollOffsetX <= -MapWin->iPixelsPerTileX) {
                CState.ScrollOffsetX += MapWin->iPixelsPerTileX;
                MapWin->iTileX += 2;
                if (MapWin->iTileX > mx) {
                    if (map_is_flat()) {
                        MapWin->iTileX = mx;
                        MapWin->iTileY &= ~1;
                        CState.ScrollOffsetX = 0;
                    } else {
                        MapWin->iTileX -= mx;
                    }
                }
            }
        } else if (x > 0 && map_is_flat()) {
            fScrolled = true;
            CState.ScrollOffsetX = 0;
        }
    }
    if (y && MapWin->iMapTilesEvenY + MapWin->iMapTilesOddY < my) {
        int iMinTileY = MapWin->iMapTilesOddY - 2;
        int iMaxTileY = my + 4 - MapWin->iMapTilesOddY;
        while (MapWin->iTileY < iMinTileY) {
            MapWin->iTileY += 2;
        }
        while (MapWin->iTileY > iMaxTileY) {
            MapWin->iTileY -= 2;
        }
        d = (MapWin->iTileY - iMinTileY) * MapWin->iPixelsPerHalfTileY - (int)CState.ScrollOffsetY;
        if (y < 0 && d > 0 ) {
            if (y < -d)
                y = -d;
            i = (int)CState.ScrollOffsetY;
            CState.ScrollOffsetY -= y;
            fScrolled = fScrolled || (i != (int)CState.ScrollOffsetY);
            while (CState.ScrollOffsetY >= MapWin->iPixelsPerTileY && MapWin->iTileY - 2 >= iMinTileY) {
                CState.ScrollOffsetY -= MapWin->iPixelsPerTileY;
                MapWin->iTileY -= 2;
            }
        }
        d = (iMaxTileY - MapWin->iTileY + 1) * MapWin->iPixelsPerHalfTileY + (int)CState.ScrollOffsetY;
        if (y > 0 && d > 0) {
            if (y > d)
                y = d;
            i = (int)CState.ScrollOffsetY;
            CState.ScrollOffsetY -= y;
            fScrolled = fScrolled || (i != (int)CState.ScrollOffsetY);
            while (CState.ScrollOffsetY <= -MapWin->iPixelsPerTileY && MapWin->iTileY + 2 <= iMaxTileY) {
                CState.ScrollOffsetY += MapWin->iPixelsPerTileY;
                MapWin->iTileY += 2;
            }
        }
    }
    if (fScrolled) {
        MapWin_draw_map(MapWin, 0);
        Win_update_screen(NULL, 0);
        Win_flip(NULL);
        ValidateRect(*phWnd, NULL);
    }
    return fScrolled;
}

void check_scroll() {
    POINT p;
    if (CState.Scrolling || (!GetCursorPos(&p) && !(CState.ScrollDragging && CState.RightButtonDown))) {
        return;
    }
    CState.Scrolling = true;
    int w = CState.ScreenSize.x;
    int h = CState.ScreenSize.y;
    static ULONGLONG ullDeactiveTimer = 0;
    ULONGLONG ullOldTickCount = 0;
    ULONGLONG ullNewTickCount = 0;
    BOOL fScrolled;
    BOOL fScrolledAtAll = false;
    BOOL fLeftButtonDown = (GetAsyncKeyState(VK_LBUTTON) < 0);
    int iScrollArea = conf.scroll_area * CState.ScreenSize.x / 1024;

    if (CState.RightButtonDown && GetAsyncKeyState(VK_RBUTTON) < 0) {
        if (labs((long)hypot((double)(p.x-CState.ScrollDragPos.x), (double)(p.y-CState.ScrollDragPos.y))) > 2.5) {
            CState.ScrollDragging = true;
            SetCursor(LoadCursor(0, IDC_HAND));
        }
    }
    CState.ScrollOffsetX = MapWin->iMapPixelLeft;
    CState.ScrollOffsetY = MapWin->iMapPixelTop;
    ullNewTickCount = get_ms_count();
    ullOldTickCount = ullNewTickCount;
//    debug("scroll_check %d %d %d\n", CState.Scrolling, (int)CState.ScrollDragPos.x, (int)CState.ScrollDragPos.y);
    do {
        double dTPS = -1;
        double dx = 0;
        double dy = 0;
        fScrolled = false;
        if (CState.ScrollDragging && CState.RightButtonDown) {
            fScrolled = true;
            dx = CState.ScrollDragPos.x - p.x;
            dy = CState.ScrollDragPos.y - p.y;
            memcpy(&CState.ScrollDragPos, &p, sizeof(POINT));

        } else if (ullNewTickCount - ullDeactiveTimer > 100 && !CState.ScrollDragging) {
            double dMin = (double)CState.ScrollMin;
            double dMax = (double)CState.ScrollMax;
            double dArea = (double)iScrollArea;
            if (p.x <= iScrollArea && p.x >= 0) {
                fScrolled = true;
                dTPS = dMin + (dArea - (double)p.x) / dArea * (dMax - dMin);
                dx = (double)(ullNewTickCount - ullOldTickCount) * dTPS * (double)MapWin->iPixelsPerTileX / -1000.0;

            } else if ((w - p.x) <= iScrollArea && w >= p.x) {
                fScrolled = true;
                dTPS = dMin + (dArea - (double)(w - p.x)) / dArea * (dMax - dMin);
                dx = (double)(ullNewTickCount - ullOldTickCount) * dTPS * (double)MapWin->iPixelsPerTileX / 1000.0;
            }
            if (p.y <= iScrollArea && p.y >= 0) {
                fScrolled = true;
                dTPS = dMin + (dArea - (double)p.y) / dArea * (dMax - dMin);
                dy = (double)(ullNewTickCount - ullOldTickCount) * dTPS * (double)MapWin->iPixelsPerTileY / -1000.0;

            } else if (h - p.y <= iScrollArea && h >= p.y &&
            // These extra conditions will stop movement when the mouse is over the bottom middle console.
            (p.x <= (CState.ScreenSize.x - ConsoleWidth) / 2 ||
             p.x >= (CState.ScreenSize.x - ConsoleWidth) / 2 + ConsoleWidth ||
             h - p.y <= 8 * CState.ScreenSize.y / 768)) {
                fScrolled = true;
                dTPS = dMin + (dArea - (double)(h - p.y)) / dArea * (dMax - dMin);
                dy = (double)(ullNewTickCount - ullOldTickCount) * dTPS * (double)MapWin->iPixelsPerTileY / 1000.0;
            }
        }
        if (fScrolled) {
            ullOldTickCount = ullNewTickCount;
            if (do_scroll(dx, dy)) {
                fScrolledAtAll = true;
            } else {
                Sleep(0);
            }
            if (DEBUG) {
                Sleep(5);
            }
            ullNewTickCount = get_ms_count();
            if (CState.RightButtonDown) {
                CState.RightButtonDown = (GetAsyncKeyState(VK_RBUTTON) < 0);
            }
            debug("scroll_move  x=%d y=%d scx=%.4f scy=%.4f dx=%.4f dy=%.4f dTPS=%.4f\n",
                (int)p.x, (int)p.y, CState.ScrollOffsetX, CState.ScrollOffsetY, dx, dy, dTPS);
        }

    } while (fScrolled && (GetCursorPos(&p) || (CState.ScrollDragging && CState.RightButtonDown)));

    if (fScrolledAtAll) {
        MapWin->drawOnlyCursor = 1;
        MapWin_set_center(MapWin, MapWin->iTileX, MapWin->iTileY, 1);
        MapWin->drawOnlyCursor = 0;
        for (int i = 1; i < 8; i++) {
            if (ppMain[i] && ppMain[i]->iDrawToggleA &&
            (!fLeftButtonDown || ppMain[i]->field_1DD80) &&
            ppMain[i]->iMapTilesOddX + ppMain[i]->iMapTilesEvenX < *MapAreaX) {
                MapWin_set_center(ppMain[i], MapWin->iTileX, MapWin->iTileY, 1);
            }
        }
        if (CState.ScrollDragging) {
            ullDeactiveTimer = ullNewTickCount;
        }
    }
    mouse_over_tile(&p);
    CState.Scrolling = false;
    flushlog();
}

int __thiscall mod_gen_map(Console* This, int iOwner, int fUnitsOnly) {

    if (This == MapWin) {
        // Save these values to restore them later
        int iMapPixelLeft = This->iMapPixelLeft;
        int iMapPixelTop = This->iMapPixelTop;
        int iMapTileLeft = This->iMapTileLeft;
        int iMapTileTop = This->iMapTileTop;
        int iMapTilesOddX = This->iMapTilesOddX;
        int iMapTilesOddY = This->iMapTilesOddY;
        int iMapTilesEvenX = This->iMapTilesEvenX;
        int iMapTilesEvenY = This->iMapTilesEvenY;
        // These are just aliased to save typing and are not modified
        int mx = *MapAreaX;
        int my = *MapAreaY;

        if (iMapTilesOddX + iMapTilesEvenX < mx && !map_is_flat()) {
            if (iMapPixelLeft > 0) {
                This->iMapPixelLeft -= This->iPixelsPerTileX;
                This->iMapTileLeft -= 2;
                This->iMapTilesEvenX++;
                This->iMapTilesOddX++;
                if (This->iMapTileLeft < 0)
                    This->iMapTileLeft += mx;
            } else if (iMapPixelLeft < 0 ) {
                This->iMapTilesEvenX++;
                This->iMapTilesOddX++;
            }
        }
        if (iMapTilesOddY + iMapTilesEvenY < my) {
            if (iMapPixelTop > 0) {
                This->iMapPixelTop -= This->iPixelsPerTileY;
                This->iMapTileTop -= 2;
                This->iMapTilesEvenY++;
                This->iMapTilesOddY++;
            } else if (iMapPixelTop < 0) {
                This->iMapTilesEvenY++;
                This->iMapTilesOddY++;
            }
        }
        MapWin_gen_map(This, iOwner, fUnitsOnly);
        // Restore This's original values
        This->iMapPixelLeft = iMapPixelLeft;
        This->iMapPixelTop = iMapPixelTop;
        This->iMapTileLeft = iMapTileLeft;
        This->iMapTileTop = iMapTileTop;
        This->iMapTilesOddX = iMapTilesOddX;
        This->iMapTilesOddY = iMapTilesOddY;
        This->iMapTilesEvenX = iMapTilesEvenX;
        This->iMapTilesEvenY = iMapTilesEvenY;
    } else {
        MapWin_gen_map(This, iOwner, fUnitsOnly);
    }
    return 0;
}

int __thiscall mod_calc_dim(Console* This) {
    static POINT ptOldTile = {-1, -1};
    POINT ptOldCenter;
    POINT ptNewCenter;
    POINT ptNewTile;
    POINT ptScale;
    int iOldZoom;
    int dx, dy;
    bool fx, fy;
//    int w = ((GraphicWin*)((int)This + This->vtbl[1]))->oCanvas.stBitMapInfo.bmiHeader.biWidth;
//    int h = -((GraphicWin*)((int)This + This->vtbl[1]))->oCanvas.stBitMapInfo.bmiHeader.biHeight;

    if (This == MapWin) {
        iOldZoom = This->iLastZoomFactor;
        ptNewTile.x = This->iTileX;
        ptNewTile.y = This->iTileY;
        fx = (ptNewTile.x == ptOldTile.x);
        fy = (ptNewTile.y == ptOldTile.y);
        memcpy(&ptOldTile, &ptNewTile, sizeof(POINT));
        MapWin_tile_to_pixel(This, ptNewTile.x, ptNewTile.y, &ptOldCenter.x, &ptOldCenter.y);
        MapWin_calculate_dimensions(This);

        if (CState.Scrolling) {
            This->iMapPixelLeft = (int)CState.ScrollOffsetX;
            This->iMapPixelTop = (int)CState.ScrollOffsetY;
        } else if (iOldZoom != -9999) {
            ptScale.x = This->iPixelsPerTileX;
            ptScale.y = This->iPixelsPerTileY;
            MapWin_tile_to_pixel(This, ptNewTile.x, ptNewTile.y, &ptNewCenter.x, &ptNewCenter.y);
            dx = ptOldCenter.x - ptNewCenter.x;
            dy = ptOldCenter.y - ptNewCenter.y;
            if (!This->iMapPixelLeft && fx && dx > -ptScale.x * 2 && dx < ptScale.x * 2) {
                This->iMapPixelLeft = dx;
            }
            if (!This->iMapPixelTop && fy && dy > -ptScale.y * 2 && dy < ptScale.y * 2
            && (dy + *ScreenHeight) / ptScale.y < (*MapAreaY - This->iMapTileTop) / 2) {
                This->iMapPixelTop = dy;
            }
        }
    } else {
        MapWin_calculate_dimensions(This);
    }
    return 0;
}

int __cdecl mod_blink_timer() {
    if (!*GameHalted && !VehBattleState[1]) {
        if (Win_is_visible(BaseWin)) {
            return TutWin_draw_arrow(TutWin);
        }
        if (Win_is_visible(SocialWin)) {
            return TutWin_draw_arrow(TutWin);
        }
        PlanWin_blink(PlanWin);
        StringBox_clip_ids(StringBox, 150);

        if ((!MapWin->field_23BE8 && (!*MultiplayerActive || !(*GameState & STATE_UNK_2))) || *ControlTurnA) {
            MapWin->field_23BFC = 0;
            return 0;
        }
        if (MapWin->field_23BE4 || (*MultiplayerActive && *GameState & STATE_UNK_2)) {
            MapWin->field_23BF8 = !MapWin->field_23BF8;
            draw_cursor();
            bool mouse_btn_down = GetAsyncKeyState(VK_LBUTTON) < 0;

            if ((int)*phInstance == GetWindowLongA(GetFocus(), GWL_HINSTANCE) && !Win_is_visible(TutWin)) {
                if (MapWin->field_23D80 != -1
                && MapWin->field_23D84 != -1
                && MapWin->field_23D88 != -1
                && MapWin->field_23D8C != -1) {
                    if (*GamePreferences & PREF_BSC_MOUSE_EDGE_SCROLL_VIEW || mouse_btn_down || *WinModalState != 0) {
                        CState.ScreenSize.x = *ScreenWidth;
                        CState.ScreenSize.y = *ScreenHeight;
                        check_scroll();
                    }
                }
            }
        }
    }
    return 0;
}

static bool sr_is_hud_noise(const char* text, bool in_menu_early);

// Build an announcement string from a list of items, filtering HUD noise
// and already-announced items. Returns true if new items were found.
// max_new: max number of new items to include (0 = unlimited).
typedef const char* (*ItemGetFunc)(int index);
static bool sr_build_announce(ItemGetFunc get_item, int count,
    const char* announced, char* buf, int bufsz,
    char* all, int allsz, bool in_menu_early, int max_new)
{
    int pos = 0, apos = 0;
    bool has_new = false;
    int spoken = 0;
    for (int i = 0; i < count; i++) {
        const char* it = get_item(i);
        if (!it || strlen(it) < 3) continue;
        if (sr_is_hud_noise(it, in_menu_early)) continue;
        int len = strlen(it);
        if (apos + len + 3 < allsz) {
            if (apos > 0) { all[apos++] = '.'; all[apos++] = ' '; }
            memcpy(all + apos, it, len);
            apos += len;
        }
        if (strstr(announced, it) == NULL && (max_new <= 0 || spoken < max_new)) {
            has_new = true;
            spoken++;
            if (pos + len + 3 < bufsz) {
                if (pos > 0) { buf[pos++] = '.'; buf[pos++] = ' '; }
                memcpy(buf + pos, it, len);
                pos += len;
            }
        }
    }
    buf[pos] = '\0';
    all[apos] = '\0';
    return has_new;
}

// HUD bar noise filter: status bar items that should not be auto-announced.
// These are drawn character-by-character and pollute all triggers.
static bool sr_is_hud_noise(const char* text, bool in_menu_early) {
    if (!text) return true;
    // Short "Mission Year" / "Missionsjahr" = HUD noise. Long = game status → let through.
    if (strncmp(text, "Mission Year", 12) == 0 && strlen(text) < 30) return true;
    if (strncmp(text, "Missionsjahr", 12) == 0 && strlen(text) < 30) return true;
    if (strncmp(text, "Econ:", 5) == 0) return true;
    if (strncmp(text, "Psych:", 6) == 0) return true;
    if (strncmp(text, "Labs:", 5) == 0) return true;
    // German HUD labels (from German language patch)
    if (strncmp(text, "Wirt:", 5) == 0) return true;  // Wirtschaft (Econ)
    if (strncmp(text, "Psyc:", 5) == 0) return true;  // Psyche
    if (strncmp(text, "Fors:", 5) == 0) return true;  // Forschung (Labs)
    // Info panel items (redundant with MAP-MOVE tracker)
    if (strncmp(text, "Energy:", 7) == 0) return true;
    if (strncmp(text, "Energie:", 8) == 0) return true;
    if (strncmp(text, "Unexplored", 10) == 0) return true;
    if (strncmp(text, "Unerforscht", 11) == 0) return true;
    if (text[0] == '(' && strstr(text, " , ") != NULL) return true;
    if (strncmp(text, "Elev:", 5) == 0) return true;
    if (strncmp(text, "Altitude:", 9) == 0) return true;
    if (strcmp(text, "ENDANGERED") == 0) return true;
    if (strncmp(text, "(Gov:", 5) == 0) return true;
    if (strncmp(text, "(Reg:", 5) == 0) return true;  // Regierung (Gov)
    if (strncmp(text, "(Gouvern.:", 10) == 0) return true;  // Governor status in German HUD
    // Terrain descriptors (handled by MAP-MOVE) — English and German
    if (strstr(text, "Rolling") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Flat") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Rocky") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Rainy") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Arid") != NULL && strlen(text) < 15) return true;
    if (strstr(text, "Moist") != NULL && strlen(text) < 15) return true;
    if (strstr(text, "Xenofungus") != NULL) return true;
    // German terrain descriptors (after ANSI→UTF-8 conversion)
    if (strstr(text, "Felsig") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Flach") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Trocken") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Feucht") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Regnerisch") != NULL && strlen(text) < 20) return true;
    if (strstr(text, "Xenopilz") != NULL) return true;
    // Note: "Hügelig" is UTF-8 after conversion, match the UTF-8 bytes
    if (strstr(text, "H\xc3\xbc""gelig") != NULL && strlen(text) < 20) return true;
    // Economic panel (world map overlay, drawn char-by-char)
    if (strncmp(text, "Commerce", 8) == 0) return true;
    if (strncmp(text, "Gross", 5) == 0) return true;
    if (strncmp(text, "Total Cost", 10) == 0) return true;
    if (strncmp(text, "NET ", 4) == 0) return true;
    // German economic panel
    if (strncmp(text, "Handel", 6) == 0) return true;
    if (strncmp(text, "Brutto", 6) == 0) return true;
    if (strncmp(text, "Gesamtkosten", 12) == 0) return true;
    if (strncmp(text, "NETTO", 5) == 0) return true;
    // German base screen labels
    if (strcmp(text, "GOUVERNEUR") == 0) return true;
    if (strcmp(text, "RESSOURCEN") == 0) return true;
    if (strcmp(text, "ENERGIE VERTEILUNG") == 0) return true;
    if (strncmp(text, "RUNDEN:", 7) == 0 && strlen(text) < 15) return true;
    // German base screen energy panel fragments
    if (strcmp(text, "WIRTSCHAFT") == 0) return true;
    if (strncmp(text, "ENERGIE  +", 10) == 0) return true;
    if (strncmp(text, "BONUS =", 7) == 0) return true;
    // German social engineering status values in HUD (short strings)
    if (strcmp(text, "Daten DeZentral") == 0) return true;
    if (strcmp(text, "Gruppendominanz") == 0) return true;
    // Partial HUD build-up (character-by-character: "Mis", "Miss", etc.)
    if (strncmp(text, "Mis", 3) == 0 && strlen(text) < 12) return true;
    if (strncmp(text, "Eco", 3) == 0 && strlen(text) < 5) return true;
    if (strncmp(text, "Psy", 3) == 0 && strlen(text) < 6) return true;
    if (strncmp(text, "Lab", 3) == 0 && strlen(text) < 5) return true;
    if (strncmp(text, "Wir", 3) == 0 && strlen(text) < 5) return true;  // Wirtschaft
    if (strncmp(text, "For", 3) == 0 && strlen(text) < 5) return true;  // Forschung
    // Menu bar items (top of world map screen)
    if (!in_menu_early) {
        if (strcmp(text, "GAME") == 0) return true;
        if (strcmp(text, "NETWORK") == 0) return true;
        if (strcmp(text, "ACTION") == 0) return true;
        if (strcmp(text, "TERRAFORM") == 0) return true;
        if (strcmp(text, "SCENARIO") == 0) return true;
        if (strcmp(text, "EDIT MAP") == 0) return true;
        if (strcmp(text, "HELP") == 0) return true;
        // German menu bar items
        if (strcmp(text, "SPIEL") == 0) return true;
        if (strcmp(text, "NETZWERK") == 0) return true;
        if (strcmp(text, "AKTION") == 0) return true;
        if (strcmp(text, "TERRAFORMING") == 0) return true;
        if (strcmp(text, "SZENARIO") == 0) return true;
        if (strcmp(text, "KARTE BEARB.") == 0) return true;
        if (strcmp(text, "HILFE") == 0) return true;
    }
    return false;
}

// Saved popup object pointer for editbox field switching
static void* sr_editbox_popup = NULL;

/// Dispatch keyboard input to whichever analysis screen handler is active.
/// Returns true if a handler consumed the key.
static bool dispatch_analysis_handler(UINT msg, WPARAM wParam) {
    struct AnalysisHandler {
        bool (*isActive)();
        bool (*update)(UINT, WPARAM);
    };
    static const AnalysisHandler handlers[] = {
        {LabsHandler::IsActive,      LabsHandler::Update},
        {ProjectsHandler::IsActive,  ProjectsHandler::Update},
        {OrbitalHandler::IsActive,   OrbitalHandler::Update},
        {MilitaryHandler::IsActive,  MilitaryHandler::Update},
        {ScoreHandler::IsActive,     ScoreHandler::Update},
        {MonumentHandler::IsActive,  MonumentHandler::Update},
        {DatalinksHandler::IsActive, DatalinksHandler::Update},
    };
    for (const auto& h : handlers) {
        if (h.isActive()) {
            if (msg == WM_KEYDOWN) {
                h.update(msg, wParam);
            }
            return true; // consume WM_KEYDOWN and WM_CHAR
        }
    }
    return false;
}

/// Handle F-key modal shortcuts on world map and main menu.
/// Returns true if the key was consumed.
static bool handle_fkey_modals(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN || !sr_is_available()) return false;
    bool ctrl = ctrl_key_down();
    bool shift = shift_key_down();

    // World map F-key modals (F1-F9, no modifiers)
    if (!ctrl && !shift && current_window() == GW_World && !*GameHalted) {
        switch (wParam) {
            case VK_F1: DatalinksHandler::RunModal(); return true;
            case VK_F2: LabsHandler::RunModal(); return true;
            case VK_F3: SocialEngHandler::RunModalAlloc(); return true;
            case VK_F4: StatusHandler::RunModal(); return true;
            case VK_F5: ProjectsHandler::RunModal(); return true;
            case VK_F6: OrbitalHandler::RunModal(); return true;
            case VK_F7: MilitaryHandler::RunModal(); return true;
            case VK_F8: ScoreHandler::RunModal(); return true;
            case VK_F9: MonumentHandler::RunModal(); return true;
            default: break;
        }
    }
    // Ctrl+F10 = game settings (main menu only)
    if (wParam == VK_F10 && ctrl && !shift
        && current_window() == GW_None && *PopupDialogState == 0) {
        GameSettingsHandler::RunModal();
        return true;
    }
    // Ctrl+Shift+F10 = multiplayer lobby settings
    if (wParam == VK_F10 && ctrl && shift && MultiplayerHandler::IsActive()) {
        NetSetupSettingsHandler::RunModal();
        return true;
    }
    return false;
}

/// Handle screen reader utility hotkeys (silence, history, toggles).
/// Returns true if the key was consumed.
static bool handle_sr_utility_keys(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN || !sr_is_available()) return false;
    bool ctrl = ctrl_key_down();
    bool shift = shift_key_down();

    // Ctrl+Shift+R = silence speech
    if (wParam == 'R' && ctrl && shift) {
        sr_debug_log("CTRL+SHIFT+R: silence");
        sr_silence();
        return true;
    }
    // F12 = accessible commlink dialog
    if (wParam == VK_F12 && !ctrl && !shift
        && current_window() == GW_World && !*GameHalted) {
        DiplomacyHandler::RunCommlink();
        return true;
    }
    // Ctrl+F12 = toggle debug logging
    if (wParam == VK_F12 && ctrl && !shift) {
        sr_debug_toggle();
        return true;
    }
    // Ctrl+Shift+F11/F12 = speech history browser
    if ((wParam == VK_F11 || wParam == VK_F12) && ctrl && shift) {
        static int hist_pos = 0;
        if (wParam == VK_F12) {
            hist_pos = 0;
        } else {
            hist_pos++;
        }
        int total = sr_history_count();
        sr_history_set_browsing(true);
        if (total == 0) {
            sr_output(loc(SR_HISTORY_EMPTY), true);
        } else if (hist_pos >= total) {
            hist_pos = total - 1;
            sr_output(loc(SR_HISTORY_OLDEST), true);
        } else {
            const char* entry = sr_history_get(hist_pos);
            if (entry) {
                char buf[600];
                snprintf(buf, sizeof(buf), loc(SR_HISTORY_FMT),
                    hist_pos + 1, total, entry);
                sr_output(buf, true);
            }
        }
        sr_history_set_browsing(false);
        return true;
    }
    // Ctrl+Shift+A = toggle all accessibility features
    if (wParam == 'A' && ctrl && shift) {
        sr_output(loc(SR_ACCESSIBILITY_OFF), true);
        sr_set_disabled(true);
        debug("SR: accessibility disabled via Ctrl+Shift+A\n");
        return true;
    }
    // Ctrl+Shift+T = toggle Thinker AI
    if (wParam == 'T' && ctrl && shift
        && current_window() == GW_World && !*GameHalted) {
        static int original_factions_enabled = -1;
        if (original_factions_enabled < 0) {
            original_factions_enabled = conf.factions_enabled;
        }
        if (conf.factions_enabled > 0) {
            conf.factions_enabled = 0;
            sr_output(loc(SR_THINKER_AI_OFF), true);
            debug("SR: Thinker AI disabled (was %d)\n", original_factions_enabled);
        } else {
            conf.factions_enabled = original_factions_enabled;
            sr_output(loc(SR_THINKER_AI_ON), true);
            debug("SR: Thinker AI enabled (%d)\n", conf.factions_enabled);
        }
        return true;
    }
    return false;
}

LRESULT WINAPI ModWinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    const bool debug_cmd = DEBUG && !*GameHalted && msg == WM_CHAR;
    const bool is_editor = !*GameHalted
        && *GameState & STATE_SCENARIO_EDITOR && *GameState & STATE_OMNISCIENT_VIEW;
    static int delta_accum = 0;
    static bool sr_initialized = false;
    static bool was_editor = false;
    POINT p;
    MAP* sq;

    // Detect scenario editor activation
    if (is_editor && !was_editor && sr_is_available()) {
        sr_output(loc(SR_EDITOR_NOT_ACCESSIBLE), true);
    }
    was_editor = is_editor;

    // Deferred screen reader init (COM not safe in DllMain)
    if (!sr_initialized) {
        sr_initialized = true;
        if (sr_all_disabled()) {
            // SR fully disabled (no_hooks.txt) — skip Tolk loading entirely
        } else if (sr_init()) {
            loc_init();
            sr_output(loc(SR_MOD_LOADED), true);
            sr_debug_log("SR init OK, sr_is_available=%d", sr_is_available());
        } else {
            sr_debug_log("SR init FAILED");
        }
    }

    // === Runtime accessibility toggle (Ctrl+Shift+A) ===
    // When disabled, only the re-enable hotkey is checked — all other
    // accessibility code is skipped and keys pass through to the game.
    if (sr_get_disabled()) {
        if (msg == WM_KEYDOWN && wParam == 'A' && ctrl_key_down() && shift_key_down()) {
            sr_set_disabled(false);
            sr_output(loc(SR_ACCESSIBILITY_ON), true);
            debug("SR: accessibility re-enabled via Ctrl+Shift+A\n");
            return 0;
        }
        return WinProc(hwnd, msg, wParam, lParam);
    }

    // === Screen reader: unified announce system ===

    // Early in_menu check for HUD filter (full in_menu computed later too)
    bool in_netsetup = (*GameHalted && NetWin && Win_is_visible(NetWin));
    bool in_menu_early = (current_window() == GW_None
        && *PopupDialogState == 0);

    // (HUD noise filter is sr_is_hud_noise() defined above ModWinProc)

    //
    static char sr_announced[2048] = "";
    static DWORD sr_last_seen_record = 0;
    static DWORD sr_stable_since = 0;
    static bool sr_arrow_active = false;
    static WPARAM sr_arrow_dir = 0;  // VK_UP or VK_DOWN
    static DWORD sr_arrow_time = 0;  // tick when arrow was pressed

    if (sr_is_available()) {
        DWORD now = GetTickCount();
        GameWinState cur_win = current_window();
        int cur_popup = *PopupDialogState;

        // --- Screen state change tracking ---
        static GameWinState sr_prev_win = GW_None;
        static int sr_prev_popup = -1;
        if (cur_win != sr_prev_win || cur_popup != sr_prev_popup) {
            const char* wname = "None";
            if (cur_win == GW_World) wname = "World";
            else if (cur_win == GW_Base) wname = "Base";
            else if (cur_win == GW_Design) wname = "Design";
            sr_debug_log("SCREEN %s popup=%d", wname, cur_popup);

            // Announce major screen transitions (spoken)
            if (cur_win != sr_prev_win) {
                if (sr_prev_win == GW_Base) {
                    BaseScreenHandler::OnClose();
                }
                if (cur_win == GW_Base) {
                    BaseScreenHandler::OnOpen();
                } else if (cur_win == GW_Design) {
                    sr_debug_log("ANNOUNCE-SCREEN: Design Workshop");
                    sr_output(loc(SR_DESIGN_WORKSHOP), true);
                }
                // Clear settled text for new screen context
                sr_announced[0] = '\0';
            }

            sr_prev_win = cur_win;
            sr_prev_popup = cur_popup;
        }

        // --- Game-end detection: announce victory type and score screen ---
        static bool sr_prev_game_done = false;
        bool cur_game_done = (*GameState & STATE_GAME_DONE) != 0;
        if (cur_game_done && !sr_prev_game_done) {
            const char* victory_msg;
            if (!is_alive(*CurrentPlayerFaction)) {
                victory_msg = loc(SR_DEFEAT);
            } else if (*GameState & STATE_VICTORY_CONQUER) {
                victory_msg = loc(SR_VICTORY_CONQUEST);
            } else if (*GameState & STATE_VICTORY_DIPLOMATIC) {
                victory_msg = loc(SR_VICTORY_DIPLOMATIC);
            } else if (*GameState & STATE_VICTORY_ECONOMIC) {
                victory_msg = loc(SR_VICTORY_ECONOMIC);
            } else if (voice_of_planet()) {
                victory_msg = loc(SR_VICTORY_TRANSCENDENCE);
            } else {
                victory_msg = loc(SR_GAME_OVER);
            }
            sr_debug_log("GAME-END: %s (GameState=0x%X)", victory_msg, *GameState);
            sr_output(victory_msg, false);
        }
        sr_prev_game_done = cur_game_done;

        // --- Multiplayer Setup (NetWin) transition detection ---
        static bool sr_netsetup_was_active = false;
        if (in_netsetup && !sr_netsetup_was_active) {
            MultiplayerHandler::OnOpen();
            sr_announced[0] = '\0';
            sr_debug_log("ANNOUNCE-SCREEN: Multiplayer Setup");
            // Enable coordinate logging in Buffer_write hooks
            sr_netsetup_log_coords = true;
            // Detailed NetWin structure dump for reverse engineering
            if (NetWin) {
                // Vtable pointer identifies actual class type
                int* raw = (int*)NetWin;
                sr_debug_log("NETWIN vtable=%p addr=%p childCount=%d caption=[%s]",
                    (void*)raw[0], (void*)NetWin, NetWin->iChildCount,
                    NetWin->pszCaption ? NetWin->pszCaption : "(null)");
                sr_debug_log("NETWIN rect=(%d,%d,%d,%d) flags=0x%X",
                    NetWin->rRect1.left, NetWin->rRect1.top,
                    NetWin->rRect1.right, NetWin->rRect1.bottom,
                    NetWin->iFlags);
                sr_debug_log("NETWIN scroll_v=%p scroll_h=%p",
                    (void*)NetWin->scroll_vert, (void*)NetWin->scroll_horz);
                sr_debug_log("NETWIN oChildList: count=%d current=%d",
                    NetWin->oChildList.iCount, NetWin->oChildList.iCurrent);
                // Check if NetWin has CWinFonted-like fields (SpotList at 0xA2C)
                int winBaseInts = 0x444 / 4;
                if (!IsBadReadPtr(raw + 0xA2C/4, 12)) {
                    int* spotListPtr = &raw[0xA2C/4];
                    sr_debug_log("NETWIN @0xA2C (SpotList?): %p max=%d cur=%d",
                        (void*)spotListPtr[0], spotListPtr[1], spotListPtr[2]);
                }
                // Dump each child window in detail
                int max_dump = NetWin->iChildCount;
                if (max_dump > 20) max_dump = 20;
                for (int ci = 0; ci < max_dump; ci++) {
                    Win* child = NetWin->apoChildren[ci];
                    if (!child) {
                        sr_debug_log("  CHILD[%d] = NULL", ci);
                        continue;
                    }
                    int* cr = (int*)child;
                    sr_debug_log("  CHILD[%d] vtable=%p addr=%p childCount=%d flags=0x%X",
                        ci, (void*)cr[0], (void*)child, child->iChildCount, child->iFlags);
                    sr_debug_log("    rect=(%d,%d,%d,%d) caption=[%s]",
                        child->rRect1.left, child->rRect1.top,
                        child->rRect1.right, child->rRect1.bottom,
                        child->pszCaption ? child->pszCaption : "(null)");
                    // Try BaseButton name (offset 0xA7C)
                    char* btn_name = (char*)cr[0xA7C/4];
                    if (btn_name && (unsigned int)btn_name > 0x10000
                        && (unsigned int)btn_name < 0x7FFFFFFF
                        && !IsBadReadPtr(btn_name, 4)
                        && btn_name[0] >= 0x20 && btn_name[0] < 0x7F) {
                        sr_debug_log("    btn_name=[%s]", btn_name);
                    }
                }
                // Game state during setup
                sr_debug_log("NETWIN-STATE: GameRules=0x%X GameMoreRules=0x%X GameState=0x%X",
                    *GameRules, *GameMoreRules, *GameState);
                sr_debug_log("NETWIN-STATE: DiffLevel=%d MapSize=%d Ocean=%d Erosion=%d Native=%d Cloud=%d",
                    *DiffLevel, *MapSizePlanet, *MapOceanCoverage,
                    *MapErosiveForces, *MapNativeLifeForms, *MapCloudCover);
                sr_debug_log("NETWIN-STATE: MultiplayerActive=%d PbemActive=%d",
                    *MultiplayerActive, *PbemActive);
                sr_debug_log("NETWIN-STATE: DialogChoices=0x%X DialogToggle=0x%X",
                    *DialogChoices, *DialogToggle);
                // Safe raw memory dump with IsBadReadPtr check
                // Dump 512 bytes (128 ints) beyond Win base (0x444)
                for (int blk = 0; blk < 8; blk++) {
                    int off = winBaseInts + blk * 16;
                    if (IsBadReadPtr(raw + off, 64)) {
                        sr_debug_log("NETWIN-RAW +0x%03X: <unreadable>", off * 4);
                        break;
                    }
                    char hexline[512];
                    int pos = snprintf(hexline, sizeof(hexline),
                        "NETWIN-RAW +0x%03X:", (off * 4));
                    for (int j = 0; j < 16 && pos < 480; j++) {
                        pos += snprintf(hexline + pos, sizeof(hexline) - pos,
                            " %08X", (unsigned int)raw[off + j]);
                    }
                    sr_debug_log("%s", hexline);
                }
            }
        } else if (!in_netsetup && sr_netsetup_was_active) {
            MultiplayerHandler::OnClose();
            sr_debug_log("SCREEN: Multiplayer Setup closed");
            sr_announced[0] = '\0';
            sr_netsetup_log_coords = false;
        }
        sr_netsetup_was_active = in_netsetup;

        // --- Base change detection (Left/Right cycles bases) ---
        static int sr_prev_base_id = -1;
        if (cur_win == GW_Base && *CurrentBaseID != sr_prev_base_id) {
            if (sr_prev_base_id >= 0) {
                // Base changed while already in base screen (not initial open)
                BaseScreenHandler::OnOpen(false);  // no help on base switch
                sr_debug_log("BASE-CHANGE: %d -> %d", sr_prev_base_id, *CurrentBaseID);
            }
            sr_prev_base_id = *CurrentBaseID;
        } else if (cur_win != GW_Base) {
            sr_prev_base_id = -1;
        }

        // --- World map state ---
        bool on_world_map = MapWin &&
            (cur_win == GW_World || (cur_win == GW_None && cur_popup >= 2));
        bool in_menu = (cur_win == GW_None && cur_popup == 0
            && !CouncilHandler::IsActive());

        // Invalidate menu cache when leaving menu context
        if (!in_menu) MenuHandler::InvalidateCache();
        if (!on_world_map) MenuHandler::OnLeaveWorldMap();

        // World map handler: transition, map tracking, targeting, unit change, Trigger 4
        WorldMapHandler::OnTimer(now, on_world_map, cur_win, cur_popup,
            sr_announced, sizeof(sr_announced));

        // Diplomacy handler: detect session open/close
        DiplomacyHandler::OnTimer();

        // Multiplayer setup: detect player list changes
        if (in_netsetup) {
            MultiplayerHandler::OnTimer();
        }

        // --- Key tracking: reset capture on every significant keypress ---
        if (msg == WM_KEYDOWN) {
            sr_debug_log("KEY 0x%X popup=%d items=%d win=%d",
                (unsigned)wParam, *PopupDialogState, sr_item_count(),
                (int)current_window());

            // Popup list navigation (research priorities, etc.)
            // NOT used for file browser — file browser uses text-capture approach.
            if ((wParam == VK_UP || wParam == VK_DOWN)
                && !sr_file_browser_active()
                && sr_popup_list.active && sr_popup_list.count > 0) {
                if (wParam == VK_DOWN) {
                    sr_popup_list.index++;
                    if (sr_popup_list.index >= sr_popup_list.count)
                        sr_popup_list.index = sr_popup_list.count - 1;
                } else {
                    sr_popup_list.index--;
                    if (sr_popup_list.index < 0)
                        sr_popup_list.index = 0;
                }
                char buf[300];
                snprintf(buf, sizeof(buf), loc(SR_POPUP_LIST_FMT),
                    sr_popup_list.index + 1,
                    sr_popup_list.count,
                    sr_popup_list.items[sr_popup_list.index]);
                sr_output(buf, true);
                sr_debug_log("POPUP-LIST nav %d/%d: %s",
                    sr_popup_list.index + 1,
                    sr_popup_list.count,
                    sr_popup_list.items[sr_popup_list.index]);

                // NETCONNECT_CREATE/JOIN: call popup's vtable OnLButtonDown
                // with popup-local coordinates to switch editbox focus.
                if ((_stricmp(sr_popup_list.label, "NETCONNECT_CREATE") == 0
                    || _stricmp(sr_popup_list.label, "NETCONNECT_JOIN") == 0)
                    && sr_editbox_popup) {
                    int* vtbl = *(int**)sr_editbox_popup;
                    if (vtbl && !IsBadReadPtr(vtbl + 19, 4)) {
                        typedef int(__thiscall *FOnLBDown)(void*, int, int, int);
                        FOnLBDown onClick = (FOnLBDown)vtbl[19];
                        // Try multiple Y values to find the text fields.
                        // Popup is #xs 320 wide, click at center X=160.
                        static const int field_y[] = { 50, 90 };
                        int idx = sr_popup_list.index;
                        if (idx >= 0 && idx < 2) {
                            int cx = 160;
                            int cy = field_y[idx];
                            sr_debug_log("EDITBOX vtbl[19]=%p click(%d,%d) field[%d]",
                                (void*)vtbl[19], cx, cy, idx);
                            onClick(sr_editbox_popup, cx, cy, MK_LBUTTON);
                        }
                    } else {
                        sr_debug_log("EDITBOX bad vtable at %p", sr_editbox_popup);
                    }
                }
            } else if (wParam == VK_UP || wParam == VK_DOWN) {
                // Menu item cache: delegate to MenuHandler
                if (MenuHandler::OnArrowKey(wParam, now, on_world_map, cur_win,
                        sr_arrow_active, sr_arrow_dir, sr_arrow_time)) {
                    sr_last_seen_record = 0;
                    sr_stable_since = 0;
                } else if (CouncilHandler::IsActive()) {
                    // Council: set arrow trigger without menu cache
                    sr_arrow_active = true;
                    sr_arrow_dir = wParam;
                    sr_arrow_time = now;
                    sr_items_clear();
                    sr_clear_text();
                    sr_snapshot_consume();
                    sr_last_seen_record = 0;
                    sr_stable_since = 0;
                }
            } else if (wParam != VK_LEFT && wParam != VK_RIGHT
                       && wParam != VK_SHIFT && wParam != VK_CONTROL
                       && wParam != VK_MENU) {
                sr_arrow_active = false;
                sr_arrow_dir = 0;
                MenuHandler::InvalidateCache();
                sr_items_clear();
                sr_clear_text();
                sr_snapshot_consume();
                sr_last_seen_record = 0;
                sr_stable_since = 0;
            }
        }

        // Detect when new text is captured by hooks
        DWORD last_record = sr_get_last_record_time();
        if (last_record != sr_last_seen_record) {
            sr_last_seen_record = last_record;
            sr_stable_since = now;
        }

        // --- Trigger 1: Snapshot (draw cycle boundary) ---
        // Suppress on world map (popup >= 2) — HUD data is noise there.
        // World map feedback comes from MAP-MOVE tracker instead.
        // Skip all announce triggers while a modal handler is active
        bool sr_modal_active = SocialEngHandler::IsActive()
            || PrefsHandler::IsActive()
            || SpecialistHandler::IsActive()
            || DesignHandler::IsActive()
            || GovernorHandler::IsActive()
            || MessageHandler::IsActive()
            || MultiplayerHandler::IsActive()
            || StatusHandler::IsActive()
            || LabsHandler::IsActive()
            || ProjectsHandler::IsActive()
            || OrbitalHandler::IsActive()
            || MilitaryHandler::IsActive()
            || ScoreHandler::IsActive()
            || MonumentHandler::IsActive()
            || DatalinksHandler::IsActive()
            || NetSetupSettingsHandler::IsActive()
            || sr_is_number_input_active();
        if (sr_modal_active || sr_file_browser_active()) {
            sr_snapshot_consume();
        } else if (sr_snapshot_ready()) {
            if (Win_is_visible(TutWin)) {
                sr_debug_log("SNAP-DISCARD (tour active)");
            } else if (sr_tutorial_announce_time > 0
                && (now - sr_tutorial_announce_time) < 3000) {
                sr_debug_log("SNAP-DISCARD (tutorial recent)");
            } else if (in_menu) {
                sr_debug_log("SNAP-DISCARD (menu context)");
            } else if (cur_win == GW_Base) {
                sr_debug_log("SNAP-DISCARD (base screen)");
            } else if (cur_win == GW_World) {
                // World map text (landmarks, info panel) handled by Trigger 4 whitelist
                sr_debug_log("SNAP-DISCARD (world map)");
            } else if (FactionSelectHandler::IsActive()) {
                sr_debug_log("SNAP-DISCARD (faction select)");
            } else if (sr_arrow_active) {
                sr_debug_log("SNAP-DISCARD (arrow)");
            } else {
                int sn = sr_snapshot_count();
                if (sn > 10) {
                    // Too many items (e.g. base screen buttons) — skip,
                    // screen transition announcement handles orientation.
                    sr_debug_log("SNAP-DISCARD (count=%d)", sn);
                } else if (sn > 0) {
                    char buf[2048], all[2048];
                    if (sr_build_announce(sr_snapshot_get, sn, sr_announced,
                            buf, sizeof(buf), all, sizeof(all), in_menu_early, 0)) {
                        strncpy(sr_announced, all, sizeof(sr_announced) - 1);
                        sr_announced[sizeof(sr_announced) - 1] = '\0';
                        sr_debug_log("ANNOUNCE-SNAP(%d): %s", sn, buf);
                        sr_output(buf, false);
                    } else {
                        sr_debug_log("SETTLE-SNAP(%d)", sn);
                    }
                }
            }
            sr_snapshot_consume();
        }

        // --- Trigger 2: Arrow navigation (50ms after keypress) ---
        // Separate from timer: counts from KEY press, not from last capture.
        // HUD data arrives character-by-character and prevents sr_stable_since
        // from ever settling. This trigger ignores ongoing HUD captures.
        if (sr_arrow_active && sr_arrow_time > 0 && !on_world_map
            && !sr_modal_active && !FactionSelectHandler::IsActive()) {
            int count = sr_item_count();
            DWORD elapsed = now - sr_arrow_time;
            // File browser: editbox (item[0]) always has current selection,
            // use shorter timeout since we don't need 2 items.
            bool fb = sr_file_browser_active();
            int threshold1 = fb ? 50 : 200;
            if ((count >= 2 && elapsed > 50) || (count == 1 && elapsed > threshold1)) {
                // Find best nav target: skip HUD noise items.
                int nav_idx = -1;
                if (fb) {
                    // File browser: item[0] is the editbox = current selection
                    nav_idx = 0;
                } else if (count >= 2) {
                    // Regular menus: game renders top-to-bottom, so
                    //   DOWN: item[0]=leaving (above), item[1]=entering (below)
                    //   UP:   item[0]=entering (above), item[1]=leaving (below)
                    int entering = (sr_arrow_dir == VK_UP) ? 0 : 1;
                    int fallback = (entering == 1) ? 0 : 1;
                    const char* it_enter = sr_item_get(entering);
                    if (it_enter && !sr_is_hud_noise(it_enter, in_menu_early)) {
                        nav_idx = entering;
                    } else {
                        sr_debug_log("NAV-BOUNDARY item[%d] is HUD, using item[%d]",
                            entering, fallback);
                        nav_idx = fallback;
                    }
                } else {
                    nav_idx = 0;
                }
                const char* it = sr_item_get(nav_idx);
                // Log all captured items for debugging
                if (MenuHandler::IsCacheActive()) {
                    int mcpos = MenuHandler::CacheGetPos();
                    MenuHandler::McLog("T2: count=%d nav_idx=%d dir=%s pos=%d",
                        count, nav_idx,
                        sr_arrow_dir == VK_DOWN ? "DN" : "UP",
                        mcpos);
                    for (int di = 0; di < count && di < 5; di++) {
                        const char* dit = sr_item_get(di);
                        MenuHandler::McLog("  item[%d]: %s%s", di,
                            dit ? dit : "(null)",
                            (dit && sr_is_hud_noise(dit, in_menu_early)) ? " [HUD]" : "");
                    }
                }
                int nav_min_len = sr_file_browser_active() ? 2 : 3;
                if (it && (int)strlen(it) >= nav_min_len && !sr_is_hud_noise(it, in_menu_early)) {
                    // Cache entering item at current position
                    int mcpos = MenuHandler::CacheGetPos();
                    MenuHandler::CacheStore(mcpos, it);
                    MenuHandler::McLog("STORE pos=%d: %s", mcpos, it);
                    // Also cache the LEAVING item at previous position
                    if (MenuHandler::IsCacheActive() && count >= 2) {
                        int prev_pos = mcpos
                            + (sr_arrow_dir == VK_DOWN ? -1 : 1);
                        const char* leaving = sr_item_get(
                            nav_idx == 1 ? 0 : 1);
                        if (leaving && strlen(leaving) >= 3
                            && !sr_is_hud_noise(leaving, in_menu_early)) {
                            MenuHandler::CacheStoreAt(prev_pos, leaving);
                            MenuHandler::McLog("STORE-PREV pos=%d: %s",
                                prev_pos, leaving);
                        }
                    }

                    sr_debug_log("ANNOUNCE-NAV idx=%d/%d dir=%s: %s",
                        nav_idx, count,
                        sr_arrow_dir == VK_DOWN ? "DOWN" : "UP", it);
                    if (sr_file_browser_active()) {
                        sr_fb_on_text_captured(it);
                    } else {
                        sr_output(it, true);
                    }
                    if (strstr(sr_announced, it) == NULL) {
                        int al = strlen(sr_announced);
                        int il = strlen(it);
                        if (al + il + 3 < (int)sizeof(sr_announced)) {
                            if (al > 0) {
                                sr_announced[al++] = '.';
                                sr_announced[al++] = ' ';
                            }
                            memcpy(sr_announced + al, it, il + 1);
                        }
                    }
                }
                sr_arrow_active = false;
                sr_arrow_time = 0;
            }
        }

        // --- Trigger 3: Timer (items stable for delay ms) ---
        // Suppress on world map (HUD noise), in menus (arrow nav handles it),
        // and in base screen (BaseScreenHandler provides structured info).
        if (sr_stable_since > 0 && (now - sr_stable_since) > 300
            && sr_item_count() > 0 && !on_world_map && !sr_arrow_active
            && !in_menu && !sr_modal_active && !sr_file_browser_active()
            && cur_win != GW_Base && !FactionSelectHandler::IsActive()) {
            int count = sr_item_count();
            char buf[2048], all[2048];
            if (sr_build_announce(sr_item_get, count, sr_announced,
                    buf, sizeof(buf), all, sizeof(all), in_menu_early, 5)) {
                strncpy(sr_announced, all, sizeof(sr_announced) - 1);
                sr_announced[sizeof(sr_announced) - 1] = '\0';
                sr_debug_log("ANNOUNCE-TIMER(%d): %s", count, buf);
                sr_output(buf, false);
            } else {
                sr_debug_log("SETTLE-TIMER(%d)", count);
            }
            sr_stable_since = 0;
        }

        // --- Council: build vote list + detect results ---
        if (CouncilHandler::IsActive() && sr_stable_since == 0
            && sr_item_count() > 2) {
            CouncilHandler::TryBuildVoteList();
            CouncilHandler::CheckAndAnnounceResults();
        }

        // Trigger 4 (worldmap polling) handled by WorldMapHandler::OnTimer above
    }
    // === End screen reader announce system ===

    // Consume WM_CHAR following a consumed Space/key WM_KEYDOWN
    // (prevents game from seeing the space character and skipping units)
    static bool sr_consumed_key_char = false;
    if (msg == WM_CHAR && sr_consumed_key_char) {
        sr_consumed_key_char = false;
        return 0;
    }
    if (msg == WM_KEYDOWN) {
        sr_consumed_key_char = false;
    }

    // Screen reader: echo typed characters in game-native popup text inputs
    // (base naming, save game, etc.) — does NOT consume the message.
    // Shadow buffer tracks input for Ctrl+R readback and Backspace feedback.
    static char sr_popup_input_buf[128] = {};
    static int sr_popup_input_len = 0;
    static bool sr_popup_input_was_active = false;
    {
        bool in_popup_input = sr_is_available()
            && !sr_is_number_input_active()
            && !BaseScreenHandler::IsRenameActive()
            && !MultiplayerHandler::IsActive()
            && !GameSettingsHandler::IsActive()
            && (sr_popup_is_active() || *WinModalState != 0);

        // Clear shadow buffer when popup input state ends
        if (sr_popup_input_was_active && !in_popup_input) {
            sr_popup_input_len = 0;
            sr_popup_input_buf[0] = '\0';
        }
        sr_popup_input_was_active = in_popup_input;

        if (in_popup_input) {
            if (msg == WM_CHAR) {
                unsigned char ch = (unsigned char)wParam;
                if (ch >= 32 && ch != 127) {
                    // Track in shadow buffer
                    if (sr_popup_input_len < (int)sizeof(sr_popup_input_buf) - 1) {
                        sr_popup_input_buf[sr_popup_input_len++] = (char)ch;
                        sr_popup_input_buf[sr_popup_input_len] = '\0';
                    }
                    // Echo: convert Windows-1252 to UTF-8 for speech
                    char ansi[2] = { (char)ch, '\0' };
                    char utf8[8];
                    sr_ansi_to_utf8(ansi, utf8, sizeof(utf8));
                    sr_output(utf8, true);
                }
            } else if (msg == WM_KEYDOWN && wParam == VK_BACK) {
                if (sr_popup_input_len > 0) {
                    sr_popup_input_len--;
                    sr_popup_input_buf[sr_popup_input_len] = '\0';
                    if (sr_popup_input_len > 0) {
                        sr_output(sr_game_str(sr_popup_input_buf), true);
                    } else {
                        sr_output(loc(SR_INPUT_NUMBER_EMPTY), true);
                    }
                }
            } else if (msg == WM_KEYDOWN && wParam == 'R' && ctrl_key_down()) {
                if (sr_popup_input_len > 0) {
                    sr_output(sr_game_str(sr_popup_input_buf), true);
                } else {
                    sr_output(loc(SR_INPUT_NUMBER_EMPTY), true);
                }
            }
        }
    }

    if (msg == WM_ACTIVATEAPP && conf.auto_minimise && !conf.reduced_mode) {
        if (!LOWORD(wParam)) {
            //If window has just become inactive e.g. ALT+TAB
            //wParam is 0 if the window has become inactive.
            set_minimised(true);
        } else {
            set_minimised(false);
        }
        return WinProc(hwnd, msg, wParam, lParam);

    } else if (msg == WM_MOVIEOVER) {
        conf.playing_movie = false;
        set_video_mode(1);

    } else if (msg == WM_KEYDOWN && LOWORD(wParam) == VK_RETURN
    && GetAsyncKeyState(VK_MENU) < 0 && !conf.reduced_mode) {
        if (conf.video_mode == VM_Custom) {
            set_windowed(true);
        } else if (conf.video_mode == VM_Window) {
            set_windowed(false);
        }

    // Screen reader: StatusHandler modal — must be BEFORE all other key handlers
    // so that keys dispatched from our PeekMessage loop are not consumed by
    // world map, base screen, or other handlers.
    } else if ((msg == WM_KEYDOWN || msg == WM_CHAR) && StatusHandler::IsActive()) {
        if (msg == WM_KEYDOWN) {
            StatusHandler::Update(msg, wParam);
        }
        return 0;

    } else if ((msg == WM_KEYDOWN || msg == WM_CHAR)
    && dispatch_analysis_handler(msg, wParam)) {
        return 0;

    } else if (msg == WM_WINDOWED && !conf.reduced_mode) {
        RECT window_rect;
        WINDOWPLACEMENT wp;
        memset(&wp, 0, sizeof(wp));
        wp.length = sizeof(wp);
        GetWindowPlacement(*phWnd, &wp);
        wp.flags = 0;
        wp.showCmd = SW_SHOWNORMAL;
        SetRect(&window_rect, 0, 0, conf.window_width, conf.window_height);
        wp.rcNormalPosition = window_rect;
        SetWindowPlacement(*phWnd, &wp);
        SetWindowPos(*phWnd, HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);

    } else if (msg == WM_MOUSEWHEEL && win_has_focus()) {
        int wheel_delta = GET_WHEEL_DELTA_WPARAM(wParam) + delta_accum;
        delta_accum = wheel_delta % WHEEL_DELTA;
        wheel_delta /= WHEEL_DELTA;
        bool zoom_in = (wheel_delta >= 0);
        wheel_delta = abs(wheel_delta);
        GameWinState state = current_window();

        if (state == GW_World) {
            int zoom_type = (zoom_in ? 515 : 516);
            for (int i = 0; i < wheel_delta; i++) {
                if (MapWin->iZoomFactor > -8 || zoom_in) {
                    Console_zoom(MapWin, zoom_type, 0);
                }
            }
        } else if (state == GW_Base && conf.render_high_detail) {
            base_resource_zoom(zoom_in);
        } else {
            int key;
            if (state == GW_Design) {
                key = (zoom_in ? VK_LEFT : VK_RIGHT);
            } else {
                key = (zoom_in ? VK_UP : VK_DOWN);
            }
            wheel_delta *= CState.ListScrollDelta;
            for (int i = 0; i < wheel_delta; i++) {
                PostMessage(hwnd, WM_KEYDOWN, key, 0);
                PostMessage(hwnd, WM_KEYUP, key, 0);
            }
            return 0;
        }

    // Screen reader: Number input modal intercepts all keys when active
    } else if ((msg == WM_KEYDOWN || msg == WM_CHAR) && sr_is_available()
    && sr_is_number_input_active()) {
        sr_number_input_update(msg, wParam);
        return 0;

    // Screen reader: Multiplayer setup handler (non-modal, yields to popup list)
    // Only consume keys that the handler actually handles (returns true).
    // Keys like Escape pass through to the game.
    } else if ((msg == WM_KEYDOWN || msg == WM_CHAR) && sr_is_available()
    && MultiplayerHandler::IsActive() && !sr_popup_list.active) {
        static bool sr_mp_last_consumed = false;
        if (msg == WM_KEYDOWN) {
            sr_mp_last_consumed = MultiplayerHandler::Update(msg, wParam);
            if (sr_mp_last_consumed) return 0;
            // If Update returned false, let key pass through to game
        } else if (msg == WM_CHAR && sr_mp_last_consumed) {
            return 0; // consume WM_CHAR for keys we handled
        }

    // Screen reader: Modal handlers (SocialEng, Prefs, Specialist) intercept ALL
    // messages when active. WM_CHAR must also be consumed to prevent the game's
    // WinProc from seeing translated keypresses inside our modal loop.
    } else if ((msg == WM_KEYDOWN || msg == WM_CHAR) && sr_is_available()
    && (SocialEngHandler::IsActive() || PrefsHandler::IsActive()
        || SpecialistHandler::IsActive() || DesignHandler::IsActive()
        || GovernorHandler::IsActive() || StatusHandler::IsActive()
        || GameSettingsHandler::IsActive() || NetSetupSettingsHandler::IsActive()
        || ThinkerMenuHandler::IsActive() || FileBrowserHandler::IsActive())) {
        if (msg == WM_KEYDOWN) {
            if (FileBrowserHandler::IsActive()) {
                FileBrowserHandler::Update(msg, wParam);
            } else if (SocialEngHandler::IsActive()) {
                SocialEngHandler::Update(msg, wParam);
            } else if (PrefsHandler::IsActive()) {
                PrefsHandler::Update(msg, wParam);
            } else if (DesignHandler::IsActive()) {
                DesignHandler::Update(msg, wParam);
            } else if (GovernorHandler::IsActive()) {
                GovernorHandler::Update(msg, wParam);
            } else if (StatusHandler::IsActive()) {
                StatusHandler::Update(msg, wParam);
            } else if (GameSettingsHandler::IsActive()) {
                GameSettingsHandler::Update(msg, wParam);
            } else if (NetSetupSettingsHandler::IsActive()) {
                NetSetupSettingsHandler::Update(msg, wParam);
            } else if (ThinkerMenuHandler::IsActive()) {
                ThinkerMenuHandler::Update(msg, wParam);
            } else {
                SpecialistHandler::Update(msg, wParam);
            }
        }
        return 0; // consume WM_CHAR and unhandled WM_KEYDOWN

    // Screen reader: rename mode intercepts WM_KEYDOWN + WM_CHAR
    } else if ((msg == WM_KEYDOWN || msg == WM_CHAR) && sr_is_available()
    && current_window() == GW_Base
    && BaseScreenHandler::IsRenameActive()) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: production picker intercepts ALL keys when active (must be first)
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && current_window() == GW_Base
    && BaseScreenHandler::IsPickerActive()) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: queue mode intercepts ALL keys when active
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && current_window() == GW_Base
    && BaseScreenHandler::IsQueueActive()) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: demolition mode intercepts ALL keys when active
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && current_window() == GW_Base
    && BaseScreenHandler::IsDemolitionActive()) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: garrison mode intercepts ALL keys when active
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && current_window() == GW_Base
    && BaseScreenHandler::IsGarrisonActive()) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: support mode intercepts ALL keys when active
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && current_window() == GW_Base
    && BaseScreenHandler::IsSupportActive()) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: tile assignment mode intercepts ALL keys when active
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && current_window() == GW_Base
    && BaseScreenHandler::IsTileAssignActive()) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+D = open facility demolition
    } else if (msg == WM_KEYDOWN && wParam == 'D' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+U = open garrison list
    } else if (msg == WM_KEYDOWN && wParam == 'U' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+Shift+S = open supported units list
    } else if (msg == WM_KEYDOWN && wParam == 'S' && ctrl_key_down()
    && (GetKeyState(VK_SHIFT) & 0x8000) && sr_is_available()
    && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+Shift+Y = psych detail
    } else if (msg == WM_KEYDOWN && wParam == 'Y' && ctrl_key_down()
    && (GetKeyState(VK_SHIFT) & 0x8000) && sr_is_available()
    && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+T = open tile assignment
    } else if (msg == WM_KEYDOWN && wParam == 'T' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+Q = open queue management
    } else if (msg == WM_KEYDOWN && wParam == 'Q' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+W = open specialist management
    } else if (msg == WM_KEYDOWN && wParam == 'W' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        SpecialistHandler::RunModal();
        return 0;

    // Screen reader: Ctrl+N = nerve staple
    } else if (msg == WM_KEYDOWN && wParam == 'N' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: F2 = rename base
    } else if (msg == WM_KEYDOWN && wParam == VK_F2
    && sr_is_available() && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+G = governor configuration
    } else if (msg == WM_KEYDOWN && wParam == 'G' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        GovernorHandler::RunModal();
        return 0;

    } else if (msg == WM_KEYDOWN && (wParam == VK_UP || wParam == VK_DOWN)
    && ctrl_key_down() && current_window() == GW_Base) {
        if (sr_is_available()) {
            BaseScreenHandler::Update(msg, wParam);
        } else if (conf.render_high_detail) {
            base_resource_zoom(wParam == VK_UP);
        }

    } else if (msg == WM_KEYDOWN && wParam == 'I' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        BaseScreenHandler::Update(msg, wParam);

    // Screen reader: Ctrl+Shift+P = open production picker
    } else if (msg == WM_KEYDOWN && wParam == 'P' && ctrl_key_down()
    && (GetKeyState(VK_SHIFT) & 0x8000) && sr_is_available()
    && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

    // Screen reader: Ctrl+F1 = help for base screen
    } else if (msg == WM_KEYDOWN && wParam == VK_F1 && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        sr_output(BaseScreenHandler::GetHelpText(), true);
        return 0;

    } else if (msg == WM_KEYDOWN && (wParam == VK_LEFT || wParam == VK_RIGHT)
    && ctrl_key_down() && current_window() == GW_Base) {
        int32_t value = BaseWin->oRender.iResWindowTab;
        if (wParam == VK_LEFT) {
            value = (value + 1) % 3;
        } else {
            value = (value + 2) % 3;
        }
        BaseWin->oRender.iResWindowTab = value;
        GraphicWin_redraw(BaseWin);
        if (sr_is_available()) {
            const char* tab_names[] = {loc(SR_TAB_RESOURCES), loc(SR_TAB_CITIZENS), loc(SR_TAB_PRODUCTION)};
            char buf[64];
            snprintf(buf, sizeof(buf), loc(SR_TAB_FMT), tab_names[value % 3]);
            sr_output(buf, true);
            // Set matching section and announce content (queued, non-interrupting)
            BaseScreenHandler::SetSectionFromTab(value % 3);
            BaseScreenHandler::AnnounceCurrentSection(false);
        }

    } else if (msg == WM_KEYDOWN && wParam == 'H' && ctrl_key_down()
    && !*MultiplayerActive && current_window() == GW_Base && *CurrentBaseID >= 0
    && Bases[*CurrentBaseID].faction_id == MapWin->cOwner) {
        BASE* base = &Bases[*CurrentBaseID];
        Faction* f = &Factions[base->faction_id];
        int mins = max(0, mineral_cost(*CurrentBaseID, base->item()) - base->minerals_accumulated);
        int cost = hurry_cost(*CurrentBaseID, base->item(), mins);
        if (base->can_hurry_item() && cost > 0 && mins > 0) {
            int available = max(0, f->energy_credits - f->hurry_cost_total);
            if (available >= cost) {
                f->energy_credits -= cost;
                base->minerals_accumulated += mins;
                base->state_flags |= BSTATE_HURRY_PRODUCTION;
                GraphicWin_redraw(BaseWin);
                ok_callback();
                if (sr_is_available()) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), loc(SR_HURRY_OK),
                        sr_game_str(prod_name(base->item())), cost, f->energy_credits);
                    sr_output(buf, true);
                }
            } else {
                wave_it(9); // Insufficient energy
                if (sr_is_available()) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), loc(SR_HURRY_NO_CREDITS),
                        cost, available);
                    sr_output(buf, true);
                }
            }
        } else if (cost > 0 && mins > 0) {
            wave_it(8); // Cannot execute order
            if (sr_is_available()) {
                sr_output(loc(SR_HURRY_CANNOT), true);
            }
        }

    } else if (conf.smooth_scrolling && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
        if (current_window() != GW_World || !win_has_focus()) {
            CState.RightButtonDown = false;
            CState.ScrollDragging = false;
            return WinProc(hwnd, msg, wParam, lParam);

        } else if (msg == WM_RBUTTONDOWN) {
            CState.RightButtonDown = true;
            GetCursorPos(&p);
            memcpy(&CState.ScrollDragPos, &p, sizeof(POINT));

        } else if (msg == WM_RBUTTONUP) {
            CState.RightButtonDown = false;
            if (CState.ScrollDragging) {
                CState.ScrollDragging = false;
                SetCursor(LoadCursor(0, IDC_ARROW));
            } else {
                WinProc(hwnd, WM_RBUTTONDOWN, wParam | MK_RBUTTON, lParam);
                return WinProc(hwnd, WM_RBUTTONUP, wParam, lParam);
            }
        } else if (CState.RightButtonDown) {
            check_scroll();
        } else {
            return WinProc(hwnd, msg, wParam, lParam);
        }

    } else if (conf.smooth_scrolling && msg == WM_CHAR && wParam == 'r' && alt_key_down()) {
        CState.MouseOverTileInfo = !CState.MouseOverTileInfo;

    } else if (!conf.reduced_mode && msg == WM_CHAR && wParam == 't' && alt_key_down()) {
        if (sr_is_available()) {
            ThinkerMenuHandler::RunModal();
        } else {
            show_mod_menu();
        }

    } else if (conf.reduced_mode && msg == WM_CHAR && wParam == 'h' && alt_key_down()) {
        if (sr_is_available()) {
            ThinkerMenuHandler::RunModal();
        } else {
            show_mod_menu();
        }

    // Screen reader: Menu bar navigation (Ctrl+F2 + arrow keys)
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && MenuHandler::HandleKey(hwnd, msg, wParam, lParam)) {
        return 0;

    } else if (msg == WM_CHAR && wParam == 'o' && alt_key_down() && is_editor) {
        uint32_t seed = ThinkerVars->map_random_value;
        int value = pop_ask_number("modmenu", "MAPGEN", seed, 0);
        if (!value) { // OK button pressed
            console_world_generate(ParseNumTable[0]);
        }

    // Screen reader: Faction selection (Ctrl+F1 help)
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && FactionSelectHandler::HandleKey(hwnd, msg, wParam)) {
        return 0;

    // Screen reader: Council key handling (S/Tab vote summary, Ctrl+F1 help)
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && CouncilHandler::HandleKey(msg, wParam)) {
        return 0;

    // Screen reader: F-key modals (F1-F9, Ctrl+F10, Ctrl+Shift+F10)
    } else if (handle_fkey_modals(msg, wParam)) {
        return 0;

    // Screen reader: Diplomacy key handling (S/Tab summary, Ctrl+F1 help)
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && DiplomacyHandler::HandleKey(msg, wParam)) {
        sr_consumed_key_char = true;
        return 0;

    // Screen reader: World map key handling (scanner, arrows, targeting, G, E, etc.)
    } else if (msg == WM_KEYDOWN && sr_is_available()
    && WorldMapHandler::HandleKey(hwnd, msg, wParam, lParam)) {
        sr_consumed_key_char = true;
        return 0;

    // Screen reader: Ctrl+M = open message log browser
    } else if (msg == WM_KEYDOWN && wParam == 'M' && ctrl_key_down()
    && !shift_key_down() && sr_is_available()
    && (current_window() == GW_World || current_window() == GW_None)
    && !*GameHalted) {
        MessageHandler::OpenBrowser();
        return 0;

    // Screen reader: Shift+T = accessible time controls dialog
    } else if (msg == WM_KEYDOWN && wParam == 'T' && shift_key_down()
    && !ctrl_key_down() && !alt_key_down() && sr_is_available()
    && current_window() == GW_World && !*GameHalted) {
        WorldMapHandler::RunTimeControls();
        return 0;

    // Screen reader: Ctrl+C = announce chat opening (pass through to game)
    } else if (msg == WM_KEYDOWN && wParam == 'C' && ctrl_key_down()
    && !shift_key_down() && !alt_key_down() && sr_is_available()
    && *MultiplayerActive && current_window() == GW_World && !*GameHalted) {
        sr_output(loc(SR_CHAT_OPEN), true);
        // Don't return 0 — let key pass through to game's native handler

    // Screen reader: utility keys (silence, history, toggles, commlink, debug)
    } else if (handle_sr_utility_keys(msg, wParam)) {
        return 0;

    } else if (msg == WM_CHAR && wParam == 'l' && alt_key_down() && is_editor
    && *ReplayEventSize > 0) {
        show_replay();

    } else if (DEBUG && !*GameHalted && msg == WM_KEYDOWN && wParam == 'Q'
    && ctrl_key_down() && shift_key_down()) {
        net_game_close(); // Close network multiplayer if active
        *ControlTurnA = 1; // Return to main menu without dialog
        *ControlTurnB = 1;

    } else if (DEBUG && msg == WM_CHAR && wParam == 'd' && alt_key_down()) {
        conf.debug_mode = !conf.debug_mode;
        if (conf.debug_mode) {
            for (int i = 1; i < MaxPlayerNum; i++) {
                Faction& f = Factions[i];
                if (!f.base_count) {
                    memset(f.goals, 0, sizeof(f.goals));
                    memset(f.sites, 0, sizeof(f.sites));
                }
            }
            *GameState |= STATE_DEBUG_MODE;
            *GamePreferences |= PREF_ADV_FAST_BATTLE_RESOLUTION;
        } else {
            *GameState &= ~STATE_DEBUG_MODE;
        }
        if (!*GameHalted) {
            MapWin_draw_map(MapWin, 0);
            InvalidateRect(hwnd, NULL, false);
            parse_says(0, MOD_VERSION, -1, -1);
            parse_says(1, (conf.debug_mode ?
                "Debug mode enabled." : "Debug mode disabled."), -1, -1);
            popp("modmenu", "GENERIC", 0, 0, 0);
        }

    } else if (debug_cmd && wParam == 'm' && alt_key_down()) {
        conf.debug_verbose = !conf.debug_verbose;
        parse_says(0, MOD_VERSION, -1, -1);
        parse_says(1, (conf.debug_verbose ?
            "Verbose mode enabled." : "Verbose mode disabled."), -1, -1);
        popp("modmenu", "GENERIC", 0, 0, 0);

    } else if (debug_cmd && wParam == 'b' && alt_key_down() && Win_is_visible(BaseWin)) {
        conf.base_psych = !conf.base_psych;
        base_compute(1);
        BaseWin_on_redraw(BaseWin);

    } else if (debug_cmd && wParam == 'p' && alt_key_down()) {
        parse_says(0, "diplomatic patience", -1, -1);
        int value = pop_ask_number("modmenu", "ASKNUMBER", conf.diplo_patience, 0);
        if (!value) { // OK button pressed
            conf.diplo_patience = max(0, ParseNumTable[0]);
        }

    } else if (debug_cmd && wParam == 'c' && alt_key_down() && is_editor
    && (sq = mapsq(MapWin->iTileX, MapWin->iTileY)) && sq->lm_items()) {
        uint32_t prev_state = MapWin->iWhatToDrawFlags;
        MapWin->iWhatToDrawFlags |= MAPWIN_DRAW_GOALS;
        refresh_overlay(code_at);
        int value = pop_ask_number("modmenu", "MAPGEN", sq->code_at(), 0);
        if (!value) { // OK button pressed
            code_set(MapWin->iTileX, MapWin->iTileY, ParseNumTable[0]);
        }
        refresh_overlay(clear_overlay);
        MapWin->iWhatToDrawFlags = prev_state;
        draw_map(1);

    } else if (debug_cmd && wParam == 'y' && alt_key_down()) {
        static int draw_diplo = 0;
        draw_diplo = !draw_diplo;
        if (draw_diplo) {
            MapWin->iWhatToDrawFlags |= MAPWIN_DRAW_DIPLO_STATE;
            *GameState |= STATE_DEBUG_MODE;
        } else {
            MapWin->iWhatToDrawFlags &= ~MAPWIN_DRAW_DIPLO_STATE;
        }
        MapWin_draw_map(MapWin, 0);
        InvalidateRect(hwnd, NULL, false);

    } else if (debug_cmd && wParam == 'v' && alt_key_down()) {
        MapWin->iWhatToDrawFlags |= MAPWIN_DRAW_GOALS;
        refresh_overlay(clear_overlay);
        static int ts_type = 0;
        int i = 0;
        TileSearch ts;
        ts_type = (ts_type+1) % (MaxTileSearchType+1);
        ts.init(MapWin->iTileX, MapWin->iTileY, ts_type, 0);
        while (ts.get_next() != NULL) {
            mapdata[{ts.rx, ts.ry}].overlay = ++i;
        }
        mapdata[{MapWin->iTileX, MapWin->iTileY}].overlay = -ts_type;
        MapWin_draw_map(MapWin, 0);
        InvalidateRect(hwnd, NULL, false);

    } else if (debug_cmd && wParam == 'f' && alt_key_down()
    && (sq = mapsq(MapWin->iTileX, MapWin->iTileY)) && sq->is_owned()) {
        MapWin->iWhatToDrawFlags |= MAPWIN_DRAW_GOALS;
        move_upkeep(sq->owner, UM_Visual);
        MapWin_draw_map(MapWin, 0);
        InvalidateRect(hwnd, NULL, false);

    } else if (debug_cmd && wParam == 'x' && alt_key_down()) {
        MapWin->iWhatToDrawFlags |= MAPWIN_DRAW_GOALS;
        static int px = 0, py = 0;
        int x = MapWin->iTileX, y = MapWin->iTileY;
        int unit_id = is_ocean(mapsq(x, y)) ? BSC_UNITY_FOIL : BSC_UNITY_ROVER;
        show_path_cost(px, py, x, y, unit_id, MapWin->cOwner);
        px=x;
        py=y;
        MapWin_draw_map(MapWin, 0);
        InvalidateRect(hwnd, NULL, false);

    } else if (debug_cmd && wParam == 'z' && alt_key_down()) {
        int x = MapWin->iTileX, y = MapWin->iTileY;
        int base_id;
        if ((base_id = base_at(x, y)) >= 0) {
            print_base(base_id);
        }
        print_map(x, y);
        for (int k = 0; k < *VehCount; k++) {
            VEH* veh = &Vehs[k];
            if (veh->x == x && veh->y == y) {
                Vehs[k].state |= VSTATE_UNK_40000;
                Vehs[k].state &= ~VSTATE_UNK_2000;
                print_veh(k);
            }
        }
        flushlog();

    } else {
        return WinProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int __thiscall mod_MapWin_focus(Console* This, int x, int y)
{
    // Return value is non-zero when the map is recentered offscreen
    if (MapWin_focus(This, x, y)) {
        This->drawOnlyCursor = 0;
        draw_map(1);
    }
    return 0;
}

int __thiscall mod_MapWin_set_center(Console* This, int x, int y, int flag)
{
    // Make sure the whole screen is refreshed when clicking on map tiles
    if (!in_box(x, y, RenderTileBounds)) {
        This->drawOnlyCursor = 0;
    }
    return MapWin_set_center(This, x, y, flag);
}

/*
This is called when ReportWin is closing and is used to refresh base labels
on any bases where workers have been adjusted from the base list window.
*/
int __thiscall ReportWin_close_handler(void* This)
{
    SubInterface_release_iface_mode(This);
    return draw_map(1);
}

/*
Fix potential crash when a game is loaded after using Edit Map > Generate/Remove Fungus > No Fungus.
Original version changed MapWin->cOwner variable for unknown reason which is skipped.
*/
void __thiscall Console_editor_fungus(Console* UNUSED(This))
{
    auto_undo();
    int v1 = X_pop7("FUNGOSITY", PopDialogBtnCancel, 0);
    if (v1 >= 0) {
        int v2 = 0;
        if (!v1 || (v2 = X_pop7("FUNGMOTIZE", PopDialogBtnCancel, 0)) > 0) {
            MAP* sq = *MapTiles;
            for (int i = 0; i < *MapAreaTiles; ++i, ++sq) {
                sq->items &= ~BIT_FUNGUS;
                for (int j = 1; j < 8; ++j) {
                    sq->visible_items[j - 1] = sq->items;
                }
            }
        }
        if (v2 < 0) {
            return;
        }
        if (v1 > 0) {
            int v3 = *MapNativeLifeForms;
            *MapNativeLifeForms = v1 - 1;
            world_fungus();
            *MapNativeLifeForms = v3;
        }
        draw_map(1);
    }
}

/*
Fix foreign base names being visible in unexplored tiles when issuing move to or patrol
orders to the tiles. This version adds visibility checks for all base tiles.
*/
void __cdecl say_loc(char* dest, int x, int y, int a4, int a5, int a6)
{
    int base_id = -1;
    MAP* sq;

    if ((sq = mapsq(x, y)) && sq->is_base()
    && (*GameState & STATE_SCENARIO_EDITOR || sq->is_visible(MapWin->cOwner))) {
        base_id = base_at(x, y);
    }
    if (a4 != 0 && base_id < 0) {
        a6 = 0;
        base_id = mod_base_find3(x, y, -1, -1, -1, MapWin->cOwner);
        if (base_id >= 0) {
            strncat(dest, label_get(62), 32); // near
            strncat(dest, " ", 2);
        }
    }
    if (base_id >= 0) {
        if (a6) {
            strncat(dest, label_get(8), 32); // at
            strncat(dest, " ", 2);
        }
        strncat(dest, sr_game_str(Bases[base_id].name), MaxBaseNameLen);
        if (a5) {
            strncat(dest, " ", 2);
        }
    }
    if (a5 == 1 || (a5 == 2 && base_id < 0)) {
        snprintf(dest + strlen(dest), 32, "(%d, %d)", x, y);
    }
}

/*
Fix diplomacy dialog to show the missing response messages (GAVEENERGY) when gifting
energy credits to another faction. The error happens when the label is written to
StrBuffer in make_gift but diplomacy_caption overwrites it with other data.
*/
void __cdecl mod_diplomacy_caption(int faction1, int faction2)
{
    char buf[StrBufLen];
    strcpy_n(buf, StrBufLen, StrBuffer);
    diplomacy_caption(faction1, faction2);
    strcpy_n(StrBuffer, StrBufLen, buf);
}

/*
Fix foreign_treaty_popup displaying same treaty changes multiple times per turn.
In any case, these events will be displayed as a non-persistent message in map window.
*/
static char netmsg_label[StrBufLen] = {};
static char netmsg_item0[StrBufLen] = {};
static char netmsg_item1[StrBufLen] = {};

void __cdecl reset_netmsg_status()
{
    netmsg_label[0] = '\0';
    netmsg_item0[0] = '\0';
    netmsg_item1[0] = '\0';
}

int __thiscall mod_NetMsg_pop(void* This, const char* label, int delay, int a4, const char* a5)
{
    // Capture all messages into the SR message log
    if (sr_is_available() && label) {
        MessageHandler::OnMessage(label, a5);
        sr_output(loc(SR_POPUP_CONTINUE), false);
    }

    if (!conf.foreign_treaty_popup) {
        return NetMsg_pop(This, label, delay, a4, a5);
    }
    if (!strcmp(label, "GOTMYPROBE")) {
        return NetMsg_pop(This, label, -1, a4, a5);
    }
    if (!strcmp(label, netmsg_label)
    && !strcmp(ParseStrBuffer[0].str, netmsg_item0)
    && !strcmp(ParseStrBuffer[1].str, netmsg_item1)) {
        // Skip additional popup windows
        return NetMsg_pop(This, label, delay, a4, a5);
    }
    strcpy_n(netmsg_label, StrBufLen, label);
    strcpy_n(netmsg_item0, StrBufLen, ParseStrBuffer[0].str);
    strcpy_n(netmsg_item1, StrBufLen, ParseStrBuffer[1].str);

    return NetMsg_pop(This, label, -1, a4, a5);
}

// Pre-game startup menus that get full modal replacement (Pattern A).
static bool sr_is_startup_menu(const char* label) {
    static const char* menus[] = {
        "TOPMENU", "MAPMENU", "MULTIMENU", "SCENARIOMENU", "USERULES"
    };
    for (int i = 0; i < 5; i++)
        if (_stricmp(label, menus[i]) == 0) return true;
    return false;
}

// Labels that are navigable menus — announce only the menu name, not body.
// Returns a friendly name for the menu, or NULL if not a menu label.
static const char* sr_menu_name(const char* label) {
    static const struct { const char* label; SrStr str; } named_menus[] = {
        {"TOPMENU",       SR_MENU_MAIN},
        {"MAPMENU",       SR_MENU_MAP_MENU},
        {"MULTIMENU",     SR_MENU_MULTIPLAYER},
        {"SCENARIOMENU",  SR_MENU_SCENARIO_MENU},
        {"USERULES",      SR_TMENU_RULES},
        {"MAINMENU",      SR_MENU_THINKER},
        {"GAMEMENU",      SR_MENU_GAME_MENU},
    };
    for (int i = 0; i < (int)(sizeof(named_menus) / sizeof(named_menus[0])); i++) {
        if (_stricmp(label, named_menus[i].label) == 0) {
            return loc(named_menus[i].str);
        }
    }
    // Menus that read #caption from file (return empty string)
    static const char* caption_menus[] = {
        "WORLDSIZE", "WORLDLAND", "WORLDTIDES", "WORLDORBIT",
        "WORLDLIFE", "WORLDCLOUDS", "WORLDNATIVE",
    };
    for (int i = 0; i < (int)(sizeof(caption_menus) / sizeof(caption_menus[0])); i++) {
        if (_stricmp(label, caption_menus[i]) == 0) {
            return "";
        }
    }
    return NULL;  // not a menu
}

// State for startup menu modal replacement.
// Set by mod_BasePop_start, consumed by mod_startup_menu_runner.
static bool sr_startup_menu_pending = false;
static char sr_startup_menu_label[64] = "";

// Original menu display function at 0x4ADB20.
typedef int(__thiscall *FMenuRunner)(void*, void*, int, int);
static FMenuRunner OrigMenuRunner = (FMenuRunner)0x4ADB20;

/// Hook for the menu display call (0x4ADB20) at TOPMENU/MAPMENU call sites.
/// When a startup menu is pending, shows our accessible modal instead of
/// the game's mouse-driven popup menu. The Win object is fully constructed
/// so destructors run safely afterward.
int __thiscall mod_startup_menu_runner(void* ContainerThis, void* WinObj, int flag, int extra) {
    if (sr_startup_menu_pending && sr_is_available() && sr_popup_list.count > 0) {
        sr_startup_menu_pending = false;
        const char* title = sr_menu_name(sr_startup_menu_label);
        int result = sr_accessible_menu_modal(
            (title && title[0]) ? title : sr_startup_menu_label,
            sr_popup_list.items, sr_popup_list.count);
        sr_popup_list_clear();
        sr_debug_log("MENU-MODAL %s: result=%d", sr_startup_menu_label, result);
        return result;
    }
    // Re-entry: game looped back after a sub-dialog (e.g., file browser cancel).
    // Re-parse the menu and show our modal again instead of the inaccessible original.
    if (!sr_startup_menu_pending && sr_is_available() && sr_startup_menu_label[0]) {
        sr_popup_list_parse("SCRIPT", sr_startup_menu_label);
        if (sr_popup_list.count > 0) {
            const char* title = sr_menu_name(sr_startup_menu_label);
            int result = sr_accessible_menu_modal(
                (title && title[0]) ? title : sr_startup_menu_label,
                sr_popup_list.items, sr_popup_list.count);
            sr_popup_list_clear();
            sr_debug_log("MENU-MODAL re-entry %s: result=%d",
                sr_startup_menu_label, result);
            return result;
        }
        sr_popup_list_clear();
    }
    sr_startup_menu_pending = false;
    return OrigMenuRunner(ContainerThis, WinObj, flag, extra);
}

// Trampoline for 0x4ADAF0 — the "variant" menu runner used by SCENARIOMENU/MULTIMENU.
// These menus are called via 0x4ADAF0 instead of 0x4ADB20 (which TOPMENU/MAPMENU use).
static uint8_t* sr_menu_variant_trampoline = NULL;

/// Allocate trampoline memory and copy the original prologue of 0x4ADAF0
/// before write_jump overwrites it. Must be called from patch_setup().
void sr_menu_variant_init_trampoline() {
    // Original bytes at 0x4ADAF0: 55 8B EC 8B 45 0C (6 bytes, 3 complete instructions)
    // push ebp; mov ebp, esp; mov eax, [ebp+0xc]
    sr_menu_variant_trampoline = (uint8_t*)VirtualAlloc(
        NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!sr_menu_variant_trampoline) {
        debug("sr_menu_variant_init_trampoline: VirtualAlloc FAILED\n");
        return;
    }
    // Copy first 6 bytes of original function
    memcpy(sr_menu_variant_trampoline, (void*)0x4ADAF0, 6);
    // Append JMP to 0x4ADAF6 (rest of original function)
    sr_menu_variant_trampoline[6] = 0xE9;
    int32_t jmp_target = 0x4ADAF6 - ((int32_t)(sr_menu_variant_trampoline + 6) + 5);
    *(int32_t*)(sr_menu_variant_trampoline + 7) = jmp_target;
}

/// Hook for 0x4ADAF0 — intercepts SCENARIOMENU/MULTIMENU (and any other
/// menus using this code path) for accessible modal replacement.
int __thiscall mod_menu_variant_runner(
    void* ContainerThis, const char* label, int arg2, int arg3)
{
    if (sr_is_available() && label && sr_is_startup_menu(label)) {
        sr_popup_list_parse("SCRIPT", label);
        if (sr_popup_list.count > 0) {
            const char* title = sr_menu_name(label);
            int result = sr_accessible_menu_modal(
                (title && title[0]) ? title : label,
                sr_popup_list.items, sr_popup_list.count);
            sr_popup_list_clear();
            sr_debug_log("MENU-VARIANT %s: result=%d", label, result);
            return result;
        }
        sr_popup_list_clear();
    }
    // Not a startup menu or SR not available — call original via trampoline
    if (sr_menu_variant_trampoline) {
        typedef int (__thiscall *FMenuVariant)(void*, const char*, int, int);
        FMenuVariant orig = (FMenuVariant)sr_menu_variant_trampoline;
        return orig(ContainerThis, label, arg2, arg3);
    }
    // Trampoline failed — cannot call original, return -1 (cancel)
    return -1;
}

int __thiscall mod_BasePop_start(
void* This, const char* filename, const char* label, int a4, int a5, int a6, int a7)
{
    if (filename && label) {
        sr_debug_log("BASEPOP file=%s label=%s popup_active=%d",
            filename, label, sr_popup_is_active());
        if (CouncilHandler::IsActive()) {
            CouncilHandler::OnPopupOpened();
            // Mark BUYVOTEMENU popups for accessible modal replacement.
            // Labels: BUYVOTEYEA0-4, BUYVOTENAY0-4, BUYVOTEABSTAIN0-4, BUYVOTEGOV0-4
            if (CouncilHandler::InBuyVotes()
                && _strnicmp(label, "BUYVOTE", 7) == 0) {
                CouncilHandler::SetBuyMenuPending(true);
                sr_debug_log("BASEPOP: BUYVOTEMENU detected (%s), flagged for modal", label);
            }
        }
        // Detect AI-called council: CALLSCOUNCIL popup appears when an AI
        // faction calls a council vote. Activate council tracking so our
        // hooks handle the human player's vote.
        if (_stricmp(label, "CALLSCOUNCIL") == 0 && !CouncilHandler::IsActive()) {
            sr_debug_log("BASEPOP: AI called council (CALLSCOUNCIL), activating");
            CouncilHandler::OnAICouncilCalled();
        }
        // Save popup pointer for editbox dialogs (NETCONNECT_CREATE/JOIN)
        if (_stricmp(label, "NETCONNECT_CREATE") == 0
            || _stricmp(label, "NETCONNECT_JOIN") == 0) {
            sr_editbox_popup = This;
            sr_debug_log("EDITBOX popup This=%p", This);
        }
        // Detect faction selection screen: BLURB label from faction files
        // Note: OnBlurbDetected (with text) is called in mod_BasePop_end
        if (_stricmp(label, "BLURB") == 0 && current_window() == GW_None) {
            // Nothing here — handler activates in OnBlurbDetected
        } else if (FactionSelectHandler::IsActive()) {
            // Any non-BLURB popup means we left the faction select screen
            FactionSelectHandler::OnNonBlurbPopup();
        }
    }
    // Clear stale popup list from previous BasePop_start dialog.
    // Only when not inside a popp/popb wrapper (which handles its own list).
    if (!sr_popup_is_active()) {
        sr_popup_list_clear();
    }
    // Flag startup menus for modal replacement in mod_startup_menu_runner.
    // We let BasePop_start run normally (Win object must be fully constructed
    // for destructors to work). The actual menu display call (0x4ADB20) is
    // patched to check this flag and show our accessible modal instead.
    // Note: Only TOPMENU/MAPMENU use the 0x4ADB20 path and need flagging.
    // SCENARIOMENU/MULTIMENU use 0x4ADAF0 (mod_menu_variant_runner) directly.
    if (!sr_popup_is_active() && sr_is_available() && filename && label
        && (_stricmp(label, "TOPMENU") == 0 || _stricmp(label, "MAPMENU") == 0
            || _stricmp(label, "USERULES") == 0)) {
        sr_popup_list_parse(filename, label);
        if (sr_popup_list.count > 0) {
            sr_startup_menu_pending = true;
            strncpy(sr_startup_menu_label, label, sizeof(sr_startup_menu_label) - 1);
            sr_startup_menu_label[sizeof(sr_startup_menu_label) - 1] = '\0';
            sr_debug_log("MENU-FLAG: %s flagged for modal (%d items)",
                label, sr_popup_list.count);
        } else {
            sr_popup_list_clear();
        }
    }
    if (!sr_popup_is_active() && sr_is_available() && filename && label) {
        // Suppress multiplayer sync progress popups (rapid-fire, not useful)
        if (strncmp(label, "SYNCH", 5) == 0) {
            return BasePop_start(This, filename, label, a4, a5, a6, a7);
        }
        // Suppress COUNCILISSUES popup for AI-called councils.
        // The AI's proposal selection is not relevant to the human player.
        if (_stricmp(label, "COUNCILISSUES") == 0
            && CouncilHandler::IsActive() && !CouncilHandler::IsCallerHuman()) {
            return BasePop_start(This, filename, label, a4, a5, a6, a7);
        }
        const char* menu = sr_menu_name(label);
        if (menu) {
            // Menu: announce just the name (or read #caption from file)
            if (menu[0]) {
                sr_output(menu, true);
            } else {
                // Empty string = read #caption from file
                char buf[2048];
                buf[0] = '\0';
                if (sr_read_popup_text(filename, label, buf, sizeof(buf))) {
                    // Caption is prepended, cut at ". " to drop body
                    char* dot = strstr(buf, ". ");
                    if (dot) *dot = '\0';
                }
                sr_output(buf[0] ? buf : label, true);
            }
        } else {
            // Dialog: announce full popup text (caption + body)
            char buf[2048];
            if (sr_read_popup_text(filename, label, buf, sizeof(buf))) {
                // Faction select: handler builds "FactionName. BLURB" announcement
                if (_stricmp(label, "BLURB") == 0
                    && current_window() == GW_None) {
                    char combined[2048];
                    if (FactionSelectHandler::OnBlurbDetected(
                            filename, buf, combined, sizeof(combined))) {
                        sr_output(combined, true);
                    }
                } else {
                    // Fix incorrect key hint in end-of-turn popups:
                    // Game says "EINGABE"/"Enter" but it's actually Ctrl+Enter.
                    if (_strnicmp(label, "ENDOFTURN", 9) == 0
                        || _stricmp(label, "ENDYOURTURN") == 0) {
                        // Fix: game says "EINGABE"/"Enter" but it's actually Ctrl+Enter
                        char* p = strstr(buf, "EINGABE");
                        if (p && (int)strlen(buf) + 5 < 2048) {
                            int rest = (int)strlen(p);
                            memmove(p + 5, p, rest + 1);
                            memcpy(p, "Strg+", 5);
                        }
                        p = strstr(buf, "Enter");
                        if (p && (int)strlen(buf) + 5 < 2048) {
                            int rest = (int)strlen(p);
                            memmove(p + 5, p, rest + 1);
                            memcpy(p, "Ctrl+", 5);
                        }
                    }
                    // Buy-votes popups: queue so offer text finishes before modal prompt
                    bool buy_interrupt = !(CouncilHandler::InBuyVotes() && label
                        && _strnicmp(label, "BUYVOTE", 7) == 0);
                    sr_output(buf, buy_interrupt);
                }
            }
            // File browser confirmation dialogs: append key hint
            // (buttons are not arrow-navigable, Enter=OK, Escape=Cancel)
            if (sr_file_browser_active() && label
                && strcmp(label, "FILEWIN_SAVEEXISTS") == 0) {
                sr_output(loc(SR_FILE_OVERWRITE_HINT), false);
            }
            // Parse popup list for dialogs that bypass popp/popb
            sr_popup_list_parse(filename, label);
            // Council: GOVVOTE has dynamic candidates — build from eligible()
            if (_stricmp(label, "GOVVOTE") == 0 && CouncilHandler::IsActive()) {
                CouncilHandler::OnGovVotePopup();
            }
        }
    }
    if (movedlabels.count(label)) {
        return BasePop_start(This, "modmenu", label, a4, a5, a6, a7);
    }
    return BasePop_start(This, filename, label, a4, a5, a6, a7);
}

/*
Modify Unit Workshop command to open prototype for the currently selected unit.
Normally unit_id can be set when ASKSEEDESIGN popup is opened from tech_achieved.
*/
int __cdecl mod_design_new_veh(int faction_id, int unit_id) {
    if (unit_id < 0 && MapWin->iUnit >= 0) {
        VEH* veh = &Vehs[MapWin->iUnit];
        if (((veh->unit_id < MaxProtoFactionNum
        && has_tech(Units[veh->unit_id].preq_tech, faction_id))
        || veh->unit_id / MaxProtoFactionNum == faction_id)
        && Units[veh->unit_id].icon_offset < 0
        && veh->x == MapWin->iTileX && veh->y == MapWin->iTileY) {
            return DesignWin_exec(DesignWin, faction_id, veh->unit_id);
        }
    }
    return DesignWin_exec(DesignWin, faction_id, unit_id);
}

int __cdecl mod_action_arty(int veh_id, int x, int y)
{
    VEH* veh = &Vehs[veh_id];
    if (*MultiplayerActive) {
        return action_arty(veh_id, x, y);
    }
    if (veh->faction_id == *CurrentPlayerFaction) {
        if (!veh_ready(veh_id)) {
            MessageHandler::OnMessage("UNITMOVED", 0);
            return NetMsg_pop(NetMsg, "UNITMOVED", 5000, 0, 0);
        }
    }
    int veh_range = arty_range(veh->unit_id);
    if (map_range(veh->x, veh->y, x, y) <= veh_range) {
        int veh_id_tgt = stack_fix(veh_at(x, y));
        if (veh_id_tgt >= 0) {
            if (veh->faction_id != Vehs[veh_id_tgt].faction_id
            && !has_pact(veh->faction_id, Vehs[veh_id_tgt].faction_id)) {
                int offset = radius_move2(veh->x, veh->y, x, y, TableRange[veh_range]);
                if (offset >= 0) {
                    *VehAttackFlags = 3;
                    return battle_fight_1(veh_id, offset, 1, 1, 0);
                }
            }
        } else {
            return action_destroy(veh_id, 0, x, y);
        }
    } else {
        MessageHandler::OnMessage("OUTOFRANGE", 0);
        return NetMsg_pop(NetMsg, "OUTOFRANGE", 5000, 0, 0);
    }
    return 0;
}

int __cdecl MapWin_right_menu_arty(int veh_id, int x, int y)
{
    VEH* veh = &Vehs[veh_id];
    return map_range(veh->x, veh->y, x, y) <= arty_range(veh->unit_id);
}

void __thiscall Console_arty_cursor_on(Console* This, int cursor_type, int veh_id)
{
    int veh_range = arty_range(Vehs[veh_id].unit_id);
    Console_cursor_on(This, cursor_type, veh_range);
}





