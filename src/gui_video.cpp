
#include "gui.h"

// PRACX-derived pointers defined in gui.cpp
extern HWND* phWnd;
extern HINSTANCE* phInstance;

int __cdecl mod_Win_init_class(const char* lpWindowName)
{
    int value = Win_init_class(lpWindowName);
    set_video_mode(0);
    return value;
}

/*
Hook for AlphaNet_setup's call to create_game (0x4E2E50).
When sr_net_start_requested is set (by MultiplayerHandler Start button),
override the return value to 1 (success) so AlphaNet_setup proceeds
with game start instead of returning to the main menu.
*/
int __thiscall mod_create_game(void* This) {
    typedef int (__thiscall *FCreateGame)(void*);
    FCreateGame orig_create_game = (FCreateGame)0x4E2E50;
    int result = orig_create_game(This);
    if (sr_net_start_requested) {
        sr_net_start_requested = false;
        sr_debug_log("NETSETUP: create_game hook override %d -> 1", result);
        return 1;
    }
    return result;
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

#pragma GCC diagnostic pop

static void popup_homepage()
{
    ShellExecute(NULL, "open", "https://github.com/induktio/thinker", NULL, NULL, SW_SHOWNORMAL);
}

static void show_mod_stats()
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

static int show_mod_config()
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
        return 0;
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
    return 0;
}
