
#include "gui.h"
#include "base_handler.h"
#include "social_handler.h"
#include "localization.h"

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

static int hurry_minimal_cost = 0;
static int base_zoom_factor = -14;

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
static GameWinState current_window() {
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

static void base_resource_zoom(bool zoom_in) {
    base_zoom_factor = clamp(base_zoom_factor + (zoom_in ? 2 : -2), -14, 0);
    GraphicWin_redraw(BaseWin);
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

// Announce tile info at given coordinates via screen reader.
// Reusable for both MAP-MOVE tracking and virtual exploration cursor.
static void sr_announce_tile(int x, int y) {
    if (!sr_is_available()) return;
    if (*MapAreaX <= 0 || *MapAreaY <= 0) return;
    if (x < 0 || x >= *MapAreaX || y < 0 || y >= *MapAreaY) {
        sr_output(loc(SR_MAP_EDGE), true);
        return;
    }
    MAP* sq = mapsq(x, y);
    if (!sq) return;

    int faction = *CurrentPlayerFaction;
    char buf[512];
    int pos = 0;

    // Coordinates
    pos += snprintf(buf + pos, sizeof(buf) - pos, "(%d, %d) ", x, y);

    // Unexplored check
    if (!sq->is_visible(faction)) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TILE_UNEXPLORED));
        sr_debug_log("TILE-ANNOUNCE (%d,%d): %s", x, y, buf);
        sr_output(buf, true);
        return;
    }

    // Terrain type
    int alt = sq->alt_level();
    bool is_land = (alt >= ALT_SHORE_LINE);
    if (alt == ALT_OCEAN_TRENCH) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_TRENCH));
    } else if (alt <= ALT_OCEAN) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_OCEAN));
    } else if (alt == ALT_OCEAN_SHELF) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_SHELF));
    } else {
        if (sq->is_rocky())
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_ROCKY));
        else if (sq->is_rolling())
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_ROLLING));
        else
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_TERRAIN_FLAT));
    }

    // Moisture (land only)
    if (is_land) {
        if (sq->is_rainy())
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_TERRAIN_RAINY));
        else if (sq->is_moist())
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_TERRAIN_MOIST));
        else if (sq->is_arid())
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_TERRAIN_ARID));
    }

    // Altitude (land only, elevated = 2+ above sea level)
    if (is_land && alt >= ALT_TWO_ABOVE_SEA) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_TERRAIN_HIGH));
    }

    // Features
    if (sq->is_fungus())
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_FUNGUS));
    if (sq->items & BIT_FOREST)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_FOREST));
    if (sq->items & BIT_RIVER)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_RIVER));
    if (sq->items & BIT_FARM)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_FARM));
    if (sq->items & BIT_SOIL_ENRICHER)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_SOIL_ENRICHER));
    if (sq->items & BIT_MINE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_MINE));
    if (sq->items & BIT_SOLAR)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_SOLAR));
    if (sq->items & BIT_CONDENSER)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_CONDENSER));
    if (sq->items & BIT_ECH_MIRROR)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_ECH_MIRROR));
    if (sq->items & BIT_THERMAL_BORE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_BOREHOLE));
    if (sq->items & BIT_ROAD)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_ROAD));
    if (sq->items & BIT_MAGTUBE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_MAGTUBE));
    if (sq->items & BIT_BUNKER)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_BUNKER));
    if (sq->items & BIT_AIRBASE)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_AIRBASE));
    if (sq->items & BIT_SENSOR)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_SENSOR));
    if (sq->items & BIT_SUPPLY_POD)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_SUPPLY_POD));
    if (sq->items & BIT_MONOLITH)
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s", loc(SR_FEATURE_MONOLITH));

    // Resource yields
    int food = mod_crop_yield(faction, -1, x, y, 0);
    int mins = mod_mine_yield(faction, -1, x, y, 0);
    int energy = mod_energy_yield(faction, -1, x, y, 0);
    pos += snprintf(buf + pos, sizeof(buf) - pos, ". ");
    pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_TILE_YIELDS), food, mins, energy);

    // Ownership
    if (sq->owner >= 0 && sq->owner < MaxPlayerNum) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ". ");
        pos += snprintf(buf + pos, sizeof(buf) - pos, loc(SR_TILE_OWNER),
            MFactions[sq->owner].noun_faction);
    } else {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ". %s", loc(SR_TILE_UNOWNED));
    }

    // City radius
    if (sq->items & BIT_BASE_RADIUS) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ". %s", loc(SR_TILE_IN_RADIUS));
    }

    // Base on tile
    int base_id = base_at(x, y);
    if (base_id >= 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            loc(SR_BASE_AT), Bases[base_id].name);
    }

    // Units on this tile
    if (Vehs && *VehCount > 0) {
        int unit_count = 0;
        for (int i = 0; i < *VehCount && unit_count < 3; i++) {
            if (Vehs[i].x == x && Vehs[i].y == y) {
                if (unit_count == 0) {
                    pos += snprintf(buf + pos, sizeof(buf) - pos,
                        loc(SR_UNIT_AT), Vehs[i].name());
                } else {
                    pos += snprintf(buf + pos, sizeof(buf) - pos,
                        ", %s", Vehs[i].name());
                }
                unit_count++;
            }
        }
        if (unit_count > 0) {
            // Count remaining
            int remaining = 0;
            for (int i = 0; i < *VehCount; i++) {
                if (Vehs[i].x == x && Vehs[i].y == y) remaining++;
            }
            if (remaining > unit_count) {
                pos += snprintf(buf + pos, sizeof(buf) - pos,
                    loc(SR_MORE_UNITS), remaining - unit_count);
            }
        }
    }

    sr_debug_log("TILE-ANNOUNCE (%d,%d): %s", x, y, buf);
    sr_output(buf, true);
}

// Menu bar: live data access helpers.
// Reads directly from MapWin->oMainMenu at runtime.
// This ensures correct order, current items (F11 toggle), and valid IDs.

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

/// Get the number of VISIBLE items in a submenu (respects F11 simple/detailed toggle).
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

// Scanner mode: jump to points of interest on the world map
static int sr_scan_filter = 0;
static const int SR_SCAN_FILTER_COUNT = 10;

static const SrStr sr_scan_filter_names[SR_SCAN_FILTER_COUNT] = {
    SR_SCAN_ALL, SR_SCAN_OWN_BASES, SR_SCAN_ENEMY_BASES,
    SR_SCAN_ENEMY_UNITS, SR_SCAN_OWN_UNITS, SR_SCAN_OWN_FORMERS,
    SR_SCAN_FUNGUS, SR_SCAN_PODS, SR_SCAN_IMPROVEMENTS, SR_SCAN_NATURE,
};

/// Check if tile (x,y) matches the current scanner filter for given faction.
static bool sr_scan_matches(int x, int y, int filter, int faction) {
    MAP* sq = mapsq(x, y);
    if (!sq || !sq->is_visible(faction)) return false;

    switch (filter) {
    case 0: // All non-empty
        return (sq->items & (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE | BIT_FARM | BIT_MINE
            | BIT_SOLAR | BIT_ROAD | BIT_MAGTUBE | BIT_FOREST | BIT_RIVER | BIT_FUNGUS
            | BIT_CONDENSER | BIT_ECH_MIRROR | BIT_SOIL_ENRICHER | BIT_THERMAL_BORE
            | BIT_BUNKER | BIT_AIRBASE | BIT_SENSOR | BIT_SUPPLY_POD | BIT_MONOLITH)) != 0;
    case 1: // Own bases
        return (sq->items & BIT_BASE_IN_TILE) && sq->owner == faction;
    case 2: // Enemy bases
        return (sq->items & BIT_BASE_IN_TILE) && sq->owner != faction && sq->owner >= 0;
    case 3: // Enemy units
        if (Vehs && *VehCount > 0) {
            for (int i = 0; i < *VehCount; i++) {
                if (Vehs[i].x == x && Vehs[i].y == y && Vehs[i].faction_id != faction)
                    return true;
            }
        }
        return false;
    case 4: // Own units
        if (Vehs && *VehCount > 0) {
            for (int i = 0; i < *VehCount; i++) {
                if (Vehs[i].x == x && Vehs[i].y == y && Vehs[i].faction_id == faction)
                    return true;
            }
        }
        return false;
    case 5: // Own formers
        if (Vehs && *VehCount > 0) {
            for (int i = 0; i < *VehCount; i++) {
                if (Vehs[i].x == x && Vehs[i].y == y && Vehs[i].faction_id == faction
                    && Vehs[i].is_former())
                    return true;
            }
        }
        return false;
    case 6: // Fungus
        return sq->is_fungus();
    case 7: // Supply pods / Monoliths
        return (sq->items & (BIT_SUPPLY_POD | BIT_MONOLITH)) != 0;
    case 8: // Improvements
        return (sq->items & (BIT_FARM | BIT_MINE | BIT_SOLAR | BIT_ROAD | BIT_MAGTUBE
            | BIT_SENSOR | BIT_BUNKER | BIT_AIRBASE | BIT_CONDENSER | BIT_ECH_MIRROR
            | BIT_SOIL_ENRICHER | BIT_THERMAL_BORE)) != 0;
    case 9: // Terrain & Nature
        return (sq->items & (BIT_RIVER | BIT_FOREST | BIT_FUNGUS)) != 0
            || sq->is_fungus();
    }
    return false;
}

/// Advance to next valid tile coordinate (row by row, left to right).
/// Returns false if wrapped back to start position.
static bool sr_scan_next_tile(int* x, int* y, int start_x, int start_y) {
    *x += 2;
    if (*x >= *MapAreaX) {
        *y += 1;
        if (*y >= *MapAreaY) {
            *y = 0;
        }
        *x = (*y % 2 == 0) ? 0 : 1;
    }
    return !(*x == start_x && *y == start_y);
}

/// Go to previous valid tile coordinate (row by row, right to left).
/// Returns false if wrapped back to start position.
static bool sr_scan_prev_tile(int* x, int* y, int start_x, int start_y) {
    *x -= 2;
    if (*x < 0) {
        *y -= 1;
        if (*y < 0) {
            *y = *MapAreaY - 1;
        }
        // Largest valid x in row: x+y must be even, x < MapAreaX
        *x = *MapAreaX - 1;
        if ((*x + *y) % 2 != 0) *x -= 1;
    }
    return !(*x == start_x && *y == start_y);
}

LRESULT WINAPI ModWinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    const bool debug_cmd = DEBUG && !*GameHalted && msg == WM_CHAR;
    const bool is_editor = !*GameHalted
        && *GameState & STATE_SCENARIO_EDITOR && *GameState & STATE_OMNISCIENT_VIEW;
    static int delta_accum = 0;
    static bool sr_initialized = false;
    POINT p;
    MAP* sq;

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

    // === Screen reader: unified announce system ===

    // Early in_menu check for HUD filter (full in_menu computed later too)
    bool in_menu_early = (current_window() == GW_None
        && *PopupDialogState == 0);

    // HUD bar noise filter: status bar items that should not be auto-announced.
    // These are drawn character-by-character and pollute all triggers.
    // Available for on-demand readback in the future.
    auto sr_is_hud_noise = [in_menu_early](const char* text) -> bool {
        if (!text) return true;
        // HUD bar items (drawn character-by-character)
        // Short "Mission Year" = HUD/info panel noise.
        // Long "Mission Year NNNN: message" = game status → let through.
        if (strncmp(text, "Mission Year", 12) == 0 && strlen(text) < 30) return true;
        if (strncmp(text, "Econ:", 5) == 0) return true;
        if (strncmp(text, "Psych:", 6) == 0) return true;
        if (strncmp(text, "Labs:", 5) == 0) return true;
        // Info panel items (redundant with MAP-MOVE tracker)
        if (strncmp(text, "Energy:", 7) == 0) return true;
        if (strncmp(text, "Unexplored", 10) == 0) return true;
        if (text[0] == '(' && strstr(text, " , ") != NULL) return true;
        if (strncmp(text, "Elev:", 5) == 0) return true;
        if (strcmp(text, "ENDANGERED") == 0) return true;
        if (strncmp(text, "(Gov:", 5) == 0) return true;
        // Terrain descriptors (handled by MAP-MOVE)
        if (strstr(text, "Rolling") != NULL && strlen(text) < 20) return true;
        if (strstr(text, "Flat") != NULL && strlen(text) < 20) return true;
        if (strstr(text, "Rocky") != NULL && strlen(text) < 20) return true;
        if (strstr(text, "Rainy") != NULL && strlen(text) < 20) return true;
        if (strstr(text, "Arid") != NULL && strlen(text) < 15) return true;
        if (strstr(text, "Moist") != NULL && strlen(text) < 15) return true;
        if (strstr(text, "Xenofungus") != NULL) return true;
        // Economic panel (world map overlay, drawn char-by-char)
        if (strncmp(text, "Commerce", 8) == 0) return true;
        if (strncmp(text, "Gross", 5) == 0) return true;
        if (strncmp(text, "Total Cost", 10) == 0) return true;
        if (strncmp(text, "NET ", 4) == 0) return true;
        // Partial HUD build-up (character-by-character: "Mis", "Miss", etc.)
        if (strncmp(text, "Mis", 3) == 0 && strlen(text) < 12) return true;
        if (strncmp(text, "Eco", 3) == 0 && strlen(text) < 5) return true;
        if (strncmp(text, "Psy", 3) == 0 && strlen(text) < 6) return true;
        if (strncmp(text, "Lab", 3) == 0 && strlen(text) < 5) return true;
        // Menu bar items (top of world map screen)
        // Skip in menu context: these words can be real menu items there
        if (!in_menu_early) {
            if (strcmp(text, "GAME") == 0) return true;
            if (strcmp(text, "NETWORK") == 0) return true;
            if (strcmp(text, "ACTION") == 0) return true;
            if (strcmp(text, "TERRAFORM") == 0) return true;
            if (strcmp(text, "SCENARIO") == 0) return true;
            if (strcmp(text, "EDIT MAP") == 0) return true;
            if (strcmp(text, "HELP") == 0) return true;
        }
        return false;
    };

    //
    static char sr_announced[2048] = "";
    static DWORD sr_last_seen_record = 0;
    static DWORD sr_stable_since = 0;
    // Virtual exploration cursor (independent of game cursor)
    static int sr_cursor_x = -1;
    static int sr_cursor_y = -1;
    static bool sr_arrow_active = false;
    static WPARAM sr_arrow_dir = 0;  // VK_UP or VK_DOWN
    static DWORD sr_arrow_time = 0;  // tick when arrow was pressed
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

        // --- World map position tracking ---
        // Active when world map is focused (GW_World) OR during gameplay (popup >= 2)
        static int sr_map_x = -999;
        static int sr_map_y = -999;
        static DWORD sr_map_change_time = 0;
        static bool sr_map_pending = false;
        bool on_world_map = MapWin &&
            (cur_win == GW_World || (cur_win == GW_None && cur_popup >= 2));
        // Menu/dialog context: arrow-navigated screens where auto-announce is noise.
        // User navigates with arrows (Trigger 2). Popups handled by popup hooks.
        bool in_menu = (cur_win == GW_None && cur_popup == 0);

        // Invalidate menu cache when leaving menu context
        if (!in_menu && sr_mcache.active) {
            sr_debug_log("MCACHE invalidated (left menu)");
            sr_mcache.active = false;
        }

        // Debug helper: log mcache events to file
        auto mclog = [](const char* fmt, ...) {
            FILE* f = fopen("mcache_debug.txt", "a");
            if (!f) return;
            va_list ap;
            va_start(ap, fmt);
            vfprintf(f, fmt, ap);
            va_end(ap);
            fprintf(f, "\n");
            fclose(f);
        };

        // Announce world map transition (spoken)
        static bool sr_prev_on_world_map = false;
        if (on_world_map && !sr_prev_on_world_map) {
            sr_debug_log("ANNOUNCE-SCREEN: World Map");
            sr_output(loc(SR_WORLD_MAP), true);
            sr_announced[0] = '\0';
            // Initialize exploration cursor to current unit position
            if (MapWin->iUnit >= 0 && Vehs && *VehCount > 0
                && MapWin->iUnit < *VehCount) {
                sr_cursor_x = Vehs[MapWin->iUnit].x;
                sr_cursor_y = Vehs[MapWin->iUnit].y;
            } else {
                sr_cursor_x = MapWin->iTileX;
                sr_cursor_y = MapWin->iTileY;
            }
            // Initialize map tracker to current position so no initial
            // tile announcement fires — only announce on actual movement
            sr_map_x = MapWin->iTileX;
            sr_map_y = MapWin->iTileY;
            sr_map_pending = false;
            sr_debug_log("CURSOR-INIT (%d,%d)", sr_cursor_x, sr_cursor_y);
        }
        if (!on_world_map && sr_menubar_active) {
            sr_menubar_active = false;
            sr_submenu_active = false;
        }
        sr_prev_on_world_map = on_world_map;

        if (on_world_map && *MapAreaX > 0 && *MapAreaY > 0) {
            int mx = MapWin->iTileX;
            int my = MapWin->iTileY;
            // Skip if coordinates look uninitialized/garbage
            if (mx < 0 || mx >= *MapAreaX || my < 0 || my >= *MapAreaY) {
                sr_map_x = -999;
                sr_map_y = -999;
            } else if (mx != sr_map_x || my != sr_map_y) {
                sr_map_x = mx;
                sr_map_y = my;
                sr_map_change_time = now;
                sr_map_pending = true;
            }
            if (sr_map_pending && (now - sr_map_change_time) > 150) {
                sr_map_pending = false;
                sr_announce_tile(sr_map_x, sr_map_y);
            }
        } else {
            sr_map_x = -999;
            sr_map_y = -999;
        }

        // --- Player turn / unit selection detection ---
        // When the game selects a unit for the player, announce "Your Turn".
        static int sr_prev_unit = -1;
        if (on_world_map && MapWin && Vehs && *VehCount > 0) {
            int cur_unit = MapWin->iUnit;
            if (cur_unit != sr_prev_unit) {
                sr_debug_log("UNIT-CHANGE: prev=%d cur=%d VehCount=%d",
                    sr_prev_unit, cur_unit, *VehCount);
                if (cur_unit >= 0 && cur_unit < *VehCount) {
                    VEH* veh = &Vehs[cur_unit];
                    sr_debug_log("UNIT-CHECK: faction=%d owner=%d unit_id=%d",
                        (int)veh->faction_id, (int)MapWin->cOwner,
                        (int)veh->unit_id);
                    if (veh->faction_id == MapWin->cOwner
                        && veh->unit_id >= 0) {
                        // Update cursor to new unit position
                        sr_cursor_x = veh->x;
                        sr_cursor_y = veh->y;
                        int total_speed = veh_speed(cur_unit, 0);
                        int remaining = max(0, total_speed - (int)veh->moves_spent);
                        int disp_rem = remaining / Rules->move_rate_roads;
                        int disp_tot = total_speed / Rules->move_rate_roads;
                        char buf[256];
                        char move_buf[64];
                        snprintf(move_buf, sizeof(move_buf),
                            loc(SR_MOVEMENT_POINTS), disp_rem, disp_tot);
                        snprintf(buf, sizeof(buf), "%s. %s",
                            loc(SR_YOUR_TURN), move_buf);
                        // Fill in format args for SR_YOUR_TURN
                        char full[320];
                        snprintf(full, sizeof(full), buf,
                            veh->name(), veh->x, veh->y);
                        sr_debug_log("ANNOUNCE-TURN: %s", full);
                        sr_output(full, true);
                    }
                }
                sr_prev_unit = cur_unit;
            }
        } else {
            sr_prev_unit = -1;
        }

        // --- Key tracking: reset capture on every significant keypress ---
        if (msg == WM_KEYDOWN) {
            sr_debug_log("KEY 0x%X popup=%d items=%d win=%d",
                (unsigned)wParam, *PopupDialogState, sr_item_count(),
                (int)current_window());

            // Popup list navigation (research priorities, etc.)
            if ((wParam == VK_UP || wParam == VK_DOWN)
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
            } else if (wParam == VK_UP || wParam == VK_DOWN) {
                if (!on_world_map && cur_win != GW_Base
                    && cur_win != GW_Design) {
                    // Activate cache on first arrow press
                    // Start at pos=15 to allow UP navigation without clamping
                    if (!sr_mcache.active) {
                        memset(&sr_mcache, 0, sizeof(sr_mcache));
                        sr_mcache.pos = 15;
                        sr_mcache.active = true;
                        mclog("MCACHE activated at pos=15");
                    }

                    // Update position
                    if (wParam == VK_DOWN) {
                        sr_mcache.pos++;
                        if (sr_mcache.pos >= 32) sr_mcache.pos = 31;
                    } else {
                        sr_mcache.pos--;
                        if (sr_mcache.pos < 0) sr_mcache.pos = 0;
                    }

                    // Check cache for instant announce
                    if (sr_mcache.items[sr_mcache.pos][0] != '\0') {
                        mclog("HIT pos=%d: %s",
                            sr_mcache.pos, sr_mcache.items[sr_mcache.pos]);
                        sr_output(sr_mcache.items[sr_mcache.pos], true);
                        // Skip Trigger 2 for this press
                        sr_arrow_active = false;
                        sr_arrow_dir = 0;
                        sr_arrow_time = 0;
                    } else {
                        // Cache miss: fall through to Trigger 2
                        sr_arrow_active = true;
                        sr_arrow_dir = wParam;
                        sr_arrow_time = now;
                        mclog("MISS pos=%d, waiting for T2",
                            sr_mcache.pos);
                    }
                    sr_items_clear();
                    sr_clear_text();
                    sr_snapshot_consume();
                    sr_last_seen_record = 0;
                    sr_stable_since = 0;
                    mclog("ARROW %s pos=%d",
                        wParam == VK_DOWN ? "DOWN" : "UP",
                        sr_mcache.pos);
                }
            } else if (wParam != VK_LEFT && wParam != VK_RIGHT
                       && wParam != VK_SHIFT && wParam != VK_CONTROL
                       && wParam != VK_MENU) {
                sr_arrow_active = false;
                sr_arrow_dir = 0;
                // Invalidate cache on non-navigation keys
                if (sr_mcache.active) {
                    sr_debug_log("MCACHE invalidated (key 0x%X)",
                        (unsigned)wParam);
                    sr_mcache.active = false;
                }
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
        // Skip all announce triggers while sr_defer_active (planetfall etc.)
        if (sr_snapshot_ready()) {
            if (sr_tutorial_announce_time > 0
                && (now - sr_tutorial_announce_time) < 3000) {
                sr_debug_log("SNAP-DISCARD (tutorial recent)");
            } else if (in_menu) {
                sr_debug_log("SNAP-DISCARD (menu context)");
            } else if (sr_arrow_active) {
                sr_debug_log("SNAP-DISCARD (arrow)");
            } else {
                int sn = sr_snapshot_count();
                if (sn > 10) {
                    // Too many items (e.g. base screen buttons) — skip,
                    // screen transition announcement handles orientation.
                    sr_debug_log("SNAP-DISCARD (count=%d)", sn);
                } else if (sn > 0) {
                    // Only announce NEW non-HUD items
                    char buf[2048];
                    int pos = 0;
                    char all[2048];
                    int apos = 0;
                    bool has_new = false;
                    for (int i = 0; i < sn; i++) {
                        const char* it = sr_snapshot_get(i);
                        if (!it || strlen(it) < 3) continue;
                        if (sr_is_hud_noise(it)) continue;
                        int len = strlen(it);
                        if (apos + len + 3 < (int)sizeof(all)) {
                            if (apos > 0) { all[apos++] = '.'; all[apos++] = ' '; }
                            memcpy(all + apos, it, len);
                            apos += len;
                        }
                        if (strstr(sr_announced, it) == NULL) {
                            has_new = true;
                            if (pos + len + 3 < (int)sizeof(buf)) {
                                if (pos > 0) { buf[pos++] = '.'; buf[pos++] = ' '; }
                                memcpy(buf + pos, it, len);
                                pos += len;
                            }
                        }
                    }
                    buf[pos] = '\0';
                    all[apos] = '\0';
                    if (has_new) {
                        strncpy(sr_announced, all, sizeof(sr_announced) - 1);
                        sr_announced[sizeof(sr_announced) - 1] = '\0';
                        sr_debug_log("ANNOUNCE-SNAP(%d): %s", sn, buf);
                        sr_output(buf, true);
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
        if (sr_arrow_active && sr_arrow_time > 0 && !on_world_map) {
            int count = sr_item_count();
            DWORD elapsed = now - sr_arrow_time;
            if ((count >= 2 && elapsed > 50) || (count == 1 && elapsed > 200)) {
                // Find best nav target: skip HUD noise items
                int nav_idx = -1;
                if (count >= 2) {
                    // Normal: item[0]=leaving, item[1]=entering
                    // Boundary: item[0]=current, item[1]=HUD noise
                    const char* it1 = sr_item_get(1);
                    if (!sr_is_hud_noise(it1)) {
                        nav_idx = 1;
                    } else {
                        sr_debug_log("NAV-BOUNDARY item[1] is HUD, using item[0]");
                        nav_idx = 0;
                    }
                } else {
                    nav_idx = 0;
                }
                const char* it = sr_item_get(nav_idx);
                // Log all captured items for debugging
                if (sr_mcache.active) {
                    mclog("T2: count=%d nav_idx=%d dir=%s pos=%d",
                        count, nav_idx,
                        sr_arrow_dir == VK_DOWN ? "DN" : "UP",
                        sr_mcache.pos);
                    for (int di = 0; di < count && di < 5; di++) {
                        const char* dit = sr_item_get(di);
                        mclog("  item[%d]: %s%s", di,
                            dit ? dit : "(null)",
                            (dit && sr_is_hud_noise(dit)) ? " [HUD]" : "");
                    }
                }
                if (it && strlen(it) >= 3 && !sr_is_hud_noise(it)) {
                    // Cache entering item at current position
                    if (sr_mcache.active && sr_mcache.pos >= 0
                        && sr_mcache.pos < 32) {
                        strncpy(sr_mcache.items[sr_mcache.pos], it,
                            sizeof(sr_mcache.items[0]) - 1);
                        sr_mcache.items[sr_mcache.pos][255] = '\0';
                        mclog("STORE pos=%d: %s", sr_mcache.pos, it);
                    }
                    // Also cache the LEAVING item at previous position
                    if (sr_mcache.active && count >= 2) {
                        int prev_pos = sr_mcache.pos
                            + (sr_arrow_dir == VK_DOWN ? -1 : 1);
                        const char* leaving = sr_item_get(
                            nav_idx == 1 ? 0 : 1);
                        if (prev_pos >= 0 && prev_pos < 32
                            && leaving && strlen(leaving) >= 3
                            && !sr_is_hud_noise(leaving)
                            && sr_mcache.items[prev_pos][0] == '\0') {
                            strncpy(sr_mcache.items[prev_pos], leaving,
                                sizeof(sr_mcache.items[0]) - 1);
                            sr_mcache.items[prev_pos][255] = '\0';
                            mclog("STORE-PREV pos=%d: %s",
                                prev_pos, leaving);
                        }
                    }

                    sr_debug_log("ANNOUNCE-NAV idx=%d/%d dir=%s: %s",
                        nav_idx, count,
                        sr_arrow_dir == VK_DOWN ? "DOWN" : "UP", it);
                    sr_output(it, true);
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
        // Suppress on world map (HUD noise) and in menus (arrow nav handles it).
        if (sr_stable_since > 0 && (now - sr_stable_since) > 300
            && sr_item_count() > 0 && !on_world_map && !sr_arrow_active
            && !in_menu) {
            int count = sr_item_count();
            {
                // Only announce NEW non-HUD items; cap at 5 to prevent walls of text
                char buf[2048];
                int pos = 0;
                char all[2048];
                int apos = 0;
                bool has_new = false;
                int spoken = 0;
                for (int i = 0; i < count; i++) {
                    const char* it = sr_item_get(i);
                    if (!it || strlen(it) < 3) continue;
                    if (sr_is_hud_noise(it)) continue;
                    int len = strlen(it);
                    if (apos + len + 3 < (int)sizeof(all)) {
                        if (apos > 0) { all[apos++] = '.'; all[apos++] = ' '; }
                        memcpy(all + apos, it, len);
                        apos += len;
                    }
                    if (strstr(sr_announced, it) == NULL && spoken < 5) {
                        has_new = true;
                        spoken++;
                        if (pos + len + 3 < (int)sizeof(buf)) {
                            if (pos > 0) { buf[pos++] = '.'; buf[pos++] = ' '; }
                            memcpy(buf + pos, it, len);
                            pos += len;
                        }
                    }
                }
                buf[pos] = '\0';
                all[apos] = '\0';
                if (has_new) {
                    strncpy(sr_announced, all, sizeof(sr_announced) - 1);
                    sr_announced[sizeof(sr_announced) - 1] = '\0';
                    sr_debug_log("ANNOUNCE-TIMER(%d): %s", count, buf);
                    sr_output(buf, true);
                } else {
                    sr_debug_log("SETTLE-TIMER(%d)", count);
                }
            }
            sr_stable_since = 0;
        }

        // --- Trigger 4: World map important message announce ---
        // On the world map, HUD draws char-by-char without gaps, preventing
        // snapshot/timer triggers. Instead of announcing everything, we use a
        // WHITELIST: only announce known important messages (tutorials, turn
        // status). Polls every 500ms.
        static DWORD sr_worldmap_poll_time = 0;
        if (on_world_map && !sr_arrow_active) {
            if (sr_worldmap_poll_time == 0) sr_worldmap_poll_time = now;
            // Suppress worldmap announcements for 3s after tutorial popup
            if (sr_tutorial_announce_time > 0
                && (now - sr_tutorial_announce_time) < 3000) {
                sr_worldmap_poll_time = now; // keep resetting poll timer
            } else if ((now - sr_worldmap_poll_time) >= 500) {
                sr_worldmap_poll_time = now;
                int count = sr_item_count();
                if (count > 0) {
                    // First pass: check if a tutorial popup is open
                    // (any item starts with "ABOUT "). If so, include
                    // all non-HUD items (body text rendered via
                    // Buffer_write_strings doesn't match whitelist).
                    bool has_about = false;
                    for (int i = 0; i < count; i++) {
                        const char* it = sr_item_get(i);
                        if (it && strncmp(it, "ABOUT ", 6) == 0) {
                            has_about = true;
                            break;
                        }
                    }
                    char buf[2048];
                    int pos = 0;
                    bool has_new = false;
                    for (int i = 0; i < count; i++) {
                        const char* it = sr_item_get(i);
                        if (!it || strlen(it) < 3) continue;
                        // Whitelist: only important messages
                        bool important = false;
                        if (strncmp(it, "ABOUT ", 6) == 0) important = true;
                        if (strstr(it, "need new orders") != NULL) important = true;
                        if (strstr(it, "Press ENTER") != NULL) important = true;
                        // Tutorial popup open: include all non-HUD items
                        if (has_about && !sr_is_hud_noise(it)) important = true;
                        if (!important) continue;
                        if (strstr(sr_announced, it) != NULL) continue;
                        has_new = true;
                        int len = strlen(it);
                        if (pos + len + 3 < (int)sizeof(buf)) {
                            if (pos > 0) {
                                buf[pos++] = '.';
                                buf[pos++] = ' ';
                            }
                            memcpy(buf + pos, it, len);
                            pos += len;
                        }
                    }
                    buf[pos] = '\0';
                    if (has_new) {
                        // Append to sr_announced
                        int al = strlen(sr_announced);
                        int bl = strlen(buf);
                        if (al + bl + 3 < (int)sizeof(sr_announced)) {
                            if (al > 0) {
                                sr_announced[al++] = '.';
                                sr_announced[al++] = ' ';
                            }
                            memcpy(sr_announced + al, buf, bl + 1);
                        }
                        sr_debug_log("ANNOUNCE-WORLDMAP: %s", buf);
                        sr_output(buf, true);
                    }
                }
            }
        } else {
            sr_worldmap_poll_time = 0;
        }
    }
    // === End screen reader announce system ===

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

    // Screen reader: Social Engineering handler intercepts ALL messages when modal loop
    // is active. WM_CHAR must also be consumed to prevent the game's WinProc from
    // seeing the translated 'e' keypress and calling social_select inside our loop.
    } else if ((msg == WM_KEYDOWN || msg == WM_CHAR) && sr_is_available()
    && SocialEngHandler::IsActive()) {
        if (msg == WM_KEYDOWN) {
            if (SocialEngHandler::Update(msg, wParam)) return 0;
        }
        return 0; // consume WM_CHAR and unhandled WM_KEYDOWN

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

    // Screen reader: Ctrl+Q = open queue management
    } else if (msg == WM_KEYDOWN && wParam == 'Q' && ctrl_key_down()
    && sr_is_available() && current_window() == GW_Base) {
        if (BaseScreenHandler::Update(msg, wParam)) return 0;

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
                        prod_name(base->item()), cost, f->energy_credits);
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
        show_mod_menu();

    } else if (conf.reduced_mode && msg == WM_CHAR && wParam == 'h' && alt_key_down()) {
        show_mod_menu();

    // Screen reader: Ctrl+F2 activates menu bar navigation (world map only)
    // Reads live data from MapWin->oMainMenu — reflects F11 toggle, correct order.
    // Level 1 (menu bar): Left/Right switch menus, Down/Enter opens submenu
    // Level 2 (submenu): Up/Down navigate items, Left/Right switch to adjacent menu
    // Enter activates item via pMenuHandlerCB(iMenuID) — no mouse clicks needed.
    } else if (msg == WM_KEYDOWN && wParam == VK_F2 && ctrl_key_down()
    && sr_is_available() && sr_get_menu_count() > 0
    && (current_window() == GW_World || (current_window() == GW_None && *PopupDialogState >= 2))) {
        sr_menubar_active = true;
        sr_submenu_active = false;
        sr_menubar_index = 0;
        sr_submenu_index = 0;
        char buf[256], mname[64];
        sr_get_menu_name(0, mname, sizeof(mname));
        snprintf(buf, sizeof(buf), loc(SR_MENU_ENTRY_FMT),
            mname, sr_get_item_count(0));
        sr_output(buf, true);
        return 0;

    // Screen reader: Menu bar/submenu navigation while active
    } else if (msg == WM_KEYDOWN && sr_menubar_active && sr_is_available()
    && (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP
        || wParam == VK_DOWN || wParam == VK_RETURN || wParam == VK_ESCAPE)) {
        char buf[256], mname[64], iname[128], hkbuf[64];
        int menu_count = sr_get_menu_count();
        if (menu_count <= 0) { sr_menubar_active = false; return 0; }

        // Helper lambda: announce a submenu item at position
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

        // Helper lambda: announce menu header + first item
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
            // --- Level 2: Submenu item navigation ---
            int item_count = sr_get_item_count(sr_menubar_index);
            if (item_count <= 0) { sr_submenu_active = false; return 0; }

            if (wParam == VK_DOWN) {
                sr_submenu_index = (sr_submenu_index + 1) % item_count;
                announce_item(sr_menubar_index, sr_submenu_index);
                sr_output(buf, true);
                return 0;

            } else if (wParam == VK_UP) {
                sr_submenu_index = (sr_submenu_index - 1 + item_count) % item_count;
                announce_item(sr_menubar_index, sr_submenu_index);
                sr_output(buf, true);
                return 0;

            } else if (wParam == VK_RIGHT) {
                sr_menubar_index = (sr_menubar_index + 1) % menu_count;
                sr_submenu_index = 0;
                announce_menu_and_first(sr_menubar_index);
                sr_output(buf, true);
                return 0;

            } else if (wParam == VK_LEFT) {
                sr_menubar_index = (sr_menubar_index - 1 + menu_count) % menu_count;
                sr_submenu_index = 0;
                announce_menu_and_first(sr_menubar_index);
                sr_output(buf, true);
                return 0;

            } else if (wParam == VK_ESCAPE) {
                sr_submenu_active = false;
                sr_get_menu_name(sr_menubar_index, mname, sizeof(mname));
                snprintf(buf, sizeof(buf), loc(SR_MENU_ENTRY_FMT),
                    mname, sr_get_item_count(sr_menubar_index));
                sr_output(buf, true);
                return 0;

            } else if (wParam == VK_RETURN) {
                // Activate item via game's menu handler callback
                sr_get_item_name(sr_menubar_index, sr_submenu_index, iname, sizeof(iname));
                sr_menubar_active = false;
                sr_submenu_active = false;
                sr_output(iname, true);
                sr_activate_menu_item(sr_menubar_index, sr_submenu_index);
                return 0;
            }

        } else {
            // --- Level 1: Menu bar navigation ---
            if (wParam == VK_RIGHT) {
                sr_menubar_index = (sr_menubar_index + 1) % menu_count;
                sr_get_menu_name(sr_menubar_index, mname, sizeof(mname));
                snprintf(buf, sizeof(buf), loc(SR_MENU_ENTRY_FMT),
                    mname, sr_get_item_count(sr_menubar_index));
                sr_output(buf, true);
                return 0;

            } else if (wParam == VK_LEFT) {
                sr_menubar_index = (sr_menubar_index - 1 + menu_count) % menu_count;
                sr_get_menu_name(sr_menubar_index, mname, sizeof(mname));
                snprintf(buf, sizeof(buf), loc(SR_MENU_ENTRY_FMT),
                    mname, sr_get_item_count(sr_menubar_index));
                sr_output(buf, true);
                return 0;

            } else if (wParam == VK_DOWN || wParam == VK_RETURN) {
                int ic = sr_get_item_count(sr_menubar_index);
                if (ic > 0) {
                    sr_submenu_active = true;
                    sr_submenu_index = 0;
                    announce_item(sr_menubar_index, 0);
                    sr_output(buf, true);
                }
                return 0;

            } else if (wParam == VK_UP) {
                return 0;

            } else if (wParam == VK_ESCAPE) {
                sr_menubar_active = false;
                sr_submenu_active = false;
                sr_output(loc(SR_MENU_CLOSED), true);
                return 0;
            }
        }

    } else if (msg == WM_CHAR && wParam == 'o' && alt_key_down() && is_editor) {
        uint32_t seed = ThinkerVars->map_random_value;
        int value = pop_ask_number("modmenu", "MAPGEN", seed, 0);
        if (!value) { // OK button pressed
            console_world_generate(ParseNumTable[0]);
        }

    // Screen reader: Scanner mode (Ctrl+Left/Right = jump, Ctrl+PgUp/PgDn = filter)
    } else if (msg == WM_KEYDOWN && sr_is_available() && MapWin
    && (current_window() == GW_World || (current_window() == GW_None && *PopupDialogState >= 2))
    && *MapAreaX > 0 && *MapAreaY > 0
    && ctrl_key_down() && !shift_key_down() && !alt_key_down()
    && (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_PRIOR || wParam == VK_NEXT)) {
        int faction = *CurrentPlayerFaction;

        if (wParam == VK_NEXT) {
            // Ctrl+PgDn: next filter
            sr_scan_filter = (sr_scan_filter + 1) % SR_SCAN_FILTER_COUNT;
            sr_output(loc(sr_scan_filter_names[sr_scan_filter]), true);
            return 0;
        }
        if (wParam == VK_PRIOR) {
            // Ctrl+PgUp: previous filter
            sr_scan_filter = (sr_scan_filter + SR_SCAN_FILTER_COUNT - 1) % SR_SCAN_FILTER_COUNT;
            sr_output(loc(sr_scan_filter_names[sr_scan_filter]), true);
            return 0;
        }

        // Initialize cursor if needed
        if (sr_cursor_x < 0 || sr_cursor_y < 0) {
            if (MapWin->iUnit >= 0 && Vehs && *VehCount > 0
                && MapWin->iUnit < *VehCount) {
                sr_cursor_x = Vehs[MapWin->iUnit].x;
                sr_cursor_y = Vehs[MapWin->iUnit].y;
            } else {
                sr_cursor_x = MapWin->iTileX;
                sr_cursor_y = MapWin->iTileY;
            }
        }

        int sx = sr_cursor_x;
        int sy = sr_cursor_y;
        int cx = sx;
        int cy = sy;
        bool found = false;

        if (wParam == VK_RIGHT) {
            // Scan forward
            while (sr_scan_next_tile(&cx, &cy, sx, sy)) {
                if (sr_scan_matches(cx, cy, sr_scan_filter, faction)) {
                    found = true;
                    break;
                }
            }
        } else {
            // Scan backward
            while (sr_scan_prev_tile(&cx, &cy, sx, sy)) {
                if (sr_scan_matches(cx, cy, sr_scan_filter, faction)) {
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            sr_cursor_x = cx;
            sr_cursor_y = cy;
            sr_announce_tile(sr_cursor_x, sr_cursor_y);
        } else {
            sr_output(loc(SR_SCAN_NOT_FOUND), true);
        }
        return 0;

    // Screen reader: World map navigation (arrows = explore, Shift+arrows = move unit)
    } else if (msg == WM_KEYDOWN && sr_is_available() && MapWin
    && (current_window() == GW_World || (current_window() == GW_None && *PopupDialogState >= 2))
    && *MapAreaX > 0 && *MapAreaY > 0
    && (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT
        || wParam == VK_HOME || wParam == VK_PRIOR || wParam == VK_END || wParam == VK_NEXT)) {
        // Direction offsets for SMAC diamond grid (matching BaseOffset order)
        // NE=0, E=1, SE=2, S=3, SW=4, W=5, NW=6, N=7
        static const int dir_dx[] = { 1, 2, 1, 0, -1, -2, -1, 0 };
        static const int dir_dy[] = { -1, 0, 1, 2, 1, 0, -1, -2 };
        // Map keys to direction index
        int dir = -1;
        if (wParam == VK_UP)          dir = 7; // N
        else if (wParam == VK_DOWN)   dir = 3; // S
        else if (wParam == VK_LEFT)   dir = 5; // W
        else if (wParam == VK_RIGHT)  dir = 1; // E
        else if (wParam == VK_HOME)   dir = 6; // NW
        else if (wParam == VK_PRIOR)  dir = 0; // NE (PgUp)
        else if (wParam == VK_END)    dir = 4; // SW
        else if (wParam == VK_NEXT)   dir = 2; // SE (PgDn)

        if (shift_key_down()) {
            // Shift+key = move unit to adjacent tile using set_move_to
            int veh_id = MapWin->iUnit;
            if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
                VEH* veh = &Vehs[veh_id];
                if (veh->faction_id == MapWin->cOwner) {
                    int tx = wrap(veh->x + dir_dx[dir]);
                    int ty = veh->y + dir_dy[dir];
                    if (ty >= 0 && ty < *MapAreaY) {
                        int old_x = veh->x;
                        int old_y = veh->y;
                        sr_debug_log("UNIT-MOVE %s (%d,%d)->(%d,%d) dir=%d",
                            veh->name(), old_x, old_y, tx, ty, dir);
                        set_move_to(veh_id, tx, ty);
                        action(veh_id);
                        // Check if unit actually moved
                        if (veh_id < *VehCount
                            && (veh->x != old_x || veh->y != old_y)) {
                            sr_cursor_x = veh->x;
                            sr_cursor_y = veh->y;
                            sr_announce_tile(sr_cursor_x, sr_cursor_y);
                            // Announce remaining moves
                            int total_speed = veh_speed(veh_id, 0);
                            int remaining = max(0, total_speed - (int)veh->moves_spent);
                            int disp_rem = remaining / Rules->move_rate_roads;
                            int disp_tot = total_speed / Rules->move_rate_roads;
                            char move_buf[64];
                            snprintf(move_buf, sizeof(move_buf),
                                loc(SR_MOVEMENT_POINTS), disp_rem, disp_tot);
                            sr_output(move_buf, false);
                        } else {
                            sr_output(loc(SR_CANNOT_MOVE), true);
                        }
                    } else {
                        sr_output(loc(SR_MAP_EDGE), true);
                    }
                }
            }
            return 0;
        } else {
            // No shift = explore: move virtual cursor, announce tile
            // Initialize cursor to unit position if not set
            if (sr_cursor_x < 0 || sr_cursor_y < 0) {
                if (MapWin->iUnit >= 0 && Vehs && *VehCount > 0
                    && MapWin->iUnit < *VehCount) {
                    sr_cursor_x = Vehs[MapWin->iUnit].x;
                    sr_cursor_y = Vehs[MapWin->iUnit].y;
                } else {
                    sr_cursor_x = MapWin->iTileX;
                    sr_cursor_y = MapWin->iTileY;
                }
            }
            int nx = sr_cursor_x + dir_dx[dir];
            int ny = sr_cursor_y + dir_dy[dir];
            // Wrap X coordinate (cylindrical map)
            nx = wrap(nx);
            // Clamp Y to map bounds
            if (ny >= 0 && ny < *MapAreaY) {
                sr_cursor_x = nx;
                sr_cursor_y = ny;
                sr_announce_tile(sr_cursor_x, sr_cursor_y);
            } else {
                sr_output(loc(SR_MAP_EDGE), true);
            }
        }
        return 0; // Consume the key, don't pass to game

    // Screen reader: Shift+Space = send unit to cursor position (Go To)
    } else if (msg == WM_KEYDOWN && wParam == VK_SPACE && shift_key_down()
    && sr_is_available() && MapWin
    && (current_window() == GW_World || (current_window() == GW_None && *PopupDialogState >= 2))
    && sr_cursor_x >= 0 && sr_cursor_y >= 0) {
        int veh_id = MapWin->iUnit;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            VEH* veh = &Vehs[veh_id];
            if (veh->faction_id == MapWin->cOwner) {
                set_move_to(veh_id, sr_cursor_x, sr_cursor_y);
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_UNIT_MOVES_TO),
                    veh->name(), sr_cursor_x, sr_cursor_y);
                sr_debug_log("GO-TO: %s", buf);
                sr_output(buf, true);
                // Advance to next unit needing orders
                mod_veh_skip(veh_id);
            }
        }
        return 0;

    // Screen reader: E on world map → open accessible Social Engineering handler
    } else if (msg == WM_KEYDOWN && wParam == 'E' && sr_is_available()
    && !ctrl_key_down() && !alt_key_down()
    && !*GameHalted && current_window() == GW_World) {
        SocialEngHandler::RunModal();
        return 0;

    // Screen reader: Escape on world map → read quit dialog text before game opens it
    } else if (msg == WM_KEYDOWN && wParam == VK_ESCAPE && sr_is_available()
    && !*GameHalted && current_window() == GW_World) {
        char buf[512];
        if (sr_read_popup_text("Script", "REALLYQUIT", buf, sizeof(buf))) {
            sr_output(buf, false);
        }
        return WinProc(hwnd, msg, wParam, lParam);

    // Screen reader: Shift+F1 = context-sensitive help for current unit/terrain
    } else if (msg == WM_KEYDOWN && wParam == VK_F1 && shift_key_down()
    && sr_is_available() && MapWin
    && (current_window() == GW_World || (current_window() == GW_None && *PopupDialogState >= 2))) {
        char buf[1024];
        int pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", loc(SR_HELP_HEADER));

        int veh_id = MapWin->iUnit;
        VEH* veh = NULL;
        if (veh_id >= 0 && Vehs && *VehCount > 0 && veh_id < *VehCount) {
            veh = &Vehs[veh_id];
        }

        // Unit-specific commands
        if (veh && veh->faction_id == MapWin->cOwner) {
            MAP* tile = mapsq(veh->x, veh->y);
            uint32_t items = tile ? tile->items : 0;

            if (veh->is_former()) {
                // Terraform commands based on terrain
                if (tile && tile->is_fungus()) {
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_REMOVE_FUNGUS));
                } else {
                    if (!(items & BIT_ROAD)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_ROAD));
                    } else if (!(items & BIT_MAGTUBE)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_MAGTUBE));
                    }
                    if (!(items & BIT_FARM)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_FARM));
                    }
                    if (!(items & BIT_MINE)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_MINE));
                    }
                    if (!(items & BIT_SOLAR)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_SOLAR));
                    }
                    if (!(items & BIT_FOREST)) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_FOREST));
                    }
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_SENSOR));
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_CONDENSER));
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_BOREHOLE));
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_AIRBASE));
                    pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_BUNKER));
                }
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_AUTOMATE));
            }
            if (veh->is_colony()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_BUILD_BASE));
            }
            if (veh->is_combat_unit()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_ATTACK));
            }
            if (veh->is_probe()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_PROBE));
            }
            if (veh->is_supply()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_CONVOY));
            }
            if (veh->is_transport()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_UNLOAD));
            }
            if (veh->triad() != TRIAD_SEA && veh->triad() != TRIAD_AIR) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_AIRDROP));
            }

            // Terrain-specific hints
            if (tile && tile->is_base()) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_OPEN_BASE));
            }
            if (items & BIT_MONOLITH) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_MONOLITH));
            }
            if (items & BIT_SUPPLY_POD) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_SUPPLY_POD));
            }
        }

        // Always-shown commands
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_MOVE));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_EXPLORE));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_SKIP));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_HOLD));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_READ));
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %s.", loc(SR_HELP_GOTO));

        sr_debug_log("CONTEXT-HELP: %s", buf);
        sr_output(buf, true);
        return 0;

    // Screen reader: Ctrl+R = read current screen text, Ctrl+Shift+R = silence
    } else if (msg == WM_KEYDOWN && wParam == 'R' && ctrl_key_down() && sr_is_available()) {
        if (shift_key_down()) {
            sr_debug_log("CTRL+SHIFT+R: silence");
            sr_silence();
        } else {
            const char* text = sr_get_last_text();
            sr_debug_log("CTRL+R: text=%s", (text && text[0]) ? text : "(empty)");
            if (text && text[0]) {
                sr_output(text, true);
            } else {
                sr_output(loc(SR_NO_TEXT), true);
            }
        }

    // Screen reader: Ctrl+F12 = toggle debug logging
    } else if (msg == WM_KEYDOWN && wParam == VK_F12 && ctrl_key_down() && sr_is_available()) {
        sr_debug_toggle();

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

int __cdecl mod_Win_init_class(const char* lpWindowName)
{
    int value = Win_init_class(lpWindowName);
    set_video_mode(0);
    return value;
}

void __cdecl mod_amovie_project(const char* name)
{
    if (!strlen(name) || !conf.video_player) {
        return;
    } else if (conf.video_player == 1) {
        conf.playing_movie = true;
        amovie_project(name);
    } else if (conf.video_player == 2) {
        conf.playing_movie = true;
        PROCESS_INFORMATION pi = {};
        STARTUPINFO si = {};
        std::string cmd = "\"" + video_player_path + "\" " + video_player_args
            + " .\\movies\\" + std::string(name) + ".wve";
        if (CreateProcessA(NULL, (char*)cmd.c_str(),
        NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    if (*phWnd) {
        PostMessage(*phWnd, WM_MOVIEOVER, 0, 0);
    }
}

void restore_video_mode()
{
    ChangeDisplaySettings(NULL, 0);
}

void set_video_mode(bool reset_window)
{
    if (conf.video_mode != VM_Window
    && (conf.window_width != conf.screen_width || conf.window_height != conf.screen_height)) {
        DEVMODE dv = {};
        dv.dmPelsWidth = conf.window_width;
        dv.dmPelsHeight = conf.window_height;
        dv.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
        dv.dmSize = sizeof(dv);
        ChangeDisplaySettings(&dv, CDS_FULLSCREEN);
    }
    else if (conf.video_mode == VM_Window && reset_window && *phWnd) {
        // Restore window layout after movie playback has ended
        restore_video_mode();
        SetWindowLong(*phWnd, GWL_STYLE, AC_WS_WINDOWED);
        PostMessage(*phWnd, WM_WINDOWED, 0, 0);
    }
}

void set_minimised(bool minimise)
{
    if (conf.minimised != minimise && *phWnd) {
        conf.minimised = minimise;
        if (minimise) {
            restore_video_mode();
            ShowWindow(*phWnd, SW_MINIMIZE);
        } else {
            set_video_mode(0);
            ShowWindow(*phWnd, SW_RESTORE);
        }
    }
}

void set_windowed(bool windowed)
{
    if (!conf.playing_movie && *phWnd) {
        if (conf.video_mode == VM_Custom && windowed) {
            conf.video_mode = VM_Window;
            restore_video_mode();
            SetWindowLong(*phWnd, GWL_STYLE, AC_WS_WINDOWED);
            PostMessage(*phWnd, WM_WINDOWED, 0, 0);
        }
        else if (conf.video_mode == VM_Window && !windowed) {
            conf.video_mode = VM_Custom;
            set_video_mode(0);
            SetWindowLong(*phWnd, GWL_STYLE, AC_WS_FULLSCREEN);
            SetWindowPos(*phWnd, HWND_TOPMOST, 0, 0, conf.window_height, conf.window_width,
                         SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
            ShowWindow(*phWnd, SW_RESTORE);
        }
    }
}

/*
Render custom debug overlays with original and additional goals.
*/
void __thiscall MapWin_gen_overlays(Console* This, int x, int y)
{
    Buffer* Canvas = (Buffer*)&This->oMainWin.oCanvas.poOwner;
    RECT rt;
    if (*GameState & STATE_OMNISCIENT_VIEW && This->iWhatToDrawFlags & MAPWIN_DRAW_GOALS)
    {
        MapWin_tile_to_pixel(This, x, y, &rt.left, &rt.top);
        rt.right = rt.left + This->iPixelsPerTileX;
        rt.bottom = rt.top + This->iPixelsPerHalfTileX;

        char buf[20] = {};
        bool found = false;
        int color = 255;
        int value = mapdata[{x, y}].overlay;

        for (int faction = 1; faction < MaxPlayerNum && !found; faction++) {
            Faction& f = Factions[faction];
            MFaction& m = MFactions[faction];
            if (!f.base_count) {
                continue;
            }
            for (int i = 0; i < MaxGoalsNum && !found; i++) {
                Goal& goal = Factions[faction].goals[i];
                if (goal.x == x && goal.y == y && goal.priority > 0
                && goal.type != AI_GOAL_UNUSED ) {
                    found = true;
                    buf[0] = m.filename[0];
                    switch (goal.type) {
                        case AI_GOAL_ATTACK:
                            buf[1] = 'a';
                            color = ColorRed;
                            break;
                        case AI_GOAL_DEFEND:
                            buf[1] = 'd';
                            color = ColorYellow;
                            break;
                        case AI_GOAL_SCOUT:
                            buf[1] = 's';
                            color = ColorMagenta;
                            break;
                        case AI_GOAL_UNK_1:
                            buf[1] = 'n';
                            color = ColorBlue;
                            break;
                        case AI_GOAL_COLONIZE:
                            buf[1] = 'c';
                            color = ColorCyan;
                            break;
                        case AI_GOAL_TERRAFORM_LAND:
                            buf[1] = 'f';
                            color = ColorGreen;
                            break;
                        case AI_GOAL_UNK_4:
                            buf[1] = '^';
                            break;
                        case AI_GOAL_RAISE_LAND:
                            buf[1] = 'r';
                            break;
                        case AI_GOAL_NAVAL_BEACH:
                        case AI_GOAL_NAVAL_START:
                        case AI_GOAL_NAVAL_END:
                            buf[1] = 'n';
                            break;
                        default:
                            buf[1] = (goal.type < Thinker_Goal_ID_First ? '*' : 'g');
                            break;
                    }
                    _itoa(goal.priority, &buf[2], 10);
                }
            }
        }
        if (!found && value != 0) {
            color = (value >= 0 ? ColorWhite : ColorYellow);
            _itoa(value, buf, 10);
        }
        if (found || value) {
            Buffer_set_text_color(Canvas, color, 0, 1, 1);
            Buffer_set_font(Canvas, &This->oFont2, 0, 0, 0);
            Buffer_write_cent_l3(Canvas, buf, &rt, 20);
        }
    }
}

void __cdecl mod_turn_timer()
{
    /*
    Timer calls this function every 500ms.
    Used for multiplayer related screen updates in turn_timer().
    */
    static uint32_t iter = 0;
    static uint32_t prev_time = 0;
    turn_timer();
    if (++iter & 1) {
        return;
    }
    if (*GameHalted && !*MapRandomSeed) {
        ThinkerVars->game_time_spent = 0;
        prev_time = 0;
    } else {
        uint32_t now = GetTickCount();
        if (prev_time && now - prev_time > 0) {
            ThinkerVars->game_time_spent += now - prev_time;
        }
        prev_time = now;
    }
}

void popup_homepage()
{
    ShellExecute(NULL, "open", "https://github.com/induktio/thinker", NULL, NULL, SW_SHOWNORMAL);
}

void show_mod_stats()
{
    int total_pop = 0,
        total_minerals = 0,
        total_energy = 0,
        faction_bases = 0,
        faction_pop = 0,
        faction_units = 0,
        faction_minerals = 0,
        faction_energy = 0;

    for (int i = 0; i < *BaseCount; ++i) {
        BASE* b = &Bases[i];
        int mindiv = (has_project(FAC_SPACE_ELEVATOR, b->faction_id)
             && (b->item() == -FAC_ORBITAL_DEFENSE_POD
             || b->item() == -FAC_NESSUS_MINING_STATION
             || b->item() == -FAC_ORBITAL_POWER_TRANS
             || b->item() == -FAC_SKY_HYDRO_LAB) ? 2 : 1);
        if (b->faction_id == MapWin->cOwner) {
            faction_bases++;
            faction_pop += b->pop_size;
            faction_minerals += b->mineral_intake_2 / mindiv;
            faction_energy += b->energy_intake_2;
        }
        total_pop += b->pop_size;
        total_minerals += b->mineral_intake_2 / mindiv;
        total_energy += b->energy_intake_2;
    }
    for (int i = 0; i < *VehCount; i++) {
        VEH* v = &Vehs[i];
        if (v->faction_id == MapWin->cOwner) {
            faction_units++;
        }
    }
    ParseNumTable[0] = *BaseCount;
    ParseNumTable[1] = *VehCount;
    ParseNumTable[2] = total_pop;
    ParseNumTable[3] = total_minerals;
    ParseNumTable[4] = total_energy;
    ParseNumTable[5] = faction_bases;
    ParseNumTable[6] = faction_units;
    ParseNumTable[7] = faction_pop;
    ParseNumTable[8] = faction_minerals;
    ParseNumTable[9] = faction_energy;
    popp("modmenu", "STATS", 0, "markbm_sm.pcx", 0);
}

int show_mod_config()
{
    enum {
        MapGen = 1,
        MapContinents = 2,
        MapLandmarks = 4,
        MapPolarCaps = 8,
        MapMirrorX = 16,
        MapMirrorY = 32,
        AutoBases = 64,
        AutoUnits = 128,
        FormerReplace = 256,
        MapBaseInfo = 512,
        TreatyPopup = 1024,
        AutoMinimise = 2048,
    };
    *DialogChoices = 0
        | (conf.new_world_builder ? MapGen : 0)
        | (conf.world_continents ? MapContinents : 0)
        | (conf.modified_landmarks ? MapLandmarks : 0)
        | (conf.world_polar_caps ? MapPolarCaps: 0)
        | (conf.world_mirror_x ? MapMirrorX : 0)
        | (conf.world_mirror_y ? MapMirrorY : 0)
        | (conf.manage_player_bases ? AutoBases : 0)
        | (conf.manage_player_units ? AutoUnits : 0)
        | (conf.warn_on_former_replace ? FormerReplace : 0)
        | (conf.render_base_info ? MapBaseInfo : 0)
        | (conf.foreign_treaty_popup ? TreatyPopup : 0)
        | (conf.auto_minimise ? AutoMinimise : 0);

    // Return value is equal to choices bitfield if OK pressed, -1 otherwise.
    int value = X_pop("modmenu", "OPTIONS", -1, 0, PopDialogCheckbox|PopDialogBtnCancel, 0);
    if (value < 0) {
        return 0;
    }
    conf.new_world_builder = !!(value & MapGen);
    WritePrivateProfileStringA(ModAppName, "new_world_builder",
        (conf.new_world_builder ? "1" : "0"), ModIniFile);

    conf.modified_landmarks = !!(value & MapLandmarks);
    WritePrivateProfileStringA(ModAppName, "modified_landmarks",
        (conf.modified_landmarks ? "1" : "0"), ModIniFile);

    conf.world_continents = !!(value & MapContinents);
    WritePrivateProfileStringA(ModAppName, "world_continents",
        (conf.world_continents ? "1" : "0"), ModIniFile);

    conf.world_polar_caps = !!(value & MapPolarCaps);
    WritePrivateProfileStringA(ModAppName, "world_polar_caps",
        (conf.world_polar_caps ? "1" : "0"), ModIniFile);

    conf.world_mirror_x = !!(value & MapMirrorX);
    WritePrivateProfileStringA(ModAppName, "world_mirror_x",
        (conf.world_mirror_x ? "1" : "0"), ModIniFile);

    conf.world_mirror_y = !!(value & MapMirrorY);
    WritePrivateProfileStringA(ModAppName, "world_mirror_y",
        (conf.world_mirror_y ? "1" : "0"), ModIniFile);

    conf.manage_player_bases = !!(value & AutoBases);
    WritePrivateProfileStringA(ModAppName, "manage_player_bases",
        (conf.manage_player_bases ? "1" : "0"), ModIniFile);

    conf.manage_player_units = !!(value & AutoUnits);
    WritePrivateProfileStringA(ModAppName, "manage_player_units",
        (conf.manage_player_units ? "1" : "0"), ModIniFile);

    conf.warn_on_former_replace = !!(value & FormerReplace);
    WritePrivateProfileStringA(ModAppName, "warn_on_former_replace",
        (conf.warn_on_former_replace ? "1" : "0"), ModIniFile);

    conf.render_base_info = !!(value & MapBaseInfo);
    WritePrivateProfileStringA(ModAppName, "render_base_info",
        (conf.render_base_info ? "1" : "0"), ModIniFile);

    conf.foreign_treaty_popup = !!(value & TreatyPopup);
    WritePrivateProfileStringA(ModAppName, "foreign_treaty_popup",
        (conf.foreign_treaty_popup ? "1" : "0"), ModIniFile);

    conf.auto_minimise = !!(value & AutoMinimise);
    WritePrivateProfileStringA(ModAppName, "auto_minimise",
        (conf.auto_minimise ? "1" : "0"), ModIniFile);

    draw_map(1);
    if (Win_is_visible(BaseWin)) {
        BaseWin_on_redraw(BaseWin);
    }
    return 0;
}

int show_mod_menu()
{
    parse_says(0, MOD_VERSION, -1, -1);
    parse_says(1, MOD_DATE, -1, -1);

    if (*GameHalted) {
        int ret = popp("modmenu", "MAINMENU", 0, "stars_sm.pcx", 0);
        if (ret == 1) {
            show_mod_config();
        }
        else if (ret == 2) {
            show_rules_menu();
        }
        else if (ret == 3) {
            popup_homepage();
        }
        return 0; // Close dialog
    }
    uint64_t seconds = ThinkerVars->game_time_spent / 1000;
    ParseNumTable[0] = seconds / 3600;
    ParseNumTable[1] = (seconds / 60) % 60;
    ParseNumTable[2] = seconds % 60;
    int ret = popp("modmenu", "GAMEMENU", 0, "stars_sm.pcx", 0);

    if (ret == 1 && !*PbemActive && !*MultiplayerActive) {
        show_mod_stats();
    }
    else if (ret == 2) {
        show_mod_config();
    }
    else if (ret == 3) {
        popup_homepage();
    }
    return 0; // Close dialog
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

int __thiscall SetupWin_buffer_draw(Buffer* src, Buffer* dst, int a3, int a4, int a5, int a6, int a7)
{
    if (conf.window_width >= 1024) {
        const int moon_positions[][4] = {
            {8, 287, 132, 132},
            {221, 0, 80, 51},
            {348, 94, 55, 57},
        };
        for (auto& p : moon_positions) {
            int x = conf.window_width  * p[0] / 1024;
            int y = conf.window_height * p[1] / 768;
            int w = conf.window_width  * p[2] / 1024;
            int h = conf.window_height * p[3] / 768;
            Buffer_copy2(src, dst, p[0], p[1], p[2], p[3], x, y, w, h);
        }
        return 0;
    } else {
        return Buffer_draw(src, dst, a3, a4, a5, a6, a7);
    }
}

int __thiscall SetupWin_buffer_copy(Buffer* src, Buffer* dst,
int xSrc, int ySrc, int xDst, int yDst, int wSrc, int hSrc)
{
    if (conf.window_width >= 1024) {
        int wDst = conf.window_width * wSrc / 1024;
        return Buffer_copy2(src, dst, xSrc, ySrc, wSrc, hSrc,
            conf.window_width - wDst, yDst, wDst, conf.window_height);
    } else {
        return Buffer_copy(src, dst, xSrc, ySrc, xDst, yDst, wSrc, hSrc);
    }
}

int __thiscall SetupWin_soft_update3(Win* This, int a2, int a3, int a4, int a5)
{
    // Update whole screen instead of partial regions
    return GraphicWin_soft_update2(This);
}

int __thiscall window_scale_load_pcx(Buffer* This, char* filename, int a3, int a4, int a5)
{
    int value;
    if (conf.window_width >= 1024) {
        Buffer image;
        Buffer_Buffer(&image);
        value = Buffer_load_pcx(&image, filename, a3, a4, a5);
        Buffer_resize(This, conf.window_width, conf.window_height);
        Buffer_copy2(&image, This, 0, 0, image.stRect->right, image.stRect->bottom,
            0, 0, conf.window_width, conf.window_height);
        Buffer_dtor(&image);
    } else {
        value = Buffer_load_pcx(This, filename, a3, a4, a5);
    }
    if (*GameHalted) {
        char buf[StrBufLen];
        snprintf(buf, StrBufLen, "%s%s%s%s",
            MOD_VERSION, " / ", MOD_DATE, (conf.smac_only ? " / SMAC" : ""));
        Buffer_set_text_color(This, ColorProdName, 0, 1, 1);
        Buffer_set_font(This, &MapWin->oFont1, 0, 0, 0);
        Buffer_write_l(This, buf, 20, conf.window_height-32, 100);
    }
    return value;
}

int __thiscall Credits_GraphicWin_init(
Win* This, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
    if (conf.window_width >= 1024) {
        return GraphicWin_init(This,
            a2 * conf.window_width / 1024,
            a3 * conf.window_height / 768,
            a4 * conf.window_width / 1024,
            a5 * conf.window_height / 768,
            a6, a7, a8, a9, a10);
    } else {
        return GraphicWin_init(This, a2, a3, a4, a5, a6, a7, a8, a9, a10);
    }
}

int __thiscall BaseWin_hurry_popup_start(
Win* This, const char* filename, const char* label, int a4, int a5, int a6, int a7)
{
    BASE* base = *CurrentBase;
    Faction* f = &Factions[base->faction_id];
    int item_cost = mineral_cost(*CurrentBaseID, base->queue_items[0]);
    int minerals = item_cost - base->minerals_accumulated - max(0, base->mineral_surplus);
    int credits = max(0, f->energy_credits - f->hurry_cost_total);
    int cost = hurry_cost(*CurrentBaseID, base->queue_items[0], minerals);
    hurry_minimal_cost = min(credits, cost);
    if (item_cost <= base->minerals_accumulated) {
        ParseNumTable[0] = 0;
    }
    ParseNumTable[1] = cost;
    ParseNumTable[2] = credits;
    return Popup_start(This, "modmenu", "HURRY", a4, a5, a6, a7);
}

#pragma GCC diagnostic pop

int __cdecl BaseWin_hurry_ask_number(const char* label, int value, int a3)
{
    ParseNumTable[0] = value;
    return pop_ask_number(ScriptFile, label, hurry_minimal_cost, a3);
}

/*
Fix issue where hurry production flag will not be set after
completely hurrying the current production "Spend $NUM0 energy credits."
*/
int __thiscall BaseWin_hurry_unlock_base(AlphaNet* This, int base_id)
{
    if (base_id >= 0) {
        Bases[base_id].state_flags |= BSTATE_HURRY_PRODUCTION;
    }
    return NetDaemon_unlock_base(This, base_id);
}

int __thiscall BaseWin_gov_options(BaseWindow* This, int flag)
{
    int base_id = *CurrentBaseID;
    if (base_id < 0 || base_id != This->oRender.base_id) {
        assert(0);
        return 1;
    }
    BASE* base = &Bases[base_id];
    int worked_tiles = base->worked_tiles;
    set_base(base_id);
    base_compute(base_id);
    if (*MultiplayerActive && !*ControlTurnC
    && worked_tiles != base->worked_tiles
    && !NetDaemon_lock_base(NetState, *CurrentBaseID, 0, -1, -1)) {
        NetDaemon_unlock_base(NetState, *CurrentBaseID);
    }
    if (base->faction_id != MapWin->cOwner && !(*GameState & STATE_OMNISCIENT_VIEW)) {
        return 1;
    }
    *DialogChoices = 0;
    for (auto& p : BaseGovOptions) {
        if (base->governor_flags & p[1]) {
            *DialogChoices |= p[0];
        }
    }
    parse_says(0, base->name, -1, -1);
    if (NetDaemon_lock_base(NetState, base_id, 0, -1, -1)) {
        return 1;
    }
    if (X_pop("modmenu", "GOVOPTIONS", -1, 0, 65, 0) >= 0) {
        base->governor_flags &= (GOV_PRIORITY_CONQUER|GOV_PRIORITY_BUILD|GOV_PRIORITY_DISCOVER|GOV_PRIORITY_EXPLORE);
        for (auto& p : BaseGovOptions) {
            if (*DialogChoices & p[0]) {
                base->governor_flags |= p[1];
            }
        }
        Factions[base->faction_id].base_governor_adv = base->governor_flags;
        if (!flag) {
            if (base->governor_flags & GOV_ACTIVE) {
                if (base->governor_flags & GOV_MANAGE_PRODUCTION) {
                    base->state_flags &= ~BSTATE_UNK_80000000;
                    base->queue_size = 0;
                    mod_base_reset(base_id, 1);
                }
                if (base->governor_flags & GOV_MANAGE_CITIZENS) {
                    base->worked_tiles = 0;
                    base->specialist_total = 0;
                    base->specialist_adjust = 0;
                    base_compute(1);
                    // base_doctors() removed as obsolete
                }
            }
            if (!(base->governor_flags & GOV_ACTIVE) || !(base->governor_flags & GOV_MANAGE_PRODUCTION)) {
                draw_radius(base->x, base->y, 2, 2);
            }
        }
        NetDaemon_unlock_base(NetState, base_id);
        GraphicWin_redraw(BaseWin);
        GraphicWin_redraw(MainWin);
        return 0;
    } else {
        NetDaemon_unlock_base(NetState, base_id);
        return 1;
    }
}

void __thiscall BaseWin_draw_support(BaseWindow* This)
{
    RECT& rc = This->oRender.rResWindow;
    Buffer_set_clip(&This->oCanvas, &rc);
    GraphicWin_fill2((Win*)This, &rc, 0);

    MapWin_init((Console*)&This->oRender, 2, 0);
    This->oRender.iZoomFactor = base_zoom_factor;
    This->oRender.iWhatToDrawFlags = MAPWIN_SUPPORT_VIEW|MAPWIN_DRAW_BONUS_RES|\
        MAPWIN_DRAW_RIVERS|MAPWIN_DRAW_IMPROVEMENTS|MAPWIN_DRAW_TRANSLUCENT;

    This->oRender.iTileX = (*CurrentBase)->x;
    This->oRender.iTileY = (*CurrentBase)->y;
    GraphicWin_redraw((Win*)&This->oRender.oBufWin);

    Buffer_copy(&This->oRender.oBufWin.oCanvas, &This->oCanvas,
        0, 0, rc.left + 11, rc.top + 31, rc.right, rc.bottom);
    GraphicWin_soft_update((Win*)This, &rc);
    Buffer_set_clip(&This->oCanvas, &This->oCanvas.stRect[0]);
}

void __thiscall BaseWin_draw_misc_eco_damage(Buffer* This, char* buf, int x, int y, int len)
{
    BASE* base = *CurrentBase;
    Faction* f = &Factions[base->faction_id];
    if (!conf.render_base_info || !strlen(label_eco_damage)) {
        Buffer_write_l(This, buf, x, y, len);
    } else {
        int clean_mins = conf.clean_minerals + f->clean_minerals_modifier
            + clamp(f->satellites_mineral, 0, (int)base->pop_size);
        int damage = terraform_eco_damage(*CurrentBaseID);
        int mins = base->mineral_intake_2 + damage/8;
        int pct;
        if (base->eco_damage > 0) {
            pct = 100 + base->eco_damage;
        } else {
            pct = (clean_mins > 0 ? 100 * clamp(mins, 0, clean_mins) / clean_mins : 0);
        }
        snprintf(buf, StrBufLen, label_eco_damage, pct);
        Buffer_write_l(This, buf, x, y, strlen(buf));
    }
}

void __thiscall BaseWin_draw_farm_set_font(Buffer* This, Font* font, int a3, int a4, int a5)
{
    char buf[StrBufLen] = {};
    // Base resource window coordinates including button row
    RECT* rc = &BaseWin->oRender.rResWindow;
    int x1 = rc->left;
    int y1 = rc->top;
    int x2 = rc->right;
    int y2 = rc->bottom;
    int N = 0;
    int M = 0;
    int E = 0;
    int SE = 0;
    Buffer_set_font(This, font, a3, a4, a5);

    if (*CurrentBaseID < 0 || x1 < 0 || y1 < 0 || x2 < 0 || y2 < 0) {
        assert(0);
    } else if (conf.render_base_info) {
        if (satellite_bonus(*CurrentBaseID, &N, &M, &E)) {
            snprintf(buf, StrBufLen, label_sat_nutrient, N);
            Buffer_set_text_color(This, ColorNutrient, 0, 1, 1);
            Buffer_write_l(This, buf, x1 + 5, y2 - 36 - 28, LineBufLen);

            snprintf(buf, StrBufLen, label_sat_mineral, M);
            Buffer_set_text_color(This, ColorMineral, 0, 1, 1);
            Buffer_write_l(This, buf, x1 + 5, y2 - 36 - 14, LineBufLen);

            snprintf(buf, StrBufLen, label_sat_energy, E);
            Buffer_set_text_color(This, ColorEnergy, 0, 1, 1);
            Buffer_write_l(This, buf, x1 + 5, y2 - 36     , LineBufLen);
        }
        if ((SE = stockpile_energy_active(*CurrentBaseID)) > 0) {
            snprintf(buf, StrBufLen, label_stockpile_energy, SE);
            Buffer_set_text_color(This, ColorEnergy, 0, 1, 1);
            Buffer_write_right_l2(This, buf, x2 - 5, y2 - 36, LineBufLen);
        }
    }
}

void __cdecl BaseWin_draw_psych_strcat(char* buffer, char* source)
{
    BASE* base = &Bases[*CurrentBaseID];
    if (conf.render_base_info && *CurrentBaseID >= 0) {
        if (base->nerve_staple_turns_left > 0
        || has_fac_built(FAC_PUNISHMENT_SPHERE, *CurrentBaseID)) {
            if (!strcmp(source, label_get(971))) { // Stapled Base
                strncat(buffer, label_get(322), StrBufLen); // Unmodified
                return;
            }
            if (!strcmp(source, label_get(327))) { // Secret Projects
                strncat(buffer, label_get(971), StrBufLen); // Stapled Base
                return;
            }
        }
        int turns = base->assimilation_turns_left;
        if (turns > 0 && !strcmp(source, label_get(970))) { // Captured Base
            snprintf(buffer, StrBufLen, label_captured_base, turns);
            return;
        }
    }
    strncat(buffer, source, StrBufLen);
}

void __thiscall BaseWin_draw_energy_set_text_color(Buffer* This, int a2, int a3, int a4, int a5)
{
    BASE* base = &Bases[*CurrentBaseID];
    char buf[StrBufLen] = {};
    if (conf.render_base_info && *CurrentBaseID >= 0) {
        int workers = base->pop_size - base->talent_total - base->drone_total - base->specialist_total;
        int color;

        if (base_maybe_riot(*CurrentBaseID)) {
            color = ColorRed;
        } else if (base->golden_age()) {
            color = ColorEnergy;
        } else {
            color = ColorIntakeSurplus;
        }
        Buffer_set_text_color(This, color, a3, a4, a5);
        snprintf(buf, StrBufLen, label_pop_size,
            base->talent_total, workers, base->drone_total, base->specialist_total);
        if (DEBUG) {
            strncat(buf, conf.base_psych ? " / B" : " / A", 32);
        }
        Buffer_write_right_l2(This, buf, 690, 423 - 42, LineBufLen);

        if (base_pop_boom(*CurrentBaseID) && base_unused_space(*CurrentBaseID) > 0) {
            Buffer_set_text_color(This, ColorNutrient, a3, a4, a5);
            snprintf(buf, StrBufLen, "%s", label_pop_boom);
            Buffer_write_right_l2(This, buf, 690, 423 - 21, LineBufLen);
        }

        if (base->nerve_staple_turns_left > 0) {
            snprintf(buf, StrBufLen, label_nerve_staple, base->nerve_staple_turns_left);
            Buffer_set_text_color(This, ColorEnergy, a3, a4, a5);
            Buffer_write_right_l2(This, buf, 690, 423, LineBufLen);
        }
    }
    Buffer_set_text_color(This, a2, a3, a4, a5);
}

void __cdecl mod_base_draw(Buffer* buffer, int base_id, int x, int y, int zoom, int opts)
{
    int color = -1;
    int width = 1;
    BASE* base = &Bases[base_id];
    base_draw(buffer, base_id, x, y, zoom, opts);

    if (conf.render_base_info && zoom >= -8) {
        if (has_fac_built(FAC_HEADQUARTERS, base_id)) {
            color = ColorWhite;
            width = 2;
        }
        if (has_fac_built(FAC_GEOSYNC_SURVEY_POD, base_id)
        || has_fac_built(FAC_FLECHETTE_DEFENSE_SYS, base_id)) {
            color = ColorCyan;
        }
        if (base->faction_id == MapWin->cOwner && base->golden_age()) {
            color = ColorEnergy;
        }
        if (base->faction_id == MapWin->cOwner && base_maybe_riot(base_id)) {
            color = ColorRed;
        }
        if (color < 0) {
            return;
        }
        // Game engine uses this way to determine the population label width
        int w = Font_width(*MapLabelFont, (base->pop_size >= 10 ? "88" : "8")) + 5;
        int h = (*MapLabelFont)->iHeight + 4;

        for (int i = 1; i <= width; i++) {
            RECT rr = {x-i, y-i, x+w+i, y+h+i};
            Buffer_box(buffer, &rr, color, color);
        }
    }
}

int __cdecl BaseWin_staple_popp(
const char* filename, const char* label, int a3, const char* imagefile, int a5)
{
    BASE* base = *CurrentBase;
    if (base && base->assimilation_turns_left
    && base->faction_id_former != MapWin->cOwner && is_alive(base->faction_id_former)) {
        return popp("modmenu", "NERVESTAPLE2", a3, imagefile, a5);
    }
    return popp(filename, label, a3, imagefile, a5);
}

/*
Refresh base window workers properly after nerve staple is done.
*/
void __cdecl BaseWin_action_staple(int base_id)
{
    if (can_staple(base_id)) {
        set_base(base_id);
        action_staple(base_id);
        base_compute(1);
        BaseWin_on_redraw(BaseWin);
    }
}

/*
Separate case where nerve stapling is done from another popup.
*/
void __cdecl popb_action_staple(int base_id)
{
    if (can_staple(base_id)) {
        action_staple(base_id);
    }
}

int __thiscall BaseWin_click_staple(Win* This)
{
    // SE_Police value is checked before calling this function
    int base_id = ((BaseWindow*)This)->oRender.base_id;
    if (base_id >= 0 && conf.nerve_staple > Bases[base_id].plr_owner()) {
        return BaseWin_nerve_staple(This);
    }
    return 0;
}

void __cdecl ReportWin_draw_ops_strcat(char* dst, char* UNUSED(src))
{
    BASE* base = *CurrentBase;
    uint32_t gov = base->governor_flags;
    char buf[StrBufLen] = {};
    dst[0] = '\0';

    if (base->faction_id == MapWin->cOwner) {
        if (gov & GOV_ACTIVE) {
            if (gov & GOV_PRIORITY_EXPLORE) {
                strncat(dst, label_get(521), 32);
            } else if (gov & GOV_PRIORITY_DISCOVER) {
                strncat(dst, label_get(522), 32);
            } else if (gov & GOV_PRIORITY_BUILD) {
                strncat(dst, label_get(523), 32);
            } else if (gov & GOV_PRIORITY_CONQUER) {
                strncat(dst, label_get(524), 32);
            } else {
                strncat(dst, label_get(457), 32); // Governor
            }
            strncat(dst, " ", 32);
        }
    }
    if (strlen(label_base_surplus)) {
        snprintf(buf, StrBufLen, label_base_surplus,
            base->nutrient_surplus, base->mineral_surplus, base->energy_surplus);
        strncat(dst, buf, StrBufLen);
    }
}

void __thiscall ReportWin_draw_ops_color(Buffer* This, int UNUSED(a2), int a3, int a4, int a5)
{
    BASE* base = *CurrentBase;
    int color = (base->mineral_surplus < 0 || base->nutrient_surplus < 0
        ? ColorRed : ColorMediumBlue);
    Buffer_set_text_color(This, color, a3, a4, a5);
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
        strncat(dest, Bases[base_id].name, MaxBaseNameLen);
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

// Labels that are navigable menus — announce only the menu name, not body.
// Returns a friendly name for the menu, or NULL if not a menu label.
static const char* sr_menu_name(const char* label) {
    static const struct { const char* label; const char* name; } menus[] = {
        {"TOPMENU",       "Main Menu"},
        {"MAPMENU",       "Map Menu"},
        {"MULTIMENU",     "Multiplayer"},
        {"SCENARIOMENU",  "Scenario"},
        {"MAINMENU",      "Thinker Menu"},
        {"GAMEMENU",      "Game Menu"},
        {"WORLDSIZE",     NULL},  // NULL = read #caption from file
        {"WORLDLAND",     NULL},
        {"WORLDTIDES",    NULL},
        {"WORLDORBIT",    NULL},
        {"WORLDLIFE",     NULL},
        {"WORLDCLOUD",    NULL},
        {"WORLDNATIVE",   NULL},
    };
    for (int i = 0; i < (int)(sizeof(menus) / sizeof(menus[0])); i++) {
        if (_stricmp(label, menus[i].label) == 0) {
            return menus[i].name ? menus[i].name : "";
        }
    }
    return NULL;  // not a menu
}

int __thiscall mod_BasePop_start(
void* This, const char* filename, const char* label, int a4, int a5, int a6, int a7)
{
    if (!sr_popup_is_active() && sr_is_available() && filename && label) {
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
                sr_output(buf, true);
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




