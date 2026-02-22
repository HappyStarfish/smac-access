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

1. **ABOUT RESEARCH popup**: Only title "ABOUT RESEARCH" is read, body text missing. Body text rendered via Buffer_write_strings (0x5DB040), not Buffer_write_l. Strings_put hook attempt failed (reentrancy crash, no text captured). Need alternative approach.
2. **Unit movement needs more testing**: set_move_to() + mod_veh_skip() approach is new — user said "glaube es funktioniert, aber noch nicht richtig". May need adjustments to how/when movement is executed. Previous attempts (PostMessage VK_NUMPAD, WinProc passthrough) failed: game interpreted Shift+Arrow as view scrolling, not unit movement.
3. **HUD character-by-character drawing**: Prevents Timer trigger from stabilizing on world map. Worldmap Important trigger (whitelist) handles this case.
4. **Hotseat vs Single Player**: Quick Start creates hotseat games. User must use START GAME for single-player.
5. ~~**No localization system**~~: RESOLVED — loc() system implemented with sr_lang/ files.
6. **German text in multiplayer menu**: Reported but could not reproduce from game files (all English). Needs investigation during testing.

## Resolved Issues

1. **Game crashes** (RESOLVED 2026-02-14): Caused by wrong compiler. GCC 15.2.0 (ucrt) generates incompatible code. Fixed by switching to GCC 14.2.0 (msvcrt) as specified by Thinker author.
2. **Missing modmenu.txt** (RESOLVED 2026-02-14): File from Thinker distribution was missing in game directory. Original Thinker code shows MessageBox (invisible to screen reader), blocking startup.
3. **Garbage Mission Year value**: HUD filter now allows long Mission Year messages through, filters only short ones.
4. **Crash on rapid Enter** (RESOLVED 2026-02-21): sr_walk_text_tree() crashed when game's red-black text tree was modified during traversal. Fixed with sr_node_ok() + IsBadReadPtr validation.

## Architecture

### Announce System (gui.cpp)
Four triggers in ModWinProc:
1. **Snapshot**: Fires on draw cycle boundary (>100ms gap). Catches fast transitions and tutorials. Suppressed during arrow nav. Capped at 10 items. HUD filter applied.
2. **Arrow Nav**: 50ms after arrow keypress (independent of HUD timing). Uses item[1] for navigation, item[0] at boundaries (HUD detection).
3. **Timer**: 300ms stable. Suppressed on world map (can't stabilize due to HUD). Max 5 new items. HUD filter applied.
4. **World Map Important**: 500ms poll. WHITELIST approach — only announces:
   - "ABOUT ..." (tutorial popups)
   - "...need new orders..." (turn status)
   - "...Press ENTER..." (action prompts)
   - Everything else silently ignored. Prevents char-by-char noise.

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

- src/gui.cpp — ModWinProc, announce system, screen transitions, MAP-MOVE, turn detection
- src/base_handler.cpp — BaseScreenHandler (7 sections incl. Units, enhanced data, Ctrl+Up/Down nav, Ctrl+I repeat, Ctrl+F1 help)
- src/base_handler.h — BaseScreenHandler declarations
- src/screen_reader.cpp — Tolk loading, 13 inline hooks, text capture buffer
- src/screen_reader.h — Screen Reader API
- src/social_handler.cpp — SocialEngHandler (SE screen accessibility)
- src/social_handler.h — SocialEngHandler declarations
- src/localization.h — SrStr enum, loc(), loc_init()
- src/localization.cpp — English defaults, UTF-8 file loader, key mapping
- sr_lang/en.txt — English SR strings (127 strings)
- sr_lang/de.txt — German SR strings (127 strings)
- docs/keybindings.md — Complete SMAC keybindings reference
- docs/game-api.md — Game API documentation

## Notes for Next Session

### Pending Tests

0. **Scanner mode** — TESTED OK (2026-02-19).
1. **Enhanced tile announcements** — TESTED OK (2026-02-19).
1b. **Social Engineering handler (E key)** — TESTED OK (2026-02-22). Modal loop approach, Enter confirms, Escape cancels.
2. **Test production queue management (Ctrl+Q)** — Open base, Ctrl+Q. Verify:
   - Queue summary announced (item count + instructions)
   - Up/Down cycles items with "N of Total: Name, cost, turns"
   - Item 0 shows mineral progress + "Current production"
   - Insert → picker opens → select → item added after current position → back to queue
   - Shift+Down on item 1 → moves to position 2
   - Delete on item 1 → removed, remaining count announced
   - Delete/Shift on item 0 → "Cannot" messages
   - D → item details (facility/unit stats)
   - Escape → "Queue closed"
   - Close/reopen base → queue changes persisted
3. **Test production picker detail (D key)** — In Ctrl+Shift+P picker, press D. Verify facility/unit/project details.
4. **Test menu bar navigation** — Alt opens, Alt+Left/Right cycles, Alt+Down opens dropdown, Escape closes.
5. **ABOUT RESEARCH body text** — Need alternative approach to capture tutorial popup content.
6. If step movement has issues, fallback: remove action() call, let game loop process ORDER_MOVE_TO naturally.

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
