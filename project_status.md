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
- **Research selection modal (Shift+R)** — dual-mode: Blind Research ON shows directions (Explore/Discover/Build/Conquer), OFF shows specific tech names per category. Announces blind research status + current direction on open. Intro text from Script.txt #TECHRANDOM shown in blind mode. All opening messages queued (non-interrupting). ANSI→UTF-8 conversion correct.
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
- **Unit list** (U) — modal list of all own units with full info, select to activate — TESTED OK
- **Stack cycling** (Tab) — cycle through own units at cursor position — TESTED OK

## Current Hotkeys

- Ctrl+R: Read captured screen text
- Ctrl+Shift+R: Stop speech (silence)
- Ctrl+F12: Toggle debug logging (%TEMP%\thinker_sr.log)
- **World Map — Exploration (no modifier):**
  - Arrow keys: move cursor N/S/W/E
  - Home/End/PgUp/PgDown: move cursor NW/SW/NE/SE
  - Announces: "(x, y) Units. Base. Terrain, Nature, Improvements. Yields. Ownership. Work status"
  - Number keys 1-8: detail query (one category per key), 0: full announcement
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
- **U: Unit list** (modal, all own units, sorted idle-first, Enter activates)
- **Tab: Stack cycling** (own units at cursor, wake + select for movement)
- **Ctrl+M: Message log browser** (world map only, browse/jump to message locations)
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
- src/message_handler.cpp — MessageHandler (message log, Ctrl+M browser, ring buffer)
- src/message_handler.h — MessageHandler namespace declarations
- src/multiplayer_handler.cpp — MultiplayerHandler (NetWin lobby, 4-zone keyboard nav)
- src/multiplayer_handler.h — MultiplayerHandler namespace declarations
- src/labs_handler.cpp — LabsHandler (F2 research/labs status)
- src/projects_handler.cpp — ProjectsHandler (F5 secret projects status)
- src/orbital_handler.cpp — OrbitalHandler (F6 orbital satellite status)
- src/military_handler.cpp — MilitaryHandler (F7 military status + unit detail)
- src/score_handler.cpp — ScoreHandler (F8 faction score/rankings)
- src/localization.h — SrStr enum, loc(), loc_init()
- src/localization.cpp — English defaults, UTF-8 file loader, key mapping
- sr_lang/en.txt — English SR strings (~220 strings)
- sr_lang/de.txt — German SR strings (~220 strings)
- docs/keybindings.md — Complete SMAC keybindings reference
- docs/game-api.md — Game API documentation

## Notes for Next Session

### NEW: Tile Detail Query Keys (1-8, 0) — needs testing (2026-03-02)
- **Refactored sr_announce_tile** into 10 category helper functions for reuse
- **Reordered full tile announcement**: Coordinates → Units → Base → Terrain → Nature → Improvements → Resource bonus → Landmarks → Yields → Ownership → Work status (previously terrain was first, units/base last)
- **Number keys 1-8 on world map** (no modifiers): each announces one category of tile info
  - 1: Coordinates + ownership/diplomacy
  - 2: Units on tile (or "No units")
  - 3: Base on tile (or "No base")
  - 4: Terrain + nature (type, moisture, altitude, fungus, forest, river)
  - 5: Improvements (or "No improvements")
  - 6: Yields + resource bonus
  - 7: Landmarks (or "No landmarks")
  - 8: Work status (or "Not in base radius")
  - 9: reserved
  - 0: Full announcement (new order)
- Works with both exploration cursor and active unit position
- 5 new loc strings (en+de): detail_no_units, detail_no_base, detail_no_improvements, detail_no_landmarks, detail_no_work
- **Test plan**: Load game → navigate world map → press 1-8 on various tiles (base tile, empty ocean, improved land, tile with units) → verify each key announces only its category. Press 0 → full announcement in new order. Check that units/base come before terrain in the full announcement.

### Game Settings Editor (Ctrl+F10) — TESTED OK (2026-03-01)
- **Ctrl+F10 in main menu** opens accessible singleplayer settings editor
- **4 categories** navigated with Tab/Shift+Tab:
  - General (difficulty: 6 levels)
  - Faction (14 factions, D=BLURB text, I=DATALINKS1 info)
  - Map (size/ocean/land/erosion/clouds/natives)
  - Rules (15 toggleable victory conditions + game rules, D=description)
- **Controls**: Up/Down navigate, Left/Right change value, Enter/Space toggle (rules), S summary
- **Enter saves** to Alpha Centauri.ini via prefs_save(), sets customize=2 for custom map
- **Escape cancels** (no changes written)
- New files: game_settings_handler.h/cpp
- ~60 new loc strings (en+de) including rule descriptions
- gui.cpp: 4 changes (include, IsActive check, Ctrl+F10 hotkey, popup-input exclusion)
- **Fix**: GameSettingsHandler added to popup-input exclusion list (prevented D/I key echo)

### NEW: Multiplayer Lobby Settings Editor (Ctrl+Shift+F10) — PARTIALLY WORKING (2026-03-02)
- Modal editor for all 8 dropdown settings + 18 checkboxes in NetWin lobby
- Architecture: GameSettingsHandler pattern (RunModal + sr_run_modal_pump)
- 2 categories: Settings (8 items), Rules (18 checkboxes)
- Tab/Shift+Tab categories, Up/Down items, Left/Right values, Space toggle, S summary
- **IAT hook on GetAsyncKeyState** (0x669330): fakes VK_LBUTTON pressed during popup apply phase
- Difficulty: direct memory write (NetWin+0xE68, known offset)
- Other settings: IAT hook + simulate_click opens popup, mouse move to target item, release selects
- Checkboxes: simulate_click toggles (already working)
- Values loaded: difficulty from memory, others from rendered text via lookup tables
- New files: netsetup_settings_handler.h/.cpp
- Changed: patch.h (GetAsyncKeyStateImport), patch.cpp (IAT hook), gui.cpp (include, modal_active, routing, hotkey), localization.h/.cpp (10 new strings), en.txt/de.txt
- **Fixed (2026-03-02):** Ctrl+Shift+F10 hotkey was unreachable — MultiplayerHandler's else-if block on line 1369 swallowed the key before the Ctrl+Shift+F10 check on line 1714. Fix: moved Ctrl+Shift+F10 handling into MultiplayerHandler::Update() directly.
- **Fixed (2026-03-02):** Lobby Left/Right on settings with unknown offsets now opens NetSetupSettingsHandler (via RunModalAt) instead of showing "not supported". Added RunModalAt(category, item) entry point.
- **OPEN:** Settings editor opens and reads values correctly from rendered text, but applying changes (save via Enter) does not work yet — popup mechanism (IAT hook + simulate_click) needs further debugging. Deferred to later session.
- **Test plan**: Open MP lobby → Ctrl+Shift+F10 → verify navigation + announcements → change difficulty → change other setting → toggle checkbox → Enter to save → check lobby display updated

### NEW: F-Key Status Screen Handlers (F2, F5-F8) — needs testing (2026-02-28)
- **F2 (Research/Labs)**: Single-screen summary. S: research summary (tech, progress, turns, allocation). D: labs breakdown per base. Escape: close.
- **F5 (Secret Projects)**: Navigable list of all secret projects. Up/Down: browse. Shows built (by faction at base), not built, or destroyed. S: summary counts.
- **F6 (Orbital Status)**: Single-screen summary of satellites (nutrient/mineral/energy/defense). D: compare all factions.
- **F7 (Military Status)**: Summary of combat units, strength, ranking, best weapon/armor/speed, planet busters. S: power rankings across factions. D: navigable unit type list (Up/Down in sub-mode, Escape returns).
- **F8 (Score/Rankings)**: Navigable faction ranking list. Up/Down: browse factions by rank. S: your rank. Shows bases, population, techs per faction.
- **F9 (Monuments)**: Modal handler UI fertig (Navigation, 16 Typen, de+en). ABER: Alle Werte = 0. Hook-Ansatz gescheitert (feuert nie). Nächster Schritt: Monument-Daten direkt aus Speicher lesen — Datenstruktur (317 ints/Fraktion) muss reverse-engineered werden. Details in memory/monuments.md.
- **Deferred**: F10 (Hall of Fame — external save data).
- New files: labs_handler.h/cpp, projects_handler.h/cpp, orbital_handler.h/cpp, military_handler.h/cpp, score_handler.h/cpp
- ~40 new loc strings (en+de)
- gui.cpp: 5 new F-key intercepts + IsActive/Update routing
- **Test**: On world map, press each F-key (F2, F5, F6, F7, F8). Check: opens modal, announces data, navigation works, Escape closes, Ctrl+F1 help.

### NEW: Time Controls (Shift+T) — TESTED OK
- Shift+T on world map opens accessible time controls picker
- 6 presets from alphax.txt #TIMECONTROLS (None, Tight/Kurz, Standard, Moderate/Mäßig, Loose/Locker, Custom/Angepasst)
- Up/Down navigates, Enter confirms (sets AlphaIniPrefs->time_controls + set_time_controls()), Escape cancels
- Announces preset name + seconds per turn/base/unit
- 4 new loc strings (en+de)

### NEW: Thinker Menu (Alt+T) — accessible, needs testing (2026-03-02)
- **Alt+T** (or Alt+H in reduced mode) now opens accessible modal handler when screen reader is active
- Non-SR mode unchanged (still uses game popups)
- **3 modes:**
  - Main menu: navigate with Up/Down, Enter to select, Escape to close
  - Statistics: announces world + faction stats (bases, units, pop, minerals, energy), any key returns
  - Mod Options: 12 checkboxes, Space toggles, S for summary, Enter saves to INI, Escape cancels
- Opens with version + build date + play time announcement
- Game Rules option available in main menu (pre-game only)
- New files: thinker_menu_handler.h/.cpp
- ~30 new loc strings (en+de)
- gui.cpp: include, IsActive/Update routing, Alt+T/Alt+H conditional dispatch
- **Test plan**: In-game, press Alt+T. Check: version announced, menu navigates, Statistics reads data, Mod Options toggles and saves, Escape closes cleanly.

### NEW: Chat (Ctrl+C) — deferred testing
- Ctrl+C on world map announces "Chat" when MultiplayerActive
- Key passes through to game's native chat handler (not consumed)
- Only relevant for network multiplayer (not hotseat) — testing deferred until network MP session

### NEW: Group Go-to-Base (J key) — TESTED OK (2026-02-27)
- J on world map opens accessible group go-to-base picker (sends ALL units on tile to selected base)
- Same modal pattern as G (single unit go-to-base)
- 2 new loc strings (en+de): group_go_to_base, group_going_to_base

### NEW: Airdrop (I key) — needs testing (late game feature)
- I on world map opens accessible airdrop modal (requires Drop Pod ability)
- **Voraussetzung**: Einheit muss Drop Pod-Fähigkeit haben, noch nicht gedroppt diese Runde, auf Luftstützpunkt stehen
- **Zwei Modi im Dialog:**
  - **Basisliste** (Standard): Hoch/Runter blättert durch alle gültigen Zielbasen (eigene + feindliche) in Reichweite, sortiert nach Entfernung
  - **Cursor-Position** (C-Taste): Wählt die aktuelle Explorations-Cursor-Position als Abwurfziel — beliebiges Landfeld, gleichwertig zu sehenden Spielern
- **Moduswechsel**: C wählt Cursor-Modus, Hoch/Runter wechselt zurück zu Basenliste
- **Bedienung**: Eingabe bestätigt (je nach Modus), Escape bricht ab
- **Workflow Cursor-Drop**: Vor I-Drücken mit Explorations-Cursor (Pfeiltasten) gewünschte Position ansteuern → I drücken → C drücken (zeigt Koordinaten + Entfernung) → Eingabe bestätigt
- **Validierung**: can_airdrop() prüft Einheit (Drop Pod, nicht gedroppt, keine Bewegung verbraucht, auf Airbase), allow_airdrop() prüft Ziel (kein Ozean, keine Aerospace Complex-Blockade, ZOC-Check für Nicht-Kampfeinheiten, keine feindlichen Einheiten auf Feld)
- **Reichweite**: 8 Felder Standard, unbegrenzt mit Orbital Insertion-Technologie
- 8 new loc strings (en+de)
- **Kann erst spät im Spiel getestet werden** — braucht Drop Pod-Technologie + Einheit mit der Fähigkeit

### NEW: Patrol (P key) — TESTED OK (2026-02-27)
- P nutzt jetzt Explorations-Cursor-Position als Patrol-Ziel (nicht mehr den Game-Cursor)
- Bestätigungsschritt: "Patrouille zu (x, y)? Eingabe zum Bestätigen, Escape zum Abbrechen."
- **Hintergrund**: Game-Pfeiltasten wechseln Einheiten, nicht den Cursor — alter Targeting-Modus war für blinde Spieler nicht nutzbar

### NEW: Ctrl+R (Road to) / Ctrl+T (Tube to) — needs testing
- Gleicher Ansatz wie Patrol: Cursor vorher positionieren, Taste drücken, Enter bestätigen
- Prüft ob ein Former ausgewählt ist (sonst "Bau nicht möglich")
- Nutzt set_road_to() / ORDER_MAGTUBE_TO direkt statt Game-Targeting
- 6 new loc strings (en+de)
- **Kann getestet werden sobald Former Straßen/Tubes bauen können**

### Probe Team actions — needs testing (late game feature)
- Subvert Enemy Unit, Mind Control City, and other probe actions use standard popup menus
- Should already work via existing sr_popup_list_parse() and NetMsg_pop capture
- **Voraussetzung**: Probe Team bauen (braucht entsprechende Technologie), neben feindliche Einheit/Basis bewegen
- **Zu testen**: Popup-Menü lesbar? Ergebnis-Meldung vorgelesen? Kostenanzeige (Energy Credits)?
- **Kann erst getestet werden wenn Kontakt mit anderen Fraktionen + Probe Team verfügbar**

### Message Handler (Ctrl+M) — needs testing
- **Ctrl+M on world map**: Opens modal message log browser with all captured messages
- **Auto-announce**: All NetMsg_pop calls now captured and announced via SR (queued, non-interrupting)
- **Browser controls**: Up/Down browse, Enter jump to location (sets SR cursor + centers map), S summary, Home/End first/last, Ctrl+F1 help, Escape close
- **Ring buffer**: 50 messages with text, coordinates, turn number
- **Coverage**: veh.cpp (monoliths, goody huts), faction.cpp (spy, atrocity, vendetta), base.cpp (escaped), tech.cpp (picking tech), patch.cpp (forest/kelp grows, production), veh_combat.cpp (surprise, vendetta/pact reports), gui.cpp (diplomacy via mod_NetMsg_pop, unit moved, out of range)
- **New files**: message_handler.h, message_handler.cpp
- **9 new loc strings** (en+de): msg_open, msg_item, msg_item_loc, msg_empty, msg_closed, msg_no_location, msg_summary, msg_help, msg_notification
- **WorldMapHandler::SetCursor()**: New function to set SR cursor, center map, and announce tile — used by message browser Enter

### Base Screen Gaps Closed — TESTED OK (2026-02-27)
- **Resources section extended**: Shows nutrient consumption, mineral support cost, energy inefficiency. Starvation warning when nutrient_surplus < 0.
- **Economy section extended**: Commerce income from trade (BaseCommerceImport sum).
- **Status section extended**: 7 new warnings — Starvation, Low minerals, Inefficiency, No headquarters, High support cost, Undefended, Police units count.
- **OnOpen extended**: Starvation and Undefended warnings in opening announcement. Help text uses GetHelpText() (always in sync with Ctrl+F1).
- **Supported Units (Ctrl+Shift+S)**: Scrollable list of all units home-based at current base. Up/Down, D details, Enter activate, Escape close.
- **Psych Detail (Ctrl+Shift+Y)**: One-shot announcement — police units, psych output, talents vs drones, riot status.
- ~20 new loc strings (en+de)

### Unit List (U key) + Stack Cycling (Tab) — TESTED OK (2026-03-01)
- **U on world map**: Opens modal list of ALL own units, sorted idle-first then by order type then position
- Each entry: name, position, moves (remaining/total), order (idle/hold/sentry/explore/go to/convoy/terraform name), health (if damaged), home base
- Up/Down/Home/End navigate, Enter selects (wakes unit, sets CurrentVehID + iUnit, focuses camera), Escape cancels, Ctrl+F1 help
- **Tab on world map**: Cycles through own units at exploration cursor position
- Uses veh_at() + next_veh_id_stack linked list, resets when cursor moves
- Wakes unit, sets CurrentVehID + iUnit for movement control, focuses camera

### Foreign Unit List (Shift+U) — TESTED OK (2026-03-02)
- **Shift+U on world map**: Opens modal list of ALL non-own units on visible (explored) tiles
- Each entry: name, faction name + diplomacy status (Pact/Treaty/Truce/Vendetta/None/Native), position, morale, attack/defense values (or "Psi" for native units), triad (Land/Sea/Air), health (if damaged)
- Sorted by map position
- Up/Down/Home/End navigate, Enter jumps to unit on map (sets SR cursor + centers camera), Escape closes, Ctrl+F1 help
- Uses is_visible(faction) to only show units on explored tiles
- Game strings (faction names) converted via sr_game_str() for correct UTF-8 umlauts
- 9 new loc strings (en+de): enemy_list_open/fmt/native/empty/help/jump/triad_land/triad_sea/triad_air
- Announces: "Name, X of Y on tile" + moves
- 16 new loc strings (en+de): unit_list_open/fmt/moves/health/home/selected/empty/help, order_idle/hold/sentry/explore/goto/convoy, tab_cycle_fmt/none
- **Key fix**: Initial version didn't set `*CurrentVehID` and `MapWin->iUnit` — unit was focused but not selected for movement. Fixed by copying garrison_activate() pattern.

### NEW: Artillery Modal (F key) — needs testing (late game feature)
- F on world map opens accessible artillery target list (requires artillery-capable unit)
- **Voraussetzung**: Einheit muss Artillerie-Fähigkeit haben (can_arty), Einheit muss bereit sein (veh_ready)
- **Zielliste**: Alle feindlichen Einheiten (top-of-stack) + feindlichen Basen in Artillerie-Reichweite, sortiert nach Entfernung
- **Zwei Modi im Dialog:**
  - **Zielliste** (Standard): Hoch/Runter blättert durch Ziele mit "X von Y: Name (Fraktion), Z Felder entfernt"
  - **Cursor-Position** (C-Taste): Zeigt Entfernung + ob in/außer Reichweite
- **Bedienung**: Eingabe feuert (mod_action_arty), Escape bricht ab, F1 Hilfe
- **Unterschied zu P (Patrol)**: P nutzt weiterhin den normalen Targeting-Modus des Spiels (Spielcursor bewegen, Enter bestätigt) — das ist beabsichtigt
- 8 new loc strings (en+de)
- **Kann erst getestet werden wenn Artillerie-Einheiten verfügbar**

### NEW: Unit Action Menu (Shift+F10) — TESTED OK (2026-02-27)
- Shift+F10 on world map opens navigable action menu for current unit
- Context-sensitive: shows only actions relevant to unit type and terrain
- Former: terraform actions (Road/Farm/Mine/Solar/Forest/Sensor/Condenser/Borehole/Airbase/Bunker/Remove Fungus/Automate)
- Colony Pod: Build Base
- Combat: Long Range Fire
- Supply: Convoy
- Transport: Unload
- Land units: Airdrop
- On base tile: Open Base
- Always: Skip/Hold/Explore/Go to Base/Patrol
- Feedback: Hold → "Hält Position", Explore → "Erkundet automatisch", Unload → "Entlädt", Skip → "Übersprungen. Nächste Einheit: X", Terraform → Terraform-Polling
- mod_veh_skip nach WinProc-Aktionen (H, X, U, Terraform) — Einheit wird korrekt abgegeben
- 32 loc strings (en+de), auch in Ctrl+F1 Hilfe referenziert

### Convoy (O key on Supply Crawler) — needs testing (late game feature)
- **Voraussetzung**: Supply Crawler bauen (braucht Industrial Automation-Technologie)
- **Mechanik**: Supply Crawler auf Feld bewegen → O drücken → Ressource wählen (Nahrung/Mineralien/Energie) → Crawler bleibt auf Feld und liefert Ertrag dauerhaft an Basis
- **Im Aktionsmenü**: Erscheint nur für Supply Crawler (`is_supply()`), nicht für Former (die bekommen "Sensor bauen, O")
- **Zu testen**: O öffnet Ressourcen-Popup (vom Spiel), Popup lesbar über bestehendes Popup-Capture-System?
- **Kann erst getestet werden wenn Supply Crawler verfügbar (Industrial Automation)**

### NEW: Runtime Toggles (Ctrl+Shift+T / Ctrl+Shift+A) — needs testing (2026-03-01)
- **Ctrl+Shift+A**: Toggle all accessibility on/off. When off, game behaves like vanilla Thinker (no speech, no handler interception, no virtual cursor). Only Ctrl+Shift+A is checked when disabled. Announces before toggling.
- **Ctrl+Shift+T**: Toggle Thinker AI on/off (world map only). Sets `conf.factions_enabled` to 0 (original AI) or restores original value. Independent of accessibility toggle.
- Both hotkeys listed in Ctrl+F1 help.
- 6 new loc strings (en+de), getter/setter in screen_reader.h/cpp, early-exit in ModWinProc.
- **Test**: On world map, Ctrl+Shift+A → "Screen reader off" → arrows scroll normally → Ctrl+Shift+A → "Screen reader on". Ctrl+Shift+T → "Thinker AI disabled" → Ctrl+Shift+T → "Thinker AI enabled".

### Pending Tests

0v. **Combat Accessibility** — Added 2026-03-02. Three features: (1) Automatic combat result announcements in mod_battle_fight_2 — melee results (victory/defeat/defense/draw/retreat/capture), artillery bombardment results (units hit + damage per unit), interceptor announcements, promotion announcements, nerve gas warning. (2) Combat odds in Shift+U enemy list — when player has a selected combat unit, odds are appended to each enemy entry (Favorable/Unfavorable/Even). (3) C key in Shift+U for detailed combat preview (weapon, morale, HP, odds). Files: veh_combat.cpp (SR code in mod_battle_fight_2), world_map_handler.cpp (Shift+U enhancement), localization.h/cpp, en.txt, de.txt. 23 new loc strings. Test: Attack enemy → hear result. Use artillery → hear bombardment. Open Shift+U with combat unit selected → hear odds. Press C → hear detailed preview.
0u. **Faction Selection Keyboard Access** — WIP (2026-03-01). Handler erkennt Fraktionsauswahl per BLURB-Detection. Fraktionsname wird vor BLURB-Text vorgelesen. Tasten: Enter (spielen), G (Gruppe bearbeiten), I (Info), R (Zufallsgruppe), Ctrl+F1 (Hilfe). Dateien: faction_select_handler.h/cpp. Änderungen V3 (2026-03-01): (1) I-Taste liest DATALINKS1+DATALINKS2 direkt aus Fraktionsdatei per sr_read_popup_text() statt INFO-Button-Klick (vermeidet unerreichbares DATALINKS-Popup). (2) ScanButtons() wird bei JEDEM OnBlurbDetected() aufgerufen statt nur beim ersten Mal (_buttonsScanned Guard entfernt) — fix für Buttons die nach Screen-Wechsel nicht mehr funktionieren. (3) BTN_INFO aus enum/MatchButton entfernt. (4) Hilfetext um G/I/R erweitert. Neuer Loc-String SR_FACTION_NO_INFO. **PROBLEM: I-Taste funktioniert gar nicht (kein Effekt).** Nächste Sitzung debuggen — prüfen ob HandleKey überhaupt aufgerufen wird, ob _lastBlurbFile gesetzt ist, ob sr_read_popup_text die Datei findet. Ctrl+F12 Debug-Log prüfen.
0t. **Research Selection Modal (Shift+R)** — TESTED OK (2026-03-01). Dual-mode: Blind Research ON shows 4 directions, OFF shows specific techs. Announces blind status + current direction on open. Intro text from #TECHRANDOM. All opening messages queued. ANSI→UTF-8 fixed (no double conversion). Enter key no longer leaks to game.
0s. **Council Handler (Planetary Council Accessibility)** — Added 2026-02-28. Inline hook on call_council (0x52C880) detects council open/close. OnOpen announces faction name + governor + vote summary. S/Tab repeats vote summary, Ctrl+F1 help. Ctrl+V on world map: can council be called? + governor info + vote summary. Observer pattern (game handles navigation). New files: council_handler.h/cpp. 10 new loc strings (en+de). Hook count 22→23. Test: Ctrl+V on world map, council einberufen, S/Tab/Ctrl+F1 während Council, KI-Council löst keine Ansage aus.
0r. **Enhanced Tile Announcements v2** — Added 2026-02-28. Five enhancements to sr_announce_tile(): (1) Foreign units show faction prefix ("Spartan Scout Patrol"), (2) Foreign territory shows diplo status ("Territory: Spartans (Treaty of Friendship)"), (3) Formers show terraform order ("Former, building Road"), (4) Worked tiles in base radius show ", worked", (5) Used monoliths show "(used)". Test all 5 on world map.
0q. **Game-End Accessibility (Siegtyp, Replay, Score)** — Added 2026-02-28. Drei Features: (1) Siegtyp-Ansage bei STATE_GAME_DONE (Conquest/Diplomatic/Economic/Transcendence/Defeat/Game Over), (2) Replay-Karte Ansage ("Visuelle Animation, Escape zum Fortfahren"), (3) Score-Screen Text kommt durch bestehendes Text-Capture. Testen: Spiel beenden (Cheat/Szenario), prüfen ob Siegtyp angesagt wird, ob Replay-Karte angesagt wird, ob Score-Texte lesbar sind.
0p. **Design Workshop: Rename/Obsolete/Upgrade (R/O/U)** — Added 2026-02-28. R: rename unit inline. O: toggle obsolete. U: upgrade with double-press confirm. Rename+Obsolete testable sofort. Upgrade braucht Late-Game mit Custom-Designs.
0o. **Base Screen Gaps** (Resources V3, Economy V3, Status warnings, Ctrl+Shift+S Support, Ctrl+Shift+Y Psych) — TESTED OK (2026-02-27).
0n. **Message Handler (Ctrl+M)** — TESTED OK (2026-02-27). Basic flow works (open, browse, close). Messages only in current session (ring buffer, no savegame persistence — by design). Open question: behavior with multiple message categories (combat, diplomacy, production mixed) not yet tested — needs a busier game state.
0z. **Specialist management (Ctrl+W)** — TESTED OK (2026-02-26). All announcements working correctly.
0k. **Design Workshop (Shift+D)** — TESTED OK (2026-02-26). Two-level navigation works. Kostenanzeige zeigt Grundkosten (ohne Prototyp-Zuschlag) — ist korrekt, Aufschlag kommt erst beim Bauen.
0a. **Preferences handler (Ctrl+P)** — TESTED OK (2026-02-26).
0b. **Targeting mode (J, P, F, Ctrl+T)** — TESTED OK (2026-02-26).
0c. **Go to base (G)** — TESTED OK (2026-02-26).
0d. **Ctrl+R conflict** — RESOLVED (2026-02-27). Key mapping decided by user.
0e. **Facility demolition (Ctrl+D)** — TESTED OK (2026-02-23).
0f. **Interlude story text** — TESTED OK (2026-02-23). Full narrative text announced with title.
0g. **Diplomacy: F12 Commlink dialog** — TESTED OK (2026-02-25). All diplomacy features working.
0h. **Interlude umlaut fix** — TESTED OK (2026-02-26). Umlauts now display correctly.
0i. **Variable substitution fix** — TESTED OK (2026-02-26). All variable patterns resolved.
0j. **Number input dialog (pop_ask_number)** — TESTED OK (2026-02-26).
0l. **Former terraform announcements** — TESTED OK (2026-02-26).
0m. **Governor priorities (Ctrl+G)** — TESTED OK (2026-02-26).
0. **Scanner mode** — TESTED OK (2026-02-19).
1. **Enhanced tile announcements** — TESTED OK (2026-02-19).
1b. **Social Engineering handler (E key)** — TESTED OK (2026-02-22).
1c. **Tutorial popup fix** — TESTED OK (2026-02-22).
1d. **Base naming prompt** — TESTED OK (2026-02-22).
1e. **Planetfall variables** — TESTED OK (2026-02-22).
1f. **Menu name announcements** — TESTED OK (2026-02-22).
2. **Production queue management (Ctrl+Q)** — TESTED OK (2026-02-22).
3. **Production picker detail (D key)** — TESTED OK (2026-02-22).
4. **Menu bar navigation** — TESTED OK (2026-02-22).
5. **Tour button accessibility** — TESTED OK (2026-02-22).
6. **Setup option accessibility** — RESOLVED (2026-02-27). Already works automatically via existing popup/text capture system.
7. If step movement has issues, fallback: remove action() call, let game loop process ORDER_MOVE_TO naturally.
8. **Load Game dialog** — TESTED OK (2026-02-27).
8d. **Save Game dialog** — TESTED OK (2026-02-27). Overwrite confirmation announces question + "Eingabe zum Bestätigen, Escape zum Abbrechen" (buttons not arrow-navigable). Background noise filtered via positive identification (only .SAV, known dirs, known buttons announced).
8e. **File browser refinement** — TODO (later). (1) File selection reliability during save/load — user reports occasional uncertainty. (2) Investigate whether custom filename input is possible (typing a new save name). Low priority, basic save/load works.
8b. **Base screen mini-map** — NOT NEEDED (2026-02-27). Map overlay is read-only/visual only. No click-to-assign-workers — worker management already handled by Ctrl+W (SpecialistHandler). No accessibility gap.
8c. **Zoom level feedback (+/- keys)** — NOT NEEDED (2026-02-27). Zoom is purely visual (tile render size). SR exploration cursor and scanner work at coordinate level, unaffected by zoom.
9. **Multiplayer Setup (NetWin)** — IMPLEMENTED, needs testing (2026-02-28).
   - Full keyboard navigation: Tab/Shift+Tab cycles 4 zones (Settings, Players, Checkboxes, Buttons)
   - Up/Down navigates within zones, Enter/Space activates (opens popup/toggles/clicks)
   - Settings: click simulates mouse click, opens game popup (handled by existing sr_popup_list)
   - Checkboxes: 18 items (victory conditions + game options), state tracked locally, announced on toggle
   - Players: 7 slots, change detection via snapshot text comparison, join/leave announced
   - S: summary, Ctrl+F1: help, Escape: cancel (pass-through to game)
   - Non-modal (game network events keep processing)
   - ~42 new loc strings (en+de), new files: multiplayer_handler.h/.cpp
   - **v2 fixes (same session):** First test showed zone navigation + checkbox announce works. Three issues fixed:
     1. **Escape** now passes through to game (was: simulated click on Cancel, didn't work)
     2. **SendInput** replaces PostMessage for mouse clicks (canvas→screen coord conversion via ClientToScreen). PostMessage clicks weren't reaching game UI elements.
     3. **Popup wait mode**: After clicking a setting, handler yields all keys for 3s so popup can be navigated. Clears when sr_popup_list activates or timeout.
     4. **WM_CHAR tracking**: Only consumes WM_CHAR for keys handler actually handled; pass-through keys (Escape, popup wait) also pass WM_CHAR through.
   - **v3 fixes (2026-02-28, sessions 3-4)**: Player list, Start Game, Spielart, dedup fixes.
     1. **Player list FIXED**: Buffer pointer changed to main canvas (NetWin+0x444). Timer uses `sr_force_snapshot()` + `GraphicWin_redraw()` + exact y-matching for 7 player slots. All 7 slots now show correct player data.
     2. **Dedup bypass**: Added `sr_mp_no_dedup` flag to skip both strstr dedup AND prefix dedup in `sr_record_text()` — needed because multiple "Computer" entries share identical text. SR_MAX_ITEMS increased from 32 to 96.
     3. **Spielart dialog WORKS**: Modal with 3 options (Random/Scenario/Load Map) writes to `DefaultPrefs->map_type`.
     4. **Start Game v3 (didn't work)**: `GraphicWin_close(NetWin)` with `PbemActive=0` → showed "Spiel leiten/Beitreten" popup, but then returned to main menu. Root cause: `create_game` (0x4E2E50) returned 0 (failure) because closing the window makes the inner event dispatch return -1.
     5. **Key reverse-engineering**: `PbemActive` (0x93A95C) is the lobby dialog result global. Cancel button (ID 9999 in pick_service loop) sets it to 1. Start leaves it at 0. AlphaNet_setup checks it after lobby closes: 0→show Host/Join popup, !=0→cancel path.
   - **v4 fix (2026-02-28, session 5)**: Two-part approach to fix Start Game:
     1. **prepare_game call**: Before closing lobby, call `prepare_game` (0x482D90, __thiscall void on NetWin) to set up factions, random assignments, and map — same as the game's original Start button does internally.
     2. **create_game hook**: `mod_create_game` in gui.cpp hooks the call at 0x4E3279 (AlphaNet_setup→create_game). When `sr_net_start_requested` flag is set, overrides return value from 0→1 so AlphaNet_setup treats it as success.
     - New flag: `sr_net_start_requested` (screen_reader.h/.cpp)
     - Hook: `write_call(0x4E3279, mod_create_game)` in patch.cpp
     - Flow: activate_button(1) → prepare_game(NetWin) → set flag → GraphicWin_close → create_game returns 1 → AlphaNet_setup proceeds
   - **Pending test**: Start Game → check thinker_sr log for "create_game hook override 0 -> 1". If game crashes in prepare_game, NetWin state may need setup. If game starts but settings wrong, investigate NetWin internal state vs game globals.

10. **Scenario Editor Accessibility** — DEFERRED (future project). See details below under "Future: Scenario Editor".
   - **v5 investigation (2026-03-01)**: Complete rewrite + deep reverse engineering.
     1. **Code simplified**: Removed run_setting_modal, all SettingOption arrays, prepare_game/NetSetupState logic (~200 lines removed).
     2. **WinProc direct call FAILED**: `WinProc(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x,y))` — clicks logged but game didn't respond. DDrawCompat likely prevents synthetic mouse processing.
     3. **SpotList FOUND at NetWin+0xD34**: Runtime scan after first redraw found 61 hitbox spots (max=85). Full hitbox map:
        - type=0: 8 Settings (pos=0,2-8), rects at y=62-215, x=11-312
        - type=1: 18 Checkboxes, left column pos=100,0-7 (y=415-593), right column pos=8-14,16-17
        - type=2: Player name areas (7 slots × 2 spots each)
        - type=3: Player faction column (7 spots)
        - type=4: Player difficulty column (7 spots)
        - type=5: Player selector buttons (7 spots)
     4. **iHitBoxTagClicked at 0xD2C FAILED**: Writing tag to NetWin+0xD2C (CWinFonted analogy: 8 bytes before SpotList) didn't trigger any action.
     5. **Vtable dump deployed**: Code to dump first 40 vtable entries is deployed but not yet tested. Next: analyze vtable to find on_left_click virtual function.
   - **v6 fixes (2026-03-01, session 6)**: Start button, sync spam, settings investigation.
     1. **Start button FIXED**: Warmup click at (10,10) before actual button click → works reliably on first try. Uses `simulate_click` (direct WinProc).
     2. **Button positions dynamic**: OnOpen reads CHILD windows, gets BaseButton.name (offset 0xA7C) and rRect1 center. Handles localized names ("ABBRECHEN", "SPIEL STARTEN").
     3. **Sync spam FIXED**: Labels starting with "SYNCH" suppressed in mod_BasePop_start (gui.cpp). No more rapid-fire "Spielsynchronisierung" announcements.
     4. **_launching flag**: Stops OnTimer when Start is clicked → no player list churn during game launch.
     5. **SpotList at 0xD34 CONFIRMED**: 61 spots read successfully. Settings (type=0) hitbox rects: x=11-312, y=62-215. Dynamic coordinate update works — settings[].clickX/Y updated from SpotList centers.
     6. **Settings popups STILL BROKEN**: Exhaustive investigation:
        - `simulate_click` (direct WinProc): works for checkboxes (type=1), NOT for settings (type=0)
        - `PostMessage` (async): works for buttons (CHILD), NOT for settings
        - `mouse_event` + WinProc (hw state): NOT for settings
        - `SetCursorPos` + PostMessage: NOT for settings
        - Direct vtable[19] call (0x0047B760): no crash, no effect
        - Direct dialog functions (`time_controls_dialog`, `config_game`): opens popup BUT crashes at 0x600F10 (null Buffer in rendering code) — these functions are for single-player setup, not MP lobby
        - Timer-deferred click (SetTimer 50ms → WinProc): DEPLOYED, not yet tested
     7. **OpenSMACX research**: NetWin vtable at 0x0066ccec. Click dispatch: WinProc → vtbl[19] (0x0047B760) → Spot::check (0x005FAB00) → sub_47E640 (OnClick dispatch) → sub_59E530 (setting popup launcher) → encrypted handlers at 0x0060A6xx.
   - **v6-v8 results (2026-03-01)**: Timer-deferred WinProc click: popup opens but AUTO-CLOSES instantly (press-drag-release pattern, GetAsyncKeyState sees no button held). SendInput: blocked by DDrawCompat. Direct memory write: +0xE68 was false positive (earlier popup auto-select had already changed display). Memory scan of 0xA14-0xF00: no setting field offsets found. All tested offsets had zero effect on display.
   - **Next session approaches (prioritized):**
     1. **Popup-autoselect exploit**: Popup opens and auto-selects first option at mouse position. If we click at the CORRECT y-position for the desired option, it auto-selects that option. Known popup item y-coords: 1, 21, 41, 61, 81, 101. Need to determine popup window position relative to click point.
     2. **AlphaIniPref test**: Write to 0x94B478 (time_controls) and 0x94B480-0x94B498 (custom_world[0..6]) + redraw → check if settings display changes.
     3. **Extended memory dump**: NetWin 0xF00-0x1800, also CHILD windows.
     4. **GetAsyncKeyState IAT hook**: Return VK_LBUTTON=pressed during _inDialog → popup stays open for keyboard navigation.

**What works (tested):**
- Transition announcement ("Mehrspieler-Einrichtung" on open/close)
- DirectPlay debug text filtered (Host ID, Guaranteed count, Sending, etc.)
- Pre-setup dialogs work via existing popup system (NETCONNECT_JOIN_OR_CREATE, NETCONNECT_CREATE, etc.)
- Popups triggered from setup (time controls, ocean coverage, game type, difficulty) work via BASEPOP capture
- Player list: all 7 slots read correctly (name, difficulty, faction)
- Spielart dialog: 3 options (Random/Scenario/Load Map)
- Checkboxes: toggle + state announcement
- Zone navigation: Tab/Shift+Tab cycles Settings/Players/Checkboxes/Buttons
- Cancel: returns to main menu (PbemActive=1)
- Start Game: calls prepare_game + create_game hook override (v4, needs testing)

**NetWin Child Window structure (confirmed via debug dump):**
- CHILD[0]: BaseButton "ABBRECHEN" — rect=(556,212,792,232)
- CHILD[1]: BaseButton "SPIEL STARTEN" — rect=(317,212,553,232)
- CHILD[2]: Status bar area — rect=(317,356,793,384), flags=0x1102120, no button name (renders "Keine Zeiteinst." + "KEINE KONTAKTE")
- CHILD[3]: Player list area — rect=(320,244,790,351), has 1 sub-child (scrollbar at x=450-470)

**Key finding: All game options, settings, and player data are rendered DIRECTLY on the NetWin surface — NOT as child windows!**
- 20 game option checkboxes (Simultane Bewegungen, Siegbedingungen, etc.) → painted with Buffer_write
- 8 game settings ({Schwierigkeitsgrad}, {Zeiteinstellungen}, {Spielart}, etc.) → painted with Buffer_write
- Player list (fjkl, Computer x5, Zufall x7) → painted with Buffer_write
- Interaction is mouse-click based via HitBox/SpotList on the main window

**GameRules, GameState, DiffLevel are ALL ZERO while NetWin is open!**
NetWin has own internal state fields (beyond Win base struct at offset 0x444+). Rules are only applied to game globals when "SPIEL STARTEN" is pressed.

**GameRules flags mapped to UI options** (engine_enums.h):
- RULES_VICTORY_TRANSCENDENCE=0x800, RULES_VICTORY_CONQUEST=0x2, RULES_VICTORY_DIPLOMATIC=0x8
- RULES_VICTORY_ECONOMIC=0x4, RULES_VICTORY_COOPERATIVE=0x1000, RULES_DO_OR_DIE=0x1
- RULES_LOOK_FIRST=0x10, RULES_TECH_STAGNATION=0x20, RULES_SPOILS_OF_WAR=0x4000
- RULES_BLIND_RESEARCH=0x200, RULES_INTENSE_RIVALRY=0x40, RULES_NO_UNITY_SURVEY=0x100
- RULES_NO_UNITY_SCATTERING=0x2000, RULES_BELL_CURVE=0x8000, RULES_TIME_WARP=0x80
- STATE_RAND_FAC_LEADER_PERSONALITIES=0x800000, STATE_RAND_FAC_LEADER_SOCIAL_AGENDA=0x1000000 (in GameState)
- "Simultane Bewegungen" → MRULES_UNK_10=0x10 (in GameMoreRules)

**Relevant game addresses:**
- NetWin: 0x80A6F8 (Win*, rect 800x600)
- GameHalted: 0x68F21C (1 during setup)
- GameRules: 0x9A649C, GameMoreRules: 0x9A681C, GameState: 0x9A64C0
- DiffLevel: 0x9A64C4, MapSizePlanet: 0x94A2A0, MapOceanCoverage: 0x94A2A4
- MapErosiveForces: 0x94A2AC, MapNativeLifeForms: 0x94A2B8, MapCloudCover: 0x94A2B4
- AlphaIniPrefs->time_controls, config_game: 0x589D30, time_controls_dialog: 0x589330
- DefaultPrefs: 0x94B350 (difficulty, faction_id, map_type)

**jackal.txt dialog sections:** NETCONNECT_JOIN_OR_CREATE, NETCONNECT_CREATE, NETCONNECT_JOIN, SELECTSERVICE, NETCONNECT_MAX_PLAYERS, NETCONNECT_PASSWORD_SET, NET_LOBBY_PASSWORD_QUERY, NET_LOBBY_INPUT_PASSWORD, NETCONNECT_SESSIONS, NET_SENDTIMEDOUT, NET_OKTODROPHOST, NET_OKTODROPCLIENT

**Script.txt dialog sections:** #RULES (18 game options with itemlist), #TIMECONTROLS, #DIFFICULTY, #USERULES (Standard/Current/Custom), #HOSTTIME

**Complete coordinate map (confirmed via NET-XY diagnostic, 2026-02-28):**
All coordinates on NetWin canvas (buf=0x80AB3C), 800x600 pixels.
Vtable: 0x0066ccec. Canvas = GraphicWin buffer at NetWin+0x444.

Settings (write_l x=11):
- y=62: Schwierigkeitsgrad, y=79: Zeiteinstellungen, y=113: Spielart (wrap2)
- y=130: Planetgröße, y=147: Ozean, y=164: Erosion, y=181: Natives, y=198: Cloud

Player list (7 slots, 22px spacing, y=58-190):
- Name: write_l2 x=357, Faction: x=537, Difficulty: x=679

Left checkboxes (write_l2 x=32, y=415-575, 20px):
y=415 SimultaneBew, y=435 Transzendenz, y=455 Eroberung, y=475 Diplomatisch,
y=495 Wirtschaftlich, y=515 Kooperativ, y=535 DoOrDie, y=555 LookFirst, y=575 TechStag

Right checkboxes (write_l2 x=426, y=415-575, 20px):
y=415 Kriegsbeute, y=435 BlindResearch, y=455 Rivalität, y=475 NoSurvey,
y=495 NoScattering, y=515 BellCurve, y=535 TimeWarp, y=555 RandPersonalities, y=575 RandAgenda

Status bar (CHILD[2], buf=0x7FDA8C): y=0 time summary, y=14 contact status
Popups: difficulty list (buf=0x1A69B8), time controls (BASEPOP SCRIPT.txt#TIMECONTROLS)
Child vtables: CHILD[0]+[1]=0x669754 (BaseButton), CHILD[2]=0x66a038, CHILD[3]=0x66adc8

Full details documented in docs/game-api.md.

**Next step:**
- Design and build NetSetupHandler (full modal replacement like PrefsHandler)
- Sections: Settings, Player slots, Checkboxes L/R, Buttons
- Simulate clicks at known coordinates for interaction
- Track checkbox state internally (defaults + toggles)

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
- **2026-02-26 (session 4)**: Text input accessibility + variable substitution expansion. (1) Modal number input: Replaced sr_hook_pop_ask_number with own modal loop. Digits 0-9 echoed, Backspace removes+announces, Ctrl+R reads buffer, Enter confirms (ParseNumTable[0]), Escape returns default. (2) Popup text input echo: Game-native text inputs (base naming at founding, save filenames) now echo typed characters. Shadow buffer tracks input for Ctrl+R readback and Backspace feedback. Detection via sr_popup_is_active() || WinModalState. Non-consuming (message passes to game). (3) Base rename umlaut fix: Expanded char filter from ASCII 32-126 to all printable (>=32, !=127). Added sr_ansi_to_utf8 conversion for character echo and sr_game_str() for buffer readback. (4) Variable substitution: Added $TECH# (Tech[ParseNumTable[slot]].name), $ABIL# (Ability[].name), $TERRAFORM# (Order[].order). Added sr_substitute_game_vars call to sr_try_tutorial_announce (was missing — $TECH0 appeared raw in tutorial text). 2 new loc strings (SR_INPUT_NUMBER_EMPTY, SR_INPUT_NUMBER_DONE) + 1 updated (SR_INPUT_NUMBER). TESTED — popup echo + umlauts OK, $TECH resolved OK.
- **2026-02-26 (session 5)**: Extended base open announcement. OnOpen() now builds announcement piece by piece instead of single format string. Order: (1) Special states first if present (drone riots, golden age, eco damage, nerve staple) — reuses existing loc strings. (2) Base name + population (new SR_FMT_BASE_OPEN_NAME). (3) Resources: nutrient surplus with growth turns, mineral surplus, energy surplus (new SR_FMT_BASE_OPEN_RESOURCES). (4) Mood: talents + drones, only if >0 (new SR_FMT_BASE_OPEN_MOOD). (5) Production with turns (new SR_FMT_BASE_OPEN_PROD). (6) Help hint on first open (unchanged). 4 new loc strings (en+de). TESTED OK by user.
- **2026-02-26 (session 6)**: Social Engineering handler extended + V key investigation. (1) SE total effects (G key): social_calc() with pending categories computes combined effects from all 4 models. Announces non-zero effects. (2) Energy allocation mode (W key): sub-mode with Up/Down slider select (Economy/Psych/Labs), Left/Right adjust in 10% steps, writes SE_alloc_psych/SE_alloc_labs directly. W toggles back to category mode. (3) Research/economy info (I key without Ctrl): announces current research, labs progress, estimated turns, energy credits, surplus. (4) Enhanced summary (S/Tab): now includes total effects + allocation + upheaval cost. 9 new loc strings (en+de). Updated help text. (5) V key stack cycling: WM_KEYDOWN arrives in HandleKey (confirmed via SR log), but WinProc call recurses into ModWinProc — game never processes V. TODO for next session: use return false + poll detection approach.
- **2026-02-26 (session 8)**: Message Handler (Ctrl+M). New files: message_handler.h/cpp. Ring buffer (50 messages) captures all NetMsg_pop calls with resolved text, coordinates, base_id, turn. Auto-announce on new message (queued, non-interrupting). Ctrl+M opens modal browser: Up/Down browse, Enter jumps to location (WorldMapHandler::SetCursor), S summary, Home/End first/last, Ctrl+F1 help, Escape close. Coverage: veh.cpp (~30 goody hut/monolith calls), faction.cpp (8 spy/atrocity calls), base.cpp (1), tech.cpp (1), patch.cpp (2 wrappers), veh_combat.cpp (4 combat reports), gui.cpp (diplomacy + 2 artillery messages). Used sr_NetMsg_pop() wrapper pattern in veh/faction/base/tech to avoid 30+ individual edits. WorldMapHandler::SetCursor() added — sets SR cursor, centers map, announces tile. 9 new loc strings (en+de). Build OK, deployed.
- **2026-02-26**: Design Workshop accessibility handler (Shift+D). New files: design_handler.h/cpp. Same modal loop pattern as SocialEngHandler. Two-level navigation: Level 1 = prototype list (default units with tech + custom units), Up/Down navigate, Enter edit, N new, Delete retire (two-press confirm), Escape close. Level 2 = component editor with 6 categories (Chassis/Weapon/Armor/Reactor/Ability1/Ability2), Left/Right cycles categories, Up/Down cycles tech-filtered options, S summary, Enter saves via mod_make_proto+mod_name_proto, Escape cancels back to list. Ability compatibility filtering via AFLAG_* flags (triad, combat/terraform/probe checks). Cost calculated via mod_proto_cost. Editing default units creates a copy in faction's custom slot. 26 new loc strings (en+de). Wired via Shift+D in world_map_handler.cpp, added to modal active check in gui.cpp. Build OK, deployed, awaiting testing.
- **2026-02-27 (session 2)**: Base screen gaps closed. (1) Resources V3: nutrient consumption, mineral support cost, energy inefficiency. Starvation warning at negative surplus. (2) Economy V3: commerce income from BaseCommerceImport. (3) Status extended: 7 warnings (starvation, low minerals, inefficiency, no HQ, high support, undefended, police count). (4) OnOpen: starvation + undefended warnings. Help text now via GetHelpText() (always in sync). (5) Ctrl+Shift+S: supported units list (home_base_id units, like garrison). (6) Ctrl+Shift+Y: psych detail one-shot. (7) gui.cpp: support mode interception + Ctrl+Shift+S/Y dispatch. ~20 new loc strings (en+de). TESTED OK.
- **2026-02-27 (session 4)**: Multiplayer Setup (NetWin) — Phase 1 investigation. (1) Transition detection: `in_netsetup = (*GameHalted && NetWin && Win_is_visible(NetWin))` announces screen open/close. (2) Initial text-capture attempt: unblocked timer/snap/nav for NetWin — caused constant noise (2 labels + player name alternating in 2 render cycles). Rolled back to silent mode (in_menu suppresses all triggers). (3) Junk filter: added 5 DirectPlay debug patterns (Guaranteed count, Sending, Average reply time, Last time, Flags). (4) Reverse-engineering: NetWin has 4 children. CH[0]+CH[1] are BaseButtons with name field at offset 0xA7C ("ABBRECHEN", "SPIEL STARTEN"). CH[2] is edit/chat field (vtbl 0x66a038). CH[3] is GraphicWin panel (470x107px) with hidden scrollbar — renders player list as pixels, no text items. CWinFonted hypothesis disproved (items=100 was wrong offset). Only 2-3 Buffer_write labels captured. Next: test popup dialogs on interaction, consider AlphaNet (0x93CD90) direct access.
- **2026-02-28**: Multiplayer Setup (NetWin) — Phase 2 reverse engineering complete. (1) Added NET-XY coordinate diagnostic: hooks log buffer pointer + x,y for every text render when in netsetup. (2) Added vtable detection for NetWin and all children. (3) Safe raw memory dump with IsBadReadPtr. (4) DialogChoices monitoring during setup. Complete coordinate map obtained: settings at x=11 (y=62-198), player list at x=357/537/679 (y=58-190), left checkboxes at x=32 (y=415-575), right checkboxes at x=426 (y=415-575). NetWin canvas buffer confirmed at 0x80AB3C. All vtables identified. Full results documented in docs/game-api.md. Ready for handler implementation.
- **2026-02-28 (session 4)**: Multiplayer Setup — Start/Cancel fix via disassembly of AlphaNet_setup (0x4E31E0). (1) Discovered `PbemActive` (0x93A95C) is the lobby dialog result: Cancel button (ID 9999 in pick_service loop at 0x4E26E0) sets it to 1, Start leaves it at 0. (2) After lobby: if PbemActive==0 → game shows accessible "Spiel leiten/Beitreten" popup (NETCONNECT_JOIN_OR_CREATE from "jackal" script), if !=0 → cancel path. (3) Fix: Cancel sets `*PbemActive=1` then `GraphicWin_close(NetWin)` → proper cancel. Start just calls `GraphicWin_close(NetWin)` with PbemActive=0 → popup appears for user. (4) Attempted and reverted: mod_netconnect_popup hook at 0x4E3266 — bypassing the popup caused DirectPlay crash (do_join null ptr) or wrong mode selection. The popup is already accessible, no hook needed. (5) Removed diagnostic dump_netwin_fields code. (6) Removed unused WinProc extern. PENDING TEST: Start → popup → Spiel leiten → game starts?
- **2026-03-01**: Multiplayer NetWin click simulation investigation. (1) Removed run_setting_modal + all SettingOption arrays + prepare_game/NetSetupState logic (~200 lines). (2) WinProc direct call `WinProc(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x,y))` — clicks were sent but game didn't respond at all (no popup, no state change). DDrawCompat (ddraw.dll in game dir confirmed) likely prevents synthetic mouse processing even via direct WinProc call. (3) SpotList scan: runtime scan of NetWin memory (offsets 0x444-0x1400) after first OnTimer redraw found SpotList at **NetWin+0xD34** (NOT 0xA2C/CWinFonted). 61 spots, max=85. Full hitbox type map: type=0 settings, type=1 checkboxes, type=2/5 player name areas, type=3 faction column, type=4 difficulty column. (4) iHitBoxTagClicked write at NetWin+0xD2C (CWinFonted analogy) — no effect, game didn't process it. (5) Vtable dump code deployed (40 entries) but not yet tested — user ended session. Next: analyze vtable, find on_left_click handler, call directly as `vtbl[N](NetWin, x, y)`.
- **2026-02-27**: Load Game + Save Game dialog accessibility + status cleanup. (1) Load Game file browser: Initial approach (parallel file list with own index, popup_list navigation) failed after 5 iterations — our index desynchronized with game's internal cursor, Enter on folders was consumed but game didn't navigate, user ended up in C:\ root. Root cause: game's file dialog is a modal with its own state we can't control. **Rewrote to text-capture approach**: game handles all navigation (arrows/Enter/Escape), we observe rendered filenames via Trigger 2 (Arrow Nav) and route through sr_fb_on_text_captured() which adds type markers (Ordner/SAV). Directory names from saves/ scanned via FindFirstFile as lookup table. Triggers 1+3 suppressed during file browser. TESTED OK. (2) Status cleanup: Ctrl+R conflict RESOLVED, Setup options RESOLVED (already works), Base screen mini-map NOT NEEDED (read-only visual), Zoom feedback NOT NEEDED (purely visual). (3) Key lesson documented in memory: for game modal dialogs, always observe+annotate, never build parallel navigation. (4) Save Game dialog: Hooked save_game at 0x5A9EB0 with same text-capture pattern as load_game (shared sr_fb_open/sr_fb_close helpers). "Spiel speichern" announced on Ctrl+S. Modified sr_fb_on_text_captured() to announce button text (OK, ABBRECHEN, SPEICHERN, LADEN, CANCEL, SAVE, LOAD) directly instead of skipping — enables overwrite confirmation dialog options to be read. SR_FILE_SAVE_GAME loc string added (en+de). Inline hook slot 24 used. Awaiting testing.
- **2026-03-02**: Three small features. (1) D key on world map: SR feedback for "Destroy Improvements" (D without modifiers). Follows H-key pattern (pass to WinProc, announce). New string SR_ACT_DONE_DESTROY (en: "Destroying improvements.", de: "Zerstört Verbesserungen."). (2) Scenario Editor announcement: Detects STATE_SCENARIO_EDITOR activation (Ctrl+K) via static was_editor flag in ModWinProc. Announces "Scenario editor. Not yet accessible with screen reader." New string SR_EDITOR_NOT_ACCESSIBLE. (3) Monument F9 disclaimer: Modified SR_MONUMENT_OPEN to include "Visualization of achievements, list may be inaccurate" / "Visualisierung der Erfolge, Liste kann fehlerhaft sein" — warns user that monument data readout may have errors.

## Future: Scenario Editor Accessibility

**Status:** Deferred. Currently announces "not yet accessible" on activation (Ctrl+K). Full accessibility is a large project.

**Alternative approach to investigate first:** Many scenario settings can also be defined via plain text files (alphax.txt, scenario .txt files). Before building full in-game editor accessibility, investigate how much can be done through text file editing — potentially more practical for blind users than navigating complex visual editor UIs.

### Part 1: In-Game Scenario Editor (Ctrl+K) — 26 functions
Activated via Ctrl+K. Mostly uses popup dialogs (popp, X_pop7, pop_ask_number) which are already partially accessible via existing hooks.

**Functions (Shift/Ctrl+F-key hotkeys):**
- Shift+F1: Create unit (Console_editor_veh, 0x4DED00)
- Ctrl+Shift+F1: Edit unit (Console_editor_edit_veh, 0x4DDA50)
- Shift+F2: Tech advance (Console_editor_tech, 0x4DFAD0)
- Ctrl+F2: Modify tech
- Shift+F3: Swap factions (Console_editor_who, 0x4D9AD0)
- Ctrl+Shift+F3: Set difficulty (Console_editor_diff, 0x4DD6F0)
- Shift+F4: Set energy (Console_editor_energy, 0x4E0120)
- Shift+F5: Set mission year (Console_editor_date, 0x4E0210)
- Shift+F6: Delete faction (Console_editor_eliminate, 0x4E05E0)
- Ctrl+Shift+F6: Reload faction (Console_editor_reload, 0x4E0290)
- Ctrl+F6: Delete all units (Console_editor_kill_vehicles, 0x4E09B0)
- Shift+F7: View replay
- Shift+F8: View videos
- Ctrl+F8: Reset techs (Console_editor_reset_tech, 0x4DBC40)
- Ctrl+Shift+F8: Reset all factions (Console_editor_reset_faction, 0x4DBB40)
- Shift+F9: Edit diplomacy (Console_editor_diplomacy, 0x4DB870)
- Ctrl+F9: Edit personality (Console_editor_personality, 0x4DBD20)
- Ctrl+Shift+F9: Edit strategy (Console_editor_strategy, 0x4DBFB0)
- Shift+F10: Edit rules (Console_editor_rules, 0x4DC230)
- Shift+F11: Edit scenario rules (Console_editor_scen_rules, 0x4DC520)
- Shift+F12: Edit scenario params (Console_editor_scen_param, 0x4DCCC0)
- Ctrl+Shift+F12: Edit victory conditions (Console_editor_scen_victory, 0x4DC860)
- Ctrl+Shift+F7: Load scenario (Console_editor_load, 0x4E0A00)
- Ctrl+F7: Save scenario (Console_editor_save, 0x4E09E0)
- Alt+Backspace: Undo (Console_editor_undo, 0x4E1F20)
- Y: Overview (Console_editor_view, 0x4DF4F0)

**Estimated effort:** Medium. Many functions use existing popup system. First step: test which hotkeys already produce accessible output via popup hooks.

### Part 2: Map Editor (Terrain Painting) — 20 functions
Completely visual brush-based painting interface. Number keys 0-9 select tools, Ctrl+Left/Right places/removes features.

**Functions:**
- 1-6: Select terrain type (elevation, rocks, rivers, resources, pods, improvements)
- 7: Change brush size (Console_editor_set_brush, 0x4E0EA0)
- 8: Climate/world params (Console_editor_climate, 0x4E0FA0)
- 9: Slow map generate (Console_editor_generate, 0x4E0FD0)
- 0: Fast map generate (Console_editor_fast, 0x4E10C0)
- Ctrl+Left: Place at cursor (Console_editor_paints, 0x4E0A50)
- Ctrl+Right: Remove at cursor
- Ctrl+F5: Save map, Ctrl+Shift+F5: Load map
- Fungus generate/remove, random rockiness, resource beacons, polar caps, clear terrain
- Ctrl+Shift+F10: Editor-only mode toggle

**Estimated effort:** Very large. Visual painting concept requires completely different interface for blind users (coordinate-based placement instead of brush painting). Low priority.

### Text File Alternative
SMAC scenarios can be partially defined through text files:
- **alphax.txt**: Game rules, tech tree, unit definitions, facilities, abilities, terrain params
- **Scenario .txt files**: Custom rules, victory conditions, faction settings
- **Map files (.mp)**: Binary format, but map generators exist
- Investigate: What scenario features can be fully configured via text editing? Could a text-based workflow replace most editor functions for blind users?
