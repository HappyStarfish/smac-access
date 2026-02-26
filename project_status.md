# SMAC Accessibility Mod - Project Status

## Current Phase: Phase 3 — Unified Announce System + Game Flow

## Completed

### Phase 1: Analysis (done)
- Game structure, text rendering, menus, tutorials documented
- Key bindings documented in docs/keybindings.md (comprehensive)

### Phase 2: Basic Framework (done)
- Tolk dynamically loaded, NVDA confirmed
- 13 inline hooks on Buffer_write/wrap functions
- Ctrl+R read, Ctrl+Shift+R silence, Ctrl+F12 debug toggle
- Junk filter, dedup, prefix dedup, auto-clear (100ms gap)

### Phase 3: Unified Announce System (in progress)
- Four-trigger system: Snapshot, Arrow Nav, Timer, World Map Important
- HUD noise filter (HUD bar, info panel, terrain, economic panel)
- Arrow nav boundary detection (HUD vs menu items)
- Screen transition announcements (World Map, Base Screen, Design Workshop)
- World map position tracking (MAP-MOVE with terrain description)
- Player turn detection (unit selection announcement)
- Snapshot cap (>10 items suppressed, e.g. base screen buttons)
- Timer cap (max 5 new items per announcement)
- Map bounds validation (prevents garbage coordinate access)

## What Works

- All menu navigation via arrows (main menu, game setup, research priorities)
- Screen transition announcements ("World Map", "Base Screen", etc.)
- Research Priority selection (EXPLORE/DISCOVER/BUILD/CONQUER navigable)
- Arrow boundary detection (last/first item correctly re-announced)
- HUD data filtered from all announcements
- MAP-MOVE terrain info on world map cursor change
- Turn announcement: "Your Turn. Colony Pod at (107, 45)"
- Faction selection screen (all factions + leaders read)
- Build and deployment pipeline (GCC 14.2.0 msvcrt, stable)
- **Enhanced tile announcements** — terrain, moisture, altitude, features (incl. magtube/soil enricher/echelon mirror), resource yields, ownership, base radius, unexplored check
- **World map exploration cursor** (arrows + Home/End/PgUp/PgDown) — tile info announced
- **Step-by-step unit movement** (Shift+arrows/nav keys) — uses action() for one tile at a time
- **Movement points announcement** on unit selection (queued after unit name)
- **Context help** (Ctrl+F1) — announces available commands for current unit/terrain
- **Go-To cursor position** (Shift+Space) — sends unit to cursor, needs testing
- **Scanner mode** (Ctrl+Left/Right/PgUp/PgDn) — jump to points of interest, 10 filter categories — TESTED OK

## Current Hotkeys

- Ctrl+R: Read captured screen text
- Ctrl+Shift+R: Stop speech (silence)
- Ctrl+F12: Toggle debug logging (%TEMP%\thinker_sr.log)
- **World Map — Exploration (no modifier):**
  - Arrow keys: move cursor N/S/W/E
  - Home/End/PgUp/PgDown: move cursor NW/SW/NE/SE
  - Announces: "(x, y) Terrain, Features. Base: Name. Unit: Name"
- **World Map — Unit Movement (Shift held):**
  - Shift+Arrow keys: move unit N/S/W/E
  - Shift+Home/End/PgUp/PgDown: move unit NW/SW/NE/SE
  - Shift+Space: send unit to cursor position (Go To)
  - Uses set_move_to() + action() (step-by-step, one tile at a time)
  - Announces tile info + remaining movement points after each step
- **Ctrl+F1: Context-sensitive help** (unit commands + terrain actions)
- **World Map — Scanner (Ctrl key):**
  - Ctrl+Left: Jump to previous matching tile
  - Ctrl+Right: Jump to next matching tile
  - Ctrl+PgUp: Previous filter category
  - Ctrl+PgDn: Next filter category
  - 10 filters: All, Own Bases, Enemy Bases, Enemy Units, Own Units, Own Formers, Fungus, Pods/Monoliths, Improvements, Terrain/Nature
- **World Map — Menu Bar (Alt key):**
  - Alt: Open menu bar, announce first menu (Game)
  - Alt+Left/Right: Navigate between menus (with shortcut summaries)
  - Alt+Down or Alt+Enter: Open dropdown (game handles submenu)
  - Escape: Close menu bar mode

## Known Issues

1. ~~**ABOUT RESEARCH popup**~~: RESOLVED (2026-02-22). Tutorial body now read via sr_read_popup_text with #caption extraction. Tutorial announce time updated at popup close (not open) to prevent worldmap interruption.
2. **Unit movement needs more testing**: set_move_to() + mod_veh_skip() approach is new — user said "glaube es funktioniert, aber noch nicht richtig". May need adjustments to how/when movement is executed. Previous attempts (PostMessage VK_NUMPAD, WinProc passthrough) failed: game interpreted Shift+Arrow as view scrolling, not unit movement.
3. **HUD character-by-character drawing**: Prevents Timer trigger from stabilizing on world map. Worldmap Important trigger (whitelist) handles this case.
4. **Hotseat vs Single Player**: Quick Start creates hotseat games. User must use START GAME for single-player.
5. ~~**No localization system**~~: RESOLVED — loc() system implemented with sr_lang/ files.
6. **German text in multiplayer menu**: Reported but could not reproduce from game files (all English). Needs investigation during testing.
7. ~~**Umlauts broken with German language patch**~~: RESOLVED (2026-02-25). Game text (Windows-1252) was passed to sr_output(CP_UTF8) without conversion. Fixed with ANSI→UTF-8 conversion at all entry points.

## Resolved Issues

1. **Game crashes** (RESOLVED 2026-02-14): Caused by wrong compiler. GCC 15.2.0 (ucrt) generates incompatible code. Fixed by switching to GCC 14.2.0 (msvcrt) as specified by Thinker author.
2. **Missing modmenu.txt** (RESOLVED 2026-02-14): File from Thinker distribution was missing in game directory. Original Thinker code shows MessageBox (invisible to screen reader), blocking startup.
3. **Garbage Mission Year value**: HUD filter now allows long Mission Year messages through, filters only short ones.
4. **Crash on rapid Enter** (RESOLVED 2026-02-21): sr_walk_text_tree() crashed when game's red-black text tree was modified during traversal. Fixed with sr_node_ok() + IsBadReadPtr validation.

## Architecture

### Announce System (gui.cpp)
Four triggers in ModWinProc:
1. **Snapshot**: Fires on draw cycle boundary (>100ms gap). Catches fast transitions and tutorials. Suppressed during arrow nav, on world map (GW_World), and on base screen. Capped at 10 items. HUD filter applied.
2. **Arrow Nav**: 50ms after arrow keypress (independent of HUD timing). Uses item[1] for navigation, item[0] at boundaries (HUD detection).
3. **Timer**: 300ms stable. Suppressed on world map (can't stabilize due to HUD). Max 5 new items. HUD filter applied.
4. **World Map Important**: 500ms poll. WHITELIST approach — each item individually checked:
   - "ABOUT ..." / "ÜBER ..." (tutorial popups)
   - "...need new orders..." / "...neue Befehle..." (turn status)
   - "...Press ENTER..." / "...Eingabe dr..." (action prompts)
   - Everything else silently ignored. Prevents char-by-char noise and map rendering text.

### HUD Noise Filter (sr_is_hud_noise)
Filters from triggers 1-3:
- HUD bar: Mission Year (short only, <30 chars), Econ:, Psych:, Labs:
- Info panel: Energy:, Unexplored, coordinates "(x , y)", Elev:, ENDANGERED, (Gov:
- Terrain descriptors: Rolling, Flat, Rocky, Rainy, Arid, Moist, Xenofungus (short strings only)
- Economic panel: Commerce, Gross, Total Cost, NET
- Partial HUD build-up: Mis(<12), Eco(<5), Psy(<6), Lab(<5)

### Screen State Detection
- current_window(): GW_None=0, GW_World=1, GW_Base=2, GW_Design=3
- PopupDialogState: 0=menu/dialog, >=2=world map gameplay
- on_world_map: MapWin && (GW_World || (GW_None && popup>=2))
- Note: Base screen opened from world map may still report GW_World (win=1)

### Map Bounds Validation
- Before accessing MapWin->iTileX/iTileY, checks MapAreaX/MapAreaY > 0
- Coordinates validated: 0 <= x < MapAreaX, 0 <= y < MapAreaY
- Garbage coordinates (during init) cause reset, not crash

### Player Turn Detection
- Tracks MapWin->iUnit changes (with UNIT-CHANGE debug logging)
- Announces when selected unit belongs to MapWin->cOwner (player faction)
- Safety: Vehs!=NULL, VehCount>0, iUnit < VehCount, unit_id>=0
- Fallback: Worldmap Important trigger catches "No units need new orders"

## Build Setup

- **Compiler:** GCC 14.2.0 msvcrt (CRITICAL: not 15.x, not ucrt!)
- **Compiler path:** C:\Users\Sonja\Downloads\mingw32-14.2.0\mingw32\bin\
- **CMake/Ninja path:** C:\Users\Sonja\Downloads\mingw32\mingw32\bin\
- **Required game file:** modmenu.txt (from Thinker distribution, in game directory)

## Key Source Files

- src/gui.cpp — ModWinProc dispatcher, announce system (triggers 1-3), popup list navigation, global SR shortcuts
- src/world_map_handler.cpp — World map SR features: exploration cursor, targeting, scanner, map tracking, unit announce, Trigger 4
- src/world_map_handler.h — WorldMapHandler namespace declarations
- src/menu_handler.cpp — Menu bar navigation (Ctrl+F2), submenu nav, mcache for dialog arrow nav
- src/menu_handler.h — MenuHandler namespace declarations
- src/base_handler.cpp — BaseScreenHandler (7 sections incl. Units, enhanced data, Ctrl+Up/Down nav, Ctrl+I repeat, Ctrl+F1 help)
- src/base_handler.h — BaseScreenHandler declarations
- src/screen_reader.cpp — Tolk loading, 13 inline hooks, text capture buffer
- src/screen_reader.h — Screen Reader API
- src/social_handler.cpp — SocialEngHandler (SE screen accessibility)
- src/social_handler.h — SocialEngHandler declarations
- src/prefs_handler.cpp — PrefsHandler (Preferences screen accessibility, 6 tabs, ~70 options)
- src/prefs_handler.h — PrefsHandler declarations
- src/specialist_handler.cpp — SpecialistHandler (citizen/specialist management, Ctrl+W)
- src/specialist_handler.h — SpecialistHandler declarations
- src/design_handler.cpp — DesignHandler (Design Workshop accessibility, Shift+D)
- src/design_handler.h — DesignHandler declarations
- src/localization.h — SrStr enum, loc(), loc_init()
- src/localization.cpp — English defaults, UTF-8 file loader, key mapping
- sr_lang/en.txt — English SR strings (~210 strings)
- sr_lang/de.txt — German SR strings (~210 strings)
- docs/keybindings.md — Complete SMAC keybindings reference
- docs/game-api.md — Game API documentation

## Notes for Next Session

### Pending Tests

0z. **Specialist management (Ctrl+W)** — PARTIALLY TESTED. Ansagen kommen. Left/Right: Worker sagt "Arbeiter haben keinen Typ", Specialist mit nur 1 Typ sagt "Keine weiteren Typen". Offen: Abgleich mit Quellcode nötig — Nutzer unsicher ob Verhalten spielkonform ist. Nächste Session: OpenSMACX/Thinker Quellcode prüfen für korrektes Specialist-Verhalten.
0k. **Design Workshop (Shift+D)** — TESTED OK (2026-02-26). Two-level navigation works. Kostenanzeige zeigt Grundkosten (ohne Prototyp-Zuschlag) — ist korrekt, Aufschlag kommt erst beim Bauen.
0a. **Preferences handler (Ctrl+P)** — NOT YET TESTED.
0b. **Targeting mode (J, P, F, Ctrl+T)** — NOT YET TESTED.
0c. **Go to base (G)** — NOT YET TESTED.
0d. **Note: Ctrl+R conflict** — Ctrl+R currently means "read screen". Game also uses Ctrl+R for "Road to" targeting. Not intercepted yet — needs decision on key mapping.
0e. **Facility demolition (Ctrl+D)** — TESTED OK (2026-02-23).
0f. **Interlude story text** — TESTED OK (2026-02-23). Full narrative text announced with title. BUT umlauts still broken (see 0h).
0g. **Diplomacy: F12 Commlink dialog** — TESTED OK (2026-02-25). F12 opens accessible faction list, Enter initiates contact. Working pattern: diplo_second_faction + diplo_lock + diplomacy_caption + communicate(player, fid, 1). Five approaches tested before success (documented in game-api.md). S/Tab summary and DiplomacyHandler open/close detection TESTED OK (2026-02-25). Remaining untested: Ctrl+F1 help during active diplomacy, NetMsg_pop treaty announcements.
0h. **Interlude umlaut fix** — NOT YET TESTED (2026-02-25). Fixed sr_ansi_to_utf8 fallback (was copying raw ANSI when UTF-8 exceeded buffer). Enlarged popup text buffer to 8192 bytes. User reported "bl ttern" instead of "blättern" in interlude text.
0i. **Variable substitution fix** — TESTED (2026-02-26). $<M1:$CHARACTERADJ9> and $<F2:$FOLLOWERADJ5> now resolved. Letter-prefixed $<...> patterns strip wrapper, inner $WORD# resolved next iteration. Diplomacy text confirmed working.
0j. **Number input dialog (pop_ask_number)** — NOT YET IMPLEMENTED. "Größe des Geschenks?" input field not announced as input, typed digits not read. Needs hook on pop_ask_number (0x627C30).
0l. **Former terraform announcements** — TESTED OK (2026-02-26). Order given ("Building Forest, 4 turns"), status on unit select ("Building Forest, 2 of 4 turns"), completion ("Forest completed").
0m. **Governor priorities (Ctrl+G)** — TESTED OK (2026-02-26). Priority selector as first item (Explore/Discover/Build/Conquer/None), Left/Right/Space to cycle.
0. **Scanner mode** — TESTED OK (2026-02-19).
1. **Enhanced tile announcements** — TESTED OK (2026-02-19).
1b. **Social Engineering handler (E key)** — TESTED OK (2026-02-22). Modal loop approach, Enter confirms, Escape cancels.
1c. **Tutorial popup fix** — TESTED OK (2026-02-22). Tutorial body text no longer interrupted by worldmap HUD.
1d. **Base naming prompt** — TESTED OK (2026-02-22). "Enter a name for your new base. Name:" now announced.
1e. **Planetfall variables** — TESTED OK (2026-02-22). $BASENAME0 and $UNITNAME1 resolved.
1f. **Menu name announcements** — TESTED OK (2026-02-22). "Main Menu", "Select Size of Planet" etc.
2. **Production queue management (Ctrl+Q)** — TESTED OK (2026-02-22).
3. **Production picker detail (D key)** — TESTED OK (2026-02-22).
4. **Menu bar navigation** — TESTED OK (2026-02-22).
5. **Tour button accessibility** — TESTED OK (2026-02-22).
6. **Setup option accessibility** — WORLDSIZE, WORLDLAND etc. have options + Random button. Need to make navigable.
7. If step movement has issues, fallback: remove action() call, let game loop process ORDER_MOVE_TO naturally.

## Session Log

- **2026-02-06**: Phase 2 complete.
- **2026-02-08**: Documentation, dialog announce, major debugging, 13 hooks working.
- **2026-02-11 (session 1)**: Holistic announce rewrite — three-trigger system, Y-coordinate tracking, arrow nav with item[1] selection.
- **2026-02-11 (session 2)**: HUD noise filter, arrow boundary fix (HUD detection), snapshot cap (>10 items), screen transition announcements, world map MAP-MOVE tracker, player turn detection. Keybindings documented in docs/keybindings.md.
- **2026-02-11 (session 3)**: Three rounds of fixes. (1) Mission Year HUD filter allows long status messages, MAP-MOVE VEH bounds check, UNIT-CHANGE debug logging, new Trigger 4 worldmap periodic announce. (2) Worldmap trigger redesigned as WHITELIST (only tutorials/turn status/action prompts). Expanded HUD filter: terrain descriptors, economic panel, Elev:, ENDANGERED, (Gov:. Map bounds validation (MapAreaX/MapAreaY).
- **2026-02-14**: Crash debugging marathon. Isolated crash cause: GCC 15.2.0 (ucrt) compiler produces incompatible code. Switched to GCC 14.2.0 (msvcrt) per Thinker author's recommendation. Also found missing modmenu.txt caused invisible MessageBox blocking startup. Added sr_all_disabled() / no_hooks.txt for crash isolation. Added persistent hook log (%TEMP%\thinker_hooks.log). Fixed SIB byte handling in calc_stolen_bytes. Game now stable — extended play session confirmed (5000+ log lines, no crash).
- **2026-02-15 (session 2)**: BaseScreenHandler implemented. New files: base_handler.h/cpp. On base open: announces name, population, current production with progress. Ctrl+Up/Down cycles 6 info sections (Overview, Resources, Production, Economy, Facilities, Status). Ctrl+I repeats current section. Ctrl+Left/Right tab switch now announces tab name. OnClose() on leaving base screen. Build OK, DLL copied — awaiting user testing.
- **2026-02-15 (session 4)**: Missing screen text fixes. (1) Quit dialog: Escape on world map now reads #REALLYQUIT text from Script.txt before game opens dialog. (2) Planetfall intro: hooked planetfall() at 0x589180 — reads full #PLANETFALL/#PLANETFALLF text from script file. Standard factions get #PLANETFALL, SMAX factions (id>=7) get #PLANETFALLF. Exposed sr_read_popup_text() as non-static for use from gui.cpp. Total hooks now 19 (13 text + 5 popup/planetfall + 1 write_strings).
- **2026-02-15 (session 6)**: Localization system implemented. New files: localization.h/cpp, sr_lang/en.txt, sr_lang/de.txt. All 63 SR strings extracted from gui.cpp, base_handler.cpp, screen_reader.cpp into loc(SR_...) calls. Language configurable via thinker.ini sr_language= setting. UTF-8 support (CP_ACP changed to CP_UTF8 in sr_output). Fallback to built-in English defaults if file/key missing. CLAUDE.md updated with localization rules.
- **2026-02-17**: Three new features. (1) Movement points: integrated into turn announcement — "Your Turn. Scout Patrol at (12, 60). 1 of 1 moves". Initially queued separately but got swallowed by next interrupt; now combined into single string. (2) Step-by-step movement: replaced mod_veh_skip() with action() in Shift+Arrow handler. Unit moves one tile, announces tile + remaining moves. "Cannot move there" on failure. First test showed movement works (TILE-ANNOUNCE in log), then game auto-advances to next unit when moves exhausted. (3) Context help: initially Ctrl+F1, but F1 intercepted by game's own help. Changed to Shift+F1. 30 new localization strings (en + de). **All three features need testing next session.**
- **2026-02-19**: Menu bar accessibility + noise fixes. (1) Menu bar navigation: Alt activates, Alt+Left/Right cycles through 9 menus with localized shortcut summaries, Alt+Down/Enter opens dropdown via simulated click on hitbox. Escape exits. Uses WM_SYSKEYDOWN handler. (2) HUD noise filter: added GAME, NETWORK, ACTION, TERRAFORM, SCENARIO, EDIT MAP, HELP to sr_is_hud_noise — prevents random menu bar text announcements. (3) Junk filter: added "No Contact"/"no contact" to sr_is_junk — prevents multiplayer status text from interrupting menu navigation. 10 new localization strings (en + de). Build OK, awaiting testing.
- **2026-02-19 (session 2)**: Scanner mode implemented. Ctrl+Left/Right jumps to next/prev point of interest on world map. Ctrl+PgUp/PgDn cycles 10 filters (All, Own Bases, Enemy Bases, Enemy Units, Own Units, Own Formers, Fungus, Pods/Monoliths, Improvements, Terrain/Nature). Linear row-by-row scan, wraps around map. 11 new loc strings (en+de). Only active on world map (no conflict with Base Screen Ctrl+Left/Right).
- **2026-02-19 (session 3)**: Base screen accessibility overhaul. (1) Enhanced all 6 existing sections with richer data: Overview now shows coordinates, worker/specialist breakdown by type (Citizen[].singular_name); Resources shows growth turns and nutrient box progress; Production shows turns to completion; Economy shows faction energy credits; Facilities shows count and total maintenance; Status unchanged but uses same framework. (2) New Units section (BS_Units): lists all VEHs at base coordinates with names. (3) Ctrl+Left/Right tab switch now also reads matching section content (queued, non-interrupting). (4) Ctrl+F1 base help: announces available base screen commands. (5) OnOpen enhanced with production turns info. 15 new loc strings (en+de). Build OK, awaiting testing.
- **2026-02-15**: World map navigation overhaul. (1) Virtual exploration cursor: arrows + Home/End/PgUp/PgDown move cursor on diamond grid, announce tile info (terrain, features, bases, units). Extracted sr_announce_tile() helper (replaces duplicated MAP-MOVE code). (2) Unit movement: Shift+nav keys. Three iterations — PostMessage VK_NUMPAD failed, WinProc passthrough failed (game treats Shift+Arrow as view scroll), final approach uses set_move_to() + mod_veh_skip() to directly set unit waypoint. (3) Shift+Space = Go-To cursor position. (4) UNIT-CHANGE now updates cursor to new unit position. Cursor initializes to unit position on world map entry. SMAC coordinate system: diamond grid with BaseOffsetX/Y from path.h (N=0,-2 / NE=1,-1 / E=2,0 / SE=1,1 / S=0,2 / SW=-1,1 / W=-2,0 / NW=-1,-1), wrap() for X.
- **2026-02-21 (session 2)**: Production picker enhancements. (1) Current production announcement: When opening picker (Ctrl+Shift+P), first announces what's currently being built with mineral progress and turns, then the picker open message. (2) D key for item details: In picker, pressing D announces full details — facilities get name, effect, cost, maintenance; secret projects get name, effect, cost, "Secret Project"; units get chassis, weapon+attack, armor+defense, reactor, speed, HP, and abilities list. 5 new loc strings (en+de). Build OK, awaiting testing.
- **2026-02-21 (session 3)**: Production queue management. Ctrl+Q opens queue mode in base screen. Up/Down navigates items (wrapping), item 0 shows mineral progress + "Current production". Shift+Up/Down reorders queued items (not item 0). Delete removes items (not item 0). Insert opens production picker in insert mode — selected item gets inserted after current position, returns to queue mode. D for item details. Escape closes. 12 new loc strings (en+de). Queue uses base->queue_items[0..queue_size] directly (no game function calls needed). Build OK, awaiting testing.
- **2026-02-21**: Crash fix + base screen test. (1) User tested base screen features: Ctrl+Left/Right tabs, Ctrl+I repeat, Ctrl+F1 help — all work. Ctrl+Up/Down section navigation needs retesting (user tried PgUp/PgDown initially). (2) Crash fix: Game crashed when pressing Enter rapidly. Root cause: sr_walk_text_tree() in screen_reader.cpp traversed an MSVC6 STL red-black tree (game's text rendering data). When the tree was modified/freed by the game engine during traversal, corrupted pointers (0x41414141 = "AAAA") caused ACCESS_VIOLATION. Fix: new sr_node_ok() function adds IsBadReadPtr(node, 52) check before dereferencing any tree node, ensuring the full node structure is readable. Crash confirmed fixed by user. (3) Reviewed all other hook functions for similar issues — no other vulnerable patterns found. (4) build.ps1 kept as reusable build script (workaround for broken bash shell in Claude Code).
- **2026-02-22**: Social Engineering handler — complete rewrite to modal loop approach. Original overlay approach (detecting SocialWin visibility) failed: game's social_select runs its own modal loop that cannot be exited programmatically (4 close methods tried: GraphicWin_close, SubInterface_release, WIN_VISIBLE flag clearing, social_select re-call — all failed). Solution: intercept 'E' key in ModWinProc before game sees it, run our own PeekMessage modal loop in RunModal(). Game's social_select is never called. Critical bug: WM_CHAR 'e' (from TranslateMessage) leaked into our loop and triggered social_select through game's WinProc — fixed by consuming WM_CHAR during active handler. Second bug: WM_CHAR from Enter/Escape leaked after loop exit, opening bases — fixed by draining WM_CHAR/WM_KEYUP after loop. Final design: Left/Right directly selects available model (no Enter needed), Enter confirms+closes, Escape cancels+closes. Up/Down cycles categories, S/Tab summary, Ctrl+I repeat, Ctrl+F1 help. Learnings documented in docs/game-api.md. TESTED OK.
- **2026-02-21 (session 4)**: Generic popup list navigation. Popup dialogs (research priorities, etc.) don't re-render text on arrow navigation — game only moves a highlight bar. Implemented `sr_popup_list_parse()` that reads script files directly and extracts selectable options. Arrow keys in popup now announce "N of Total: OptionText" via screen reader. Fixed double `.txt` extension bug (game passes "SCRIPT.txt", function appended another ".txt"). 1 new loc string (popup_list_fmt, en+de). GitHub repo created at HappyStarfish/smac-access. TESTED OK.
- **2026-02-22 (session 2)**: Tutorial popup + base naming + menu names. (1) Tutorial interruption fix: sr_tutorial_announce_time was set at popup OPEN (when text first renders), but if user reads popup for 4+ seconds, the 3s suppression window expired by close time. Fix: all 4 popup hooks (popp/popb/X_pop/X_pops) now update sr_tutorial_announce_time to GetTickCount() AFTER the blocking call returns. Also added same check to Trigger 1 (Snapshot). (2) Base naming prompt: Base naming dialog goes through pops/pop_ask → BasePop_start, NOT through popp. Added sr_read_popup_text call to mod_BasePop_start for popups not already handled by popp/popb hooks (uses sr_popup_is_active() to avoid double announcements). (3) #caption extraction: sr_read_popup_text now extracts #caption text and prepends it to body (was previously skipped as directive). (4) Variable substitution: Generic $WORD# pattern substitution via ParseStrBuffer in sr_read_popup_text. Handles $BASENAME0, $UNITNAME1, $NUM0 etc. from game's parse_says slots. (5) Menu name announcements: sr_menu_name() maps labels to friendly names (TOPMENU→"Main Menu", etc.). WORLD* labels read #caption from file. Prevents full menu content from being announced. (6) Worldmap initial position: sr_map_x/y initialized to current position on worldmap entry — tile announced only on actual movement, not on entry. All tested OK by user.
- **2026-02-22 (session 3)**: Preferences handler (Ctrl+P). Same modal loop pattern as SocialEngHandler. Intercepts Ctrl+P before game sees it, runs PeekMessage loop. 6 tabs (General, Warnings, Advanced, Automation, Audio/Visual, Map Display) with ~70 options total. Left/Right switches tabs, Up/Down navigates options (wrapping), Space toggles on/off. Enter saves (syncs to AlphaIniPrefs + prefs_save), Escape restores backup values. S/Tab for tab summary, Ctrl+I repeat, Ctrl+F1 help. ~80 new loc strings (en+de). New files: prefs_handler.h/cpp. Build OK, awaiting testing.
- **2026-02-22 (session 5)**: Specialist management handler (Ctrl+W). New files: specialist_handler.h/cpp. Modal loop pattern (same as SocialEngHandler/PrefsHandler). Activated with Ctrl+W in base screen. Citizens displayed as workers (with tile yields) and specialists (with type name and bonuses). Up/Down navigates all citizen slots (wrapping). Enter toggles worker↔specialist. Left/Right cycles specialist type (only available types). S/Tab repeats summary. Ctrl+I repeats current slot. Ctrl+F1 help. Escape closes. Worker→Specialist: clears worked_tiles bit, increments specialist_total, sets best_specialist type. Specialist→Worker: decrements specialist_total, game auto-assigns tile via base_compute(1). Base center (bit 0) protected. Min base size check uses same logic as game (Rules->min_base_size_specialists, psych_bonus>=2 exception). 16 new loc strings (en+de). Base help updated to include Ctrl+W. Bugfixes: (1) Removed GraphicWin_redraw from modal loop exit (caused random text noise). (2) Added sr_modal_active guard to all 4 announce triggers. (3) Left/Right on worker slots now says "Workers have no type", on specialist with only 1 available type says "No other types available". Partially tested — user unsure if specialist behavior matches game intent. Next: verify against OpenSMACX/Thinker source.
- **2026-02-22 (session 4)**: Targeting mode + Go to base. (1) Targeting mode: When J (Group Go to), P (Patrol), F (Long range fire), or Ctrl+T (Tube to) are pressed on world map, sr_targeting_active flag is set and key is passed to game. While flag is active, arrow keys pass through to game's internal cursor (not captured by exploration cursor). Map position tracking (sr_map_x/y) announces tile changes from game cursor. Escape cancels, Enter confirms, auto-reset on screen change or 30s timeout. (2) Go to base (G): Intercepts G before game, opens modal PeekMessage loop with list of own bases sorted by distance. Up/Down navigates with wrapping, Enter sends unit via set_move_to + mod_veh_skip. (3) Shift+G: Goes directly to home base (veh->home_base_id). (4) Ctrl+R conflict noted: game uses Ctrl+R for "Road to" targeting, our mod uses it for "read screen" — deferred for later resolution. 8 new loc strings (en+de). Build OK, awaiting testing.
- **2026-02-23**: Facility demolition + interlude story text. (1) Facility demolition mode (Ctrl+D): New demolition sub-mode in base screen. Up/Down navigates built facilities, Delete demolishes with two-press confirmation, D for facility detail, Escape exits. Checks BSTATE_FACILITY_SCRAPPED for one-per-turn limit. Uses set_fac() for proper removal + base_compute(1) recalc. 8 new loc strings (en+de). Required gui.cpp dispatch entries (handler code alone not sufficient — keys must be intercepted in ModWinProc). (2) Popup "Enter to continue": Info-only popups (0-1 options) now append "Enter to continue" to text. Appended directly to string (separate sr_output was too late in chained popups). (3) Interlude story text: Hooked interlude() at 0x5230E0 via install_inline_hook to capture interlude ID. sr_pre_popup detects INTERLUDEX popups and reads numbered story section (e.g. INTERLUDE21) instead of generic header. Fixed ^ paragraph marker handling (was discarding text after ^, now strips ^ and keeps text). Critical bug: `interlude = sr_orig_interlude` in hook install bypassed the hook entirely (mod calls through function pointer) — fixed by not redirecting the pointer. Both features TESTED OK.
- **2026-02-22 (session 6)**: gui.cpp refactoring — SR code extraction. Extracted ~1200 lines of screen reader code from gui.cpp (3649→2457 lines) into two new handler files. (1) world_map_handler.h/cpp (937 lines): All world map SR features — exploration cursor, targeting mode, scanner, map position tracking, unit change detection, Trigger 4 worldmap polling, all world map key handlers (arrows, scanner, targeting, G/Shift+G, E, Ctrl+P, Escape, Shift+F1). (2) menu_handler.h/cpp (361 lines): Menu bar navigation (Ctrl+F2), submenu navigation, mcache dialog arrow caching. gui.cpp now serves as thin dispatcher — calls WorldMapHandler::OnTimer() for timer block, WorldMapHandler::HandleKey() and MenuHandler::HandleKey() for key events. Pure mechanical refactoring, no functional changes. CMakeLists.txt auto-detects new files via GLOB. Build clean (0 warnings). DLL copied.
- **2026-02-25 (session 2)**: Diplomacy accessibility + variable substitution. (1) F12 Commlink: accessible faction list with modal PeekMessage loop (same pattern as Go-to-Base). Five contact approaches tried — communicate(p,f,0) empty, commlink_attempt(f) silent, commlink_attempter(p,f) own popup, finally working: diplo_second_faction + diplo_lock + diplomacy_caption + communicate(p,f,1). TESTED OK. (2) Variable substitution fix: extracted sr_substitute_game_vars() shared function from sr_read_popup_text. Fixed underscores in var names ($TO_CARRY_OUT_OUR_MISSION6). Added $<N:form0:form1:...> gender/plurality pattern (German 6-form: gender + plurality*3). Added substitution to sr_popup_list_parse for dialog options ($TITLE3, $NAME4, $TECH0). (3) DiplomacyHandler: new files diplo_handler.h/cpp. IsActive() via DiploWinState, OnTimer() detects open/close, HandleKey() for S/Tab summary + Ctrl+F1 help. (4) NetMsg_pop SR output for treaty notifications. (5) Documented full diplomacy API in game-api.md. 20 new loc strings (en+de).
- **2026-02-25**: German language patch support + umlaut fix. (1) ANSI→UTF-8 conversion: Game text (Windows-1252) was passed to sr_output(CP_UTF8) without encoding conversion, causing umlauts to appear as question marks. Added sr_ansi_to_utf8() and sr_game_str() helpers to screen_reader.h/cpp. Applied conversion at all entry points: sr_record_text (13 hooks), sr_read_popup_text (game text files), sr_popup_list_parse (popup options), sr_try_tutorial_announce (tutor.txt body text). Wrapped ~25 game string locations in handlers (Facility[].name/effect, Bases[].name, Vehs[].name(), Chassis/Weapon/Armor/Reactor/Ability/Citizen names, Tech[].name, prod_name()) with sr_game_str(). (2) de.txt umlauts: Replaced all ASCII approximations (oe→ö, ue→ü, ae→ä, ss→ß) in sr_lang/de.txt (~210 strings). (3) HUD filter German support: Added German equivalents for all English HUD noise keywords (Missionsjahr, Energie:, Wirt:, Fors:, terrain descriptors, menu bar items, economic panel). Added German junk filter entries (ZUG BEENDET, Kein Kontakt). (4) Auto language detection: loc_init() now defaults to sr_language=auto. Checks alphax.txt for German content indicators. Automatically loads de.txt for German game installs, en.txt otherwise. Manual override still possible via thinker.ini. TESTED OK — user confirmed umlauts work in tutorials, game text, and mod text.
- **2026-02-26 (session 2)**: Former terraform accessibility + governor priorities + text filter fixes. (1) Former terraform announcements: polling-based in OnTimer via state tracking (sr_prev_order/sr_prev_order_unit). get_terraform_name() helper uses Terraform[order-4].name/name_sea + is_ocean(). Three announcements: order given ("Building Forest, 4 turns"), status on unit select ("Building Forest, 2 of 4 turns"), completion ("Forest completed"). movement_turns counts down from rate. 3 new loc strings (en+de). TESTED OK. (2) Governor priorities: Added priority selector (Explore/Discover/Build/Conquer/None) as first item in Ctrl+G handler. Left/Right/Space cycles priority (radio-button style, mutually exclusive). Priority bits now saved on confirm. Summary includes priority. 6 new loc strings (en+de). TESTED OK. (3) Variable substitution fix: $<M1:$CHARACTERADJ9> and $<F2:$FOLLOWERADJ5> patterns now resolved — letter-prefixed $<...> strips wrapper, inner $WORD# resolved next iteration. (4) Worldmap text filter: Removed has_about flag that made ALL captured items important. Each item now individually checked against whitelist. Added German patterns (ÜBER, neue Befehle, Eingabe). (5) Snapshot suppression on world map: Added GW_World check to gui.cpp snapshot discard — worldmap text (landmarks, info panel) no longer announced via snapshots. Trigger 4 whitelist handles important messages. Fixed "Martialische Luft / Sunny Mesa" noise.
- **2026-02-26**: Design Workshop accessibility handler (Shift+D). New files: design_handler.h/cpp. Same modal loop pattern as SocialEngHandler. Two-level navigation: Level 1 = prototype list (default units with tech + custom units), Up/Down navigate, Enter edit, N new, Delete retire (two-press confirm), Escape close. Level 2 = component editor with 6 categories (Chassis/Weapon/Armor/Reactor/Ability1/Ability2), Left/Right cycles categories, Up/Down cycles tech-filtered options, S summary, Enter saves via mod_make_proto+mod_name_proto, Escape cancels back to list. Ability compatibility filtering via AFLAG_* flags (triad, combat/terraform/probe checks). Cost calculated via mod_proto_cost. Editing default units creates a copy in faction's custom slot. 26 new loc strings (en+de). Wired via Shift+D in world_map_handler.cpp, added to modal active check in gui.cpp. Build OK, deployed, awaiting testing.
