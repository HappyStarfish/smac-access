# SMAC Game API Reference

Central documentation for game structures, keys, menus, and patterns.
Update this file after every new code analysis. Never re-analyze what is documented here.

---

## Game Window States

The game tracks which window has focus via `current_window()` (gui.cpp):

- `GW_None` — No game window (main menu, between states)
- `GW_World` — World map visible and focused (main gameplay)
- `GW_Base` — Base window visible (overlays world map)
- `GW_Design` — Unit design window visible

Detection uses `Win_get_key_window()` and compares against `MainWinHandle` (0x939444).

---

## Menu Structure

From `menu.txt` in game directory. Format: `#CATEGORY` followed by items with `|` hotkey separator.

### Game Menu (#GAME)
- Switch To Detailed/Simple Menus (F11)
- Preferences (Ctrl+P) — sub-tabs: General, Warning, Advanced, Automation, Audio/Visual, Map Display
- Save Game (Ctrl+S)
- Load Game (Ctrl+L)
- Resign
- Start Over
- Quit (Ctrl+Q)

### HQ Menu (#HQ) — Main Command Center
- Social Engineering (Shift+E)
- Research Priorities (Shift+R)
- Design Workshop (Shift+D)
- Datalinks (F1)
- Laboratories Status (F2)
- Energy Banks (F3)
- Base Operations Status (F4)
- Secret Project Data (F5) — CONFLICTS with current SR read key!
- Orbital and Space Status (F6)
- Military Command Nexus (F7)
- Alpha Centauri Score (F8)
- View Monuments (F9)
- Hall of Fame (F10)
- Communications and Protocol (F12)

### Map Menu (#MAP)
- Move/View Toggle (V)
- Zoom controls (+/-)
- Grid (Ctrl+G)
- Show terrain (T)
- Center screen (C)

### Action Menu (#ACTION)
- Construct Base (B)
- Long Range Fire (F)
- Airdrop (I)
- Skip Turn (Space)
- Hold Position (H)
- Go to Base (Shift+G)
- Explore (X)
- Patrol (P)

### Terraform Menu (#TERRAFORM)
- Farm+Solar+Road (F,S,R combinations)
- Road (R), Tube (U)
- Automatic improvements (Shift+A)
- Fungus Removal (F)

### Help Menu (#HELP)
- Index, Concepts, Technologies, Facilities
- 5 Tours: Interface, Terraforming, Base Control, Social Engineering, Design Workshop
- Version/About

---

## Game Key Bindings (CRITICAL — never conflict!)

### Function Keys (ALL USED by game!)
- F1: Datalinks
- F2: Laboratories Status
- F3: Energy Banks
- F4: Base Operations Status
- F5: Secret Project Data
- F6: Orbital/Space Status
- F7: Military Command Nexus
- F8: Alpha Centauri Score
- F9: View Monuments
- F10: Hall of Fame
- F11: Switch Menus
- F12: Communications/Protocol

### Shift+Function Keys
- Shift+E: Social Engineering
- Shift+R: Research Priorities
- Shift+D: Design Workshop
- Shift+G: Go to Base
- Shift+A: Auto-improve

### Ctrl Combinations
- Ctrl+P: Preferences
- Ctrl+S: Save
- Ctrl+L: Load
- Ctrl+Q: Quit
- Ctrl+G: Grid
- Ctrl+H: Hurry production (base window, Thinker addition)
- Ctrl+Left/Right: Tab base resource windows (Thinker addition)
- Ctrl+Up/Down: Zoom base resource info (Thinker addition)

### Alt Combinations (Thinker Mod)
- Alt+T/Alt+H: Mod menu
- Alt+R: Mouse-over tile info toggle
- Alt+O: Map generator (editor only)
- Alt+L: Replay (editor only)

### Current Screen Reader Keys
- Ctrl+R: Read captured screen text
- Ctrl+Shift+R: Stop speech (silence)
- Ctrl+F12: Toggle debug logging (writes to %TEMP%\thinker_sr.log)

### Safe Mod Keys (Available)
- Ctrl+F1 through Ctrl+F12 (most unused)
- Ctrl+Shift combinations
- Alt+Shift combinations
- NOTE: Ctrl+F12 is now used for SR debug toggle

---

## Dialog System

### PopDialog Flags (gui_dialog.h)
- `PopDialogCheckbox` (0x1)
- `PopDialogListChoice` (0x2)
- `PopDialogTextInput` (0x4)
- `PopDialogLargeWindow` (0x80)
- `PopDialogSmallWindow` (0x400)

### Dialog Functions (engine.h)
- `X_pop2()`, `X_pop3()` — Simple popup dialogs
- `BasePop_start()` — Base-related popups
- `NetMsg_pop()` — Network message notifications
- `pop_ask_number()` — Number input dialog
- `Popup_start()` — Generic popup window

### Diplomacy Dialogs (gui_dialog.h)
- Treaty proposals, tech trades, energy trades
- Base swaps, joint attacks, spying notifications
- Council votes

---

## Text File Format

Game text files use a marker-based system:

- `#LABEL_NAME` — section marker, referenced by code
- `^` — line break within text
- `{TEXT}` — emphasized text
- `$BASENAME0`, `$UNITNAME1`, `$FACTION3` — runtime variable substitution
- `$STR0`, `$NUM0`, `$NUM1` — template parameters for help system
- `|` — hotkey separator in menus (e.g., "Save Game|Ctrl+S")

### Key Text Files
- `labels.txt` — 1131 UI labels (numbered, used throughout UI)
- `menu.txt` — Menu structure and hotkeys
- `tutor.txt` — Tutorial text (1445 lines, ~30 topics)
- `help.txt` / `helpx.txt` — Help system definitions
- `alpha.txt` / `alphax.txt` — Game rules, factions, technologies, facilities
- `Blurbs.txt` / `blurbsx.txt` — Flavor text, quotes

---

## Tutorial Structure

Tutorial in `tutor.txt`, organized by `#LABEL` markers:

### Getting Started
- #LANDFALL — Colony pod landing, first steps
- #DIRECTIONS — Cardinal directions
- #TOUR1 / #TOUR1B — Unit movement basics

### Interface Tours
- #TOURMENU — Main menu
- #TOURCOMMLINK — Communications
- #TOURUNITSINFO — Unit info window
- #TOURTERRAININFO — Terrain info
- #TOURSTACKINFO — Unit stack viewing

### Resources and Terrain
- #TOURMAP — Three resources (Nutrients, Minerals, Energy)
- #TOURRAIN* — Rainfall and terrain types
- #TOURALT — Altitude and energy

### Terraforming Tour
- #TOURFOREST — Basic improvements (Farm, Mine, Solar, Forest, Road)
- #TOURADVANCED — Advanced terraforming

### Base Control Tour
- #BASECONTROL — Base management overview
- #TOUR1GOVERNOR — Governor modes (Explore, Discover, Build, Conquer)
- #TOUR1POP* — Population management
- #TOUR1NUT, #TOUR1MIN, #TOUR1ENERGY — Resources

### Design Workshop Tour
- #DESWIN — Unit design
- #DESTOURCHAS through #DESTOURRETIRE — Components

### Social Engineering Tour
- #SOCWIN — Overview
- #SOCTOURMAIN — Four categories
- #SOCTOUR0-9 — Ten social effects

### Special Topics
- #DRONERIOTS / #DRONETOUR1-7 — Drone management
- #MINDWORMS — Native creatures
- #PSICOMBAT — Psi combat
- #VENDETTA — War
- #COMMERCE — Trade

---

## Hooked Functions (Thinker + Accessibility)

### Text Capture Hooks (screen_reader.cpp)
5 Buffer_write functions hooked via inline hooking for text capture:
1. `Buffer_write_l` (0x5DCEA0) — left-aligned
2. `Buffer_write_cent_l` (0x5DD020) — centered
3. `Buffer_write_right_l2` (0x5DD3B0) — right-aligned
4. `Buffer_wrap2` (0x5DD730) — word-wrapped dialogs
5. `Buffer_wrap_cent` (0x5DD920) — centered word-wrap

### Thinker Game Logic Hooks (patch.cpp, partial list)
- `MapWin_gen_overlays` (0x4688E0)
- `base_compute` (0x4EC3B0)
- `social_calc` (0x5B4210)
- `X_pop2` (0x5BF310) — popup dialog
- `say_tech` (0x5B9C40)
- `prefs_load`, `prefs_put`, `prefs_save`
- Diplomacy functions: treaty_on/off, agenda_on/off

### Input Hook
- `ModWinProc` replaces original `pfncWinProc` (0x5F0650) via RegisterClassA patch

---

## Key Memory Addresses

- Game code base: 0x401000, length 0x263000
- Import table: 0x669000
- MainWinHandle: 0x939444
- MapWin pointer: 0x9156B0
- Config file: "./thinker.ini"
- Registry: "Alpha Centauri"

---

## Base Screen — Full Interactive Reference

The base screen (`current_window() == GW_Base`) is the most complex screen in the game.
A sighted player sees and interacts with: production, queue, specialists, governor, facilities, and more.

### Current Accessibility Status

Already accessible (keyboard + SR):
- Ctrl+Up/Down: cycle 7 info sections (Overview, Resources, Production, Economy, Facilities, Status, Units)
- Ctrl+Left/Right: switch tabs (Resources=0, Citizens=1, Production=2)
- Ctrl+I: repeat current section
- Ctrl+F1: base help text
- Ctrl+H: hurry production (Thinker addition) — NO SR feedback yet
- Escape: close base screen (game-native)

Needs implementation (mouse-only for sighted players):
- Change production (select what to build)
- Production queue management (reorder, remove)
- Specialist management (reassign workers/specialists)
- Governor configuration (toggle, set priorities)
- Facility demolition (sell/scrap)
- Base rename
- Nerve staple (faction-dependent)

### BASE Struct Key Fields (engine_base.h, 308 bytes total)

Population and citizens:
- `int8_t pop_size` — population (actual citizens = pop_size + 1, includes base center worker)
- `int32_t worked_tiles` — bitmap, each bit = one tile in base radius being worked (bit 0 = center, always set)
- `int32_t specialist_total` — count of citizens assigned as specialists (not in worked_tiles)
- `int32_t specialist_adjust` — pending specialist adjustment (Thinker variable)
- `int32_t specialist_types[2]` — packed 4-bit citizen type IDs (8 per int, 16 max: `MaxBaseSpecNum=16`)
- `int32_t talent_total` — talented citizens
- `int32_t drone_total` — drone citizens
- `int32_t superdrone_total` — superdrone citizens
- Formula: `pop_size + 1 == popcount(worked_tiles) + specialist_total` (base.cpp:814)

Production and queue:
- `int32_t queue_items[10]` — production queue. `queue_items[0]` = current item
- `int32_t queue_size` — number of items in queue
- `int32_t minerals_accumulated` — minerals accumulated toward current production
- `int item()` — returns `queue_items[0]`
- `bool item_is_unit()` — true if `queue_items[0] >= 0`
- `bool item_is_project()` — true if `queue_items[0] <= -SP_ID_First`

Item ID encoding:
- Positive (>=0): unit prototype IDs. Name via `Units[item_id].name`
- Negative (<0): facility/project IDs (negated). Name via `Facility[-item_id].name`
- Example: Scout Patrol = 0, Solar Collector = -1

Resources:
- `int32_t nutrients_accumulated` — progress toward population growth
- `int32_t nutrient_surplus`, `mineral_surplus`, `energy_surplus` — per-turn values
- `int32_t economy_total`, `psych_total`, `labs_total` — energy allocation

State flags (`int32_t state_flags`):
- `BSTATE_DRONE_RIOTS_ACTIVE` = 0x2
- `BSTATE_GOLDEN_AGE_ACTIVE` = 0x4
- `BSTATE_FACILITY_SCRAPPED` = 0x200 — one scrap per turn (cleared in upkeep)
- `BSTATE_RENAME_BASE` = 0x10000 — set on capture, allows rename
- `BSTATE_PRODUCTION_DONE` = 0x800000
- `BSTATE_HURRY_PRODUCTION` = 0x40000000

Other:
- `char name[25]` — base name (24 chars + null)
- `int32_t governor_flags` — governor configuration (see Governor section)
- `uint8_t nerve_staple_turns_left` — countdown
- `int32_t nerve_staple_count` — total times stapled
- `int32_t eco_damage` — environmental damage percentage
- `uint8_t facilities_built[12]` — bitfield (96 bits for ~64 facilities)
- `bool has_fac_built(FacilityId id)` — checks bitfield
- `bool can_hurry_item()` — checks if hurry is allowed (no riots, not already hurried, etc.)
- `bool drone_riots_active()` — checks BSTATE_DRONE_RIOTS_ACTIVE
- `bool golden_age_active()` — checks BSTATE_GOLDEN_AGE_ACTIVE
- `uint32_t gov_config()` — returns governor_flags for humans, ~0u for AI

### Key Memory Pointers

- `Bases` array: 0x97D040 (BASE[512])
- `CurrentBase` pointer: 0x90EA30 (BASE** — points to current base being edited)
- `CurrentBaseID`: 0x689370 (int*)
- `BaseCount`: 0x9A64CC (int*)
- `Citizen` array: 0x946020 (CCitizen[10], 28 bytes each)
- `Facility` array: accessed via `Facility[-item_id]`
- `Units` array: accessed via `Units[item_id]`
- `Factions` array: 0x96C9E0

### Production Change

How sighted players change production: click on production area or "Change" button.
The game opens a selection dialog listing all buildable items.

Key functions:
- `base_production` (0x4F07E0, `fp_none`) — called during upkeep when production completes. Advances queue, shows notifications.
- `base_queue` (0x4F06E0, `fp_1int`) — queue management function. Takes base_id.
- `facility_avail` (0x5BA0E0, `fp_4int`) — checks if facility can be built. Args: (FacilityId, faction_id, base_id, queue_count). Returns 1 if buildable.
- `veh_avail` (0x5BA910, `fp_3int`) — checks if unit prototype can be built. Args: (unit_id, faction_id, base_id). Returns 1 if buildable.
- `popp` (0x48C0A0, `Fpopp`) — game's popup dialog. Signature: `(filename, label, a3, pcx_filename, a5)`.
- `BasePop_start` (0x601BF0, `FBasePop_start`) — base popup infrastructure. `__thiscall` on popup object.
- `pop_ask_number` (0x48AA20, `Fpop_ask_number`) — number input dialog.
- Thinker hook: `base_production_popp` at 0x4F2A4C — intercepts production completion popup.

Cost functions (base.cpp):
- `prod_name(int item_id)` — returns item name string (base.cpp:2253)
- `mineral_cost(int base_id, int item_id)` — returns total mineral cost with faction modifiers (base.cpp:2262)
- `hurry_cost(int base_id, int item_id, int hurry_mins)` — returns energy credit cost to hurry (base.cpp:2278)
  - Facilities: `2 * minerals_remaining`
  - Units: `minerals_remaining^2 / 20 + 2 * minerals_remaining`
  - Doubles if `minerals_accumulated < Rules->retool_exemption`

Accessibility plan for production change:
- New hotkey (e.g. Ctrl+P or a dedicated key) opens an SR-navigable production picker
- Enumerate all buildable items: loop units 0..MaxProtoNum-1 with `veh_avail()`, facilities 1..Fac_ID_Last with `facility_avail()`
- For each item: name via `prod_name()`, cost via `mineral_cost()`, turns via `(cost - minerals_accumulated) / mineral_surplus`
- Navigate with Up/Down arrows, confirm with Enter, cancel with Escape
- Set `base->queue_items[0] = selected_item_id` and `base->minerals_accumulated = 0` (or keep if same category)
- Announce: "Building [Name], [Cost] minerals, [Turns] turns"

### Production Queue Management

Queue data: `base->queue_items[0..9]`, `base->queue_size`
- `queue_items[0]` is always the current production
- Items 1..queue_size-1 are queued for later
- Max 10 items total

Accessibility plan for queue:
- New hotkey (e.g. Ctrl+Q) enters queue mode
- Up/Down: navigate queue items
- Announce: "[Position] of [Total]: [ItemName], [Cost] minerals"
- Shift+Up/Shift+Down: move item up/down in queue (swap adjacent entries)
- Delete: remove item from queue (shift remaining items)
- Enter/Insert: add new item at position (opens production picker)
- Escape: exit queue mode

### Specialist Management

The game has up to 7 specialist types (MaxSpecialistNum=7), plus 3 other citizen types (MaxCitizenNum=10).

CCitizen struct (engine_types.h:641, 28 bytes):
- `char* singular_name` — "Librarian", "Engineer", "Doctor", etc.
- `char* plural_name`
- `int32_t preq_tech` — tech ID required to unlock this specialist
- `int32_t obsol_tech` — tech that makes this obsolete
- `int32_t econ_bonus` — economy bonus per specialist
- `int32_t psych_bonus` — psych bonus per specialist
- `int32_t labs_bonus` — labs bonus per specialist

Specialist data in BASE:
- `specialist_total` — how many citizens are specialists
- `specialist_types[2]` — packed 4-bit IDs. Access via:
  - `base->specialist_type(index)` — returns citizen_id for specialist at index 0..15
  - `base->set_specialist_type(index, citizen_id)` — sets citizen type
- Storage: `specialist_types[index/8] >> 4*(index & 7)) & 0xF`

Key functions:
- `best_specialist` (0x4E4020, patched to `mod_best_specialist`) — auto-picks optimal specialist type. Args: (BASE*, econ_val, labs_val, psych_val). Returns citizen_id.
- Governor with `GOV_MANAGE_CITIZENS` auto-manages workers/specialists (resets worked_tiles and recalculates).
- Worker management is complex: tiles scored by yield, specialists assigned based on economic weights.

Worker vs specialist tradeoff:
- Workers occupy tiles (worked_tiles bitmap). Each tile yields nutrients/minerals/energy.
- Specialists don't work tiles but provide flat bonuses (econ/psych/labs).
- Reassigning: remove worker from worked_tiles bit, increment specialist_total, set specialist_type.
- After changes: call `base_compute(1)` (0x4EC3B0) to recalculate all base outputs.

Accessibility plan for specialists:
- New hotkey (e.g. Ctrl+W) enters specialist mode
- Show current: "[N] workers, [M] specialists: 2 Librarians, 1 Doctor"
- Up/Down: cycle through specialist slots
- Left/Right: change specialist type (cycle through unlocked types via preq_tech check)
- Enter: convert one worker to specialist (increment specialist_total, set type)
- Delete/Backspace: convert specialist back to worker (decrement specialist_total)
- After any change: recalculate with `base_compute(1)`
- Announce new resource totals after changes

### Governor Configuration

Governor flags (engine_base.h:58-91, `int32_t governor_flags`):

Core:
- `GOV_ACTIVE` = 0x80000000 — governor enabled/disabled
- `GOV_MANAGE_PRODUCTION` = 0x1 — governor controls production choices
- `GOV_MANAGE_CITIZENS` = 0x40 — governor manages worker/specialist allocation
- `GOV_MAY_HURRY_PRODUCTION` = 0x20 — governor can hurry items
- `GOV_NEW_VEH_FULLY_AUTO` = 0x80 — new vehicles get full automation
- `GOV_MAY_FORCE_PSYCH` = 0x2 — allow forced psych (Thinker variable)
- `GOV_MULTI_PRIORITIES` = 0x200000 — allow multiple priority flags

Allowed production types:
- `GOV_MAY_PROD_LAND_COMBAT` = 0x200
- `GOV_MAY_PROD_NAVAL_COMBAT` = 0x400
- `GOV_MAY_PROD_AIR_COMBAT` = 0x800
- `GOV_MAY_PROD_LAND_DEFENSE` = 0x1000
- `GOV_MAY_PROD_AIR_DEFENSE` = 0x2000
- `GOV_MAY_PROD_TERRAFORMERS` = 0x8000
- `GOV_MAY_PROD_FACILITIES` = 0x10000
- `GOV_MAY_PROD_COLONY_POD` = 0x20000
- `GOV_MAY_PROD_SP` = 0x40000 — secret projects
- `GOV_MAY_PROD_PROTOTYPE` = 0x80000
- `GOV_MAY_PROD_PROBES` = 0x100000
- `GOV_MAY_PROD_EXPLORE_VEH` = 0x400000
- `GOV_MAY_PROD_TRANSPORT` = 0x800000
- `GOV_MAY_PROD_NATIVE` = 0x100 — (Thinker variable)

Priority flags:
- `GOV_PRIORITY_EXPLORE` = 0x1000000
- `GOV_PRIORITY_DISCOVER` = 0x2000000
- `GOV_PRIORITY_BUILD` = 0x4000000
- `GOV_PRIORITY_CONQUER` = 0x8000000

BaseGovOptions table (engine_base.h:93-115) maps dialog checkbox bits to governor flags.
The game's governor dialog uses `X_pop("modmenu", "GOVOPTIONS", ...)` with `*DialogChoices` for result.
Thinker processes the result in gui.cpp:2754-2770.

When governor changes:
- `GOV_MANAGE_PRODUCTION` enabled: calls `mod_base_reset(base_id, 1)` to rebuild queue
- `GOV_MANAGE_CITIZENS` enabled: resets `worked_tiles=0, specialist_total=0`, calls `base_compute(1)`

Accessibility plan for governor:
- New hotkey (e.g. Ctrl+G) enters governor mode
- Announce current state: "Governor [active/inactive]. Priority: [Explore/Discover/Build/Conquer]"
- Toggle on/off with Enter or Space
- Up/Down: cycle through options (each GOV_ flag as a toggleable item)
- Left/Right or Space: toggle individual flag
- Group logically: Core settings, production permissions, priorities
- Announce each toggle: "[Option] [enabled/disabled]"
- On exit: apply flags, trigger base_compute if citizens changed

### Facility Demolition

Sighted players: right-click or button to demolish a built facility. One per turn limit (BSTATE_FACILITY_SCRAPPED).

Key data:
- `base->has_fac_built(FacilityId)` — checks if built
- `base->facilities_built[12]` — bitfield storage
- `BSTATE_FACILITY_SCRAPPED` = 0x200 — prevents more than one scrap per turn, cleared in upkeep

CFacility struct (engine_types.h:613, 48 bytes):
- `char* name` — facility name
- `char* effect` — description text
- `int32_t cost` — mineral cost to build
- `int32_t maint` — per-turn maintenance cost
- `int32_t preq_tech` — required technology

Accessibility plan for facility demolition:
- In Facilities section (Ctrl+Up/Down to BS_Facilities), add sub-navigation
- Up/Down: cycle through built facilities (iterate facilities_built bitfield)
- Announce: "[Position] of [Total]: [Name], maintenance [Cost]"
- Delete key: initiate demolition with confirmation
- Check BSTATE_FACILITY_SCRAPPED first — "Cannot: already scrapped one this turn"
- On confirm: clear bit in facilities_built, set BSTATE_FACILITY_SCRAPPED, call base_compute(1)
- Announce: "[FacilityName] demolished"

### Nerve Staple

Available to factions with certain social engineering settings. Suppresses drone riots for several turns.

Key functions:
- `BaseWin_nerve_staple` (0x41B4F0, `FGenWin`, `__thiscall`) — opens nerve staple dialog on base window
- `action_staple` (0x4CA7F0, `fp_1int`) — performs the staple action. Takes base_id.
- `can_staple(int base_id)` (build.cpp) — checks if stapling is allowed
- Thinker wrapper: `BaseWin_action_staple(int base_id)` checks `can_staple()` then calls `action_staple()`
- Thinker wrapper: `BaseWin_click_staple(Win* This)` gets base_id from BaseWindow, checks conf.nerve_staple setting

BASE fields:
- `nerve_staple_turns_left` — countdown timer (uint8_t)
- `nerve_staple_count` — total times stapled (int32_t)

Accessibility plan:
- New hotkey (e.g. Ctrl+N) in base screen
- Check `can_staple(*CurrentBaseID)` first
- If not allowed: "Cannot: nerve staple not available"
- If allowed: confirmation prompt, then call `action_staple(*CurrentBaseID)`
- Announce: "Nerve staple applied, [turns] turns"

### Base Rename

Base names stored in `base->name[25]` (24 chars + null terminator, MaxBaseNameLen=25).

Rename happens:
- On capture: BSTATE_RENAME_BASE set, renamed during upkeep via `mod_name_base()`
- Manual: through base screen UI (mouse-only currently)
- `mod_name_base(faction_id, name_buf, save_offset, sea_base)` — auto-generates name (game.cpp:1570)

Accessibility plan:
- New hotkey (e.g. Ctrl+N is taken — use F2 or Alt+R)
- Announce current name: "Base name: [Name]"
- Need text input — could use `pop_ask_number` or custom approach
- Alternative: cycle through unused faction base names with Up/Down, confirm with Enter
- After rename: `strncpy(base->name, new_name, 24)` and redraw

### Hurry Production — SR Feedback (Quick Fix)

Ctrl+H already works (gui.cpp:1596-1615) but gives no screen reader feedback.

Current code flow:
1. Checks `base->can_hurry_item()`, cost > 0, minerals remaining > 0
2. Checks `f->energy_credits - f->hurry_cost_total >= cost`
3. On success: deducts credits, adds minerals, sets BSTATE_HURRY_PRODUCTION
4. On failure: plays error sound (inaudible to SR user)

Fix needed:
- After successful hurry: announce "Hurried [ItemName]. [Cost] credits spent, [Remaining] credits left"
- On insufficient credits: announce "Cannot hurry: need [Cost] credits, have [Available]"
- On cannot hurry: announce "Cannot hurry: [Reason]" (riots, already hurried, etc.)

### Implementation Priority

1. **Quick fix**: Ctrl+H SR feedback (small change, high value)
2. **Production change**: most-used action, needs item picker dialog
3. **Production queue**: extends production change, same UI patterns
4. **Specialist management**: important for mid/late game optimization
5. **Facility demolition**: occasional use, simpler than specialists
6. **Governor configuration**: toggle + options list
7. **Base rename**: rare action
8. **Nerve staple**: faction-specific, rare

---

## Thinker INI Settings (Accessibility-Relevant)

- `video_mode=0` — 0=fullscreen, 1=windowed, 2=borderless
- `window_width`, `window_height` — resolution
- `MainFontSize=16` — UI font size (customizable)
- `render_base_info=1` — extra base details
- `render_probe_labels=1` — faction-colored labels
- `warn_on_former_replace=1` — replacement warnings
- `foreign_treaty_popup=1` — diplomatic notifications
- `smooth_scrolling=0` — map scrolling style
- `label_base_surplus`, `label_pop_size` etc. — customizable UI labels

---

## Social Engineering Screen

### Game Data
- `SocialWin` (0x8A6270) — Win* pointer for the SE dialog
- `SocialField[4]` (0x94B000) — CSocialField array, one per category
  - `field_name` — category name (Politics, Economics, Values, Future Society)
  - `soc_name[4]` — model names within category
  - `soc_preq_tech[4]` — tech prerequisite per model (TECH_None = no req, TECH_Disable = disabled)
  - `soc_effect[4]` — CSocialEffect per model (economy, efficiency, support, talent, morale, police, growth, planet, probe, industry, research)
- `Factions[id].SE_Politics` through `.SE_Future` — current active settings (4 ints)
- `Factions[id].SE_Politics_pending` through `.SE_Future_pending` — pending changes
- `MFactions[id].soc_opposition_category/model` — faction opposition (one model blocked)

### Game Functions
- `social_select` (0x5B5620, fp_1int) — opens the SE dialog with a modal message pump
- `social_set` (0x5B4600, fp_1int) — applies pending SE changes to faction
- `social_upheaval` (0x5B4550, fp_2int) — calculates energy cost of SE changes
- `society_avail` (0x5B4730, fp_3int) — checks if model is available (tech + opposition)

### Critical Lessons: Modal Dialogs and WM_CHAR

**Problem:** The game's `social_select` runs a modal message pump (nested PeekMessage/GetMessage loop). Once entered, there is no known way to exit it programmatically — `GraphicWin_close`, `SubInterface_release_iface_mode`, clearing WIN_VISIBLE flag, and direct field manipulation all failed across 4+ attempts. The game stays in the modal loop even when the window is hidden.

**Solution:** Intercept the 'E' key in ModWinProc BEFORE the game sees it and run our own modal PeekMessage loop (`RunModal()`). The game's `social_select` is never called. We read all SE data directly from game memory and handle the full interaction ourselves.

**WM_CHAR trap (critical!):** When intercepting WM_KEYDOWN for a letter key (e.g., 'E') and running a modal loop, the corresponding WM_CHAR message is ALREADY in the queue (placed by TranslateMessage before DispatchMessage called our WinProc). If the modal loop dispatches this WM_CHAR, it reaches the game's original WinProc, which triggers the very function we're trying to bypass. Fix: consume ALL WM_CHAR messages while the modal handler is active:
```cpp
} else if ((msg == WM_KEYDOWN || msg == WM_CHAR) && handler_active) {
    if (msg == WM_KEYDOWN) { handler->Update(msg, wParam); }
    return 0; // consume WM_CHAR too
}
```

**General rule for future modal handlers:** Any time we intercept a letter-key WM_KEYDOWN and enter a modal loop, we MUST also intercept WM_CHAR to prevent the translated character from reaching the game's WinProc. This applies to all letter keys, not just 'E'.

---

## Diplomacy System

### Key Memory Addresses
- `DiploWinState` (0x93FAB4, int*) — non-zero when diplomacy communication window is active
- `diplo_second_faction` (0x93F7CC, int*) — faction ID of the other party in diplomacy
- `diplo_third_faction` (0x93F7D4, int*) — third faction (for three-way negotiations)
- `diplo_tech_faction` (0x93FA38, int*) — faction for tech trade context
- `diplo_entry_id` (0x93FAA8, int*) — tech entry for trade
- `diplo_current_proposal_id` (0x93FA34, int*) — active proposal type
- `diplo_counter_proposal_id` (0x93FAB0, int*) — counter-proposal type
- `diplo_ask_base_swap_id` (0x93FA7C, int*) — base being asked for
- `diplo_bid_base_swap_id` (0x93FA30, int*) — base being offered

### Key Functions
- `communicate` (0x54FFD0, fp_3int) — Main diplomacy session. Args: (faction1, faction2, flag). Has its own modal loop — CANNOT be replaced. Flag=1 for player-initiated contact.
- `proposal_menu` (0x54DCF0, fp_2int) — Shows proposal options (what to offer/demand)
- `make_a_proposal` (0x54F420, fp_3int) — Executes a specific proposal
- `commlink_attempter` (0x5589E0, fp_2int) — Game's F12 handler. Shows its own faction popup, THEN calls communicate. NOT usable for accessible F12 (its popup is inaccessible).
- `commlink_attempt` (0x558C60, fp_1int) — Simpler commlink, takes only target faction. Did NOT open diplomacy in testing.
- `diplo_lock` (0x539820, fp_1int) — Lock/initialize diplomacy state for a faction
- `diplo_unlock` (0x5398C0, fp_none) — Release diplomacy lock
- `diplomacy_caption` (0x5399A0, fp_2int) — Set dialog caption for faction pair
- `X_dialog(label, faction2)` — Show diplomacy popup with faction portrait (wrapper around X_pops)
- `pop_ask_number` (0x627C30, Fpop_ask_number) — Number input dialog (e.g., "Size of gift?")

### Player-Initiated Diplomacy (F12 Commlink) — SOLVED Pattern

**Problem:** The game's F12 handler (`commlink_attempter`) shows a visual faction selection popup that is completely inaccessible to screen readers. Five approaches were tried:

1. `communicate(player, fid, 0)` — Opened empty window, nothing navigable
2. `commlink_attempt(fid)` — Returned silently, no dialog opened
3. `commlink_attempter(player, fid)` — Opened game's own faction popup (inaccessible), not the diplomacy dialog
4. `communicate(player, fid, 1)` alone — Not tested standalone

**Working solution (attempt 5):**
```cpp
*diplo_second_faction = fid;      // Set target faction
diplo_lock(fid);                  // Initialize diplomacy state
diplomacy_caption(player, fid);   // Set dialog caption
communicate(player, fid, 1);      // Start diplomacy (1 = player-initiated)
```

All four calls are needed. `communicate` runs its own modal loop (like `social_select`), so the call blocks until diplomacy ends. The popup hooks (`sr_hook_popp`, `sr_hook_X_pops`, etc.) handle text announcements during the session automatically — all proposal menus, responses, and dialog text go through the standard popup system.

**Implementation:** F12 is intercepted in `gui.cpp` before the game sees it. Our `DiplomacyHandler::RunCommlink()` opens a modal PeekMessage loop with a navigable faction list (Up/Down, Enter to contact, Escape to cancel). On Enter, the above function sequence is called.

### Variable Substitution in Diplomacy Text

Game dialog text uses variables that must be resolved:

- `$WORD#` or `$WORD_WITH_UNDERSCORES#` — String from `ParseStrBuffer[slot]` (slots 0-9)
- `$NUM#` — Integer from `ParseNumTable[slot]` (slots 0-15)
- `$<N:form0:form1:form2:form3:form4:form5>` — Gender/plurality selection. Index = `ParseStrGender[N] + ParseStrPlurality[N] * 3`. German: 6 forms (3 genders x 2 numbers).
- `$<M1:$FACTIONADJ0>` / `$<F1:$FACTIONADJ0>` — Letter-prefixed patterns (planetfall only, handled by `sr_substitute_vars`)

The game populates slots via `parse_says(slot, text, gender, plural)` and `parse_num(slot, value)` before showing dialogs. Our `sr_substitute_game_vars()` reads from the same global arrays to resolve variables in screen reader text.

### AI-Initiated Diplomacy

When an AI faction contacts the player, the game calls `communicate(ai, player, 0)` internally. All dialog text goes through the popup hooks automatically — no special handling needed from our side. `DiplomacyHandler::OnTimer()` detects the session opening (`DiploWinState != 0`) and announces "Diplomacy with [faction name]".

### Diplomacy State Detection
- Check `*DiploWinState != 0` for active diplomacy
- `DiplomacyHandler::HandleKey()` provides S/Tab (relationship summary) and Ctrl+F1 (help) during active sessions
- Treaty flags: `DIPLO_PACT`, `DIPLO_TREATY`, `DIPLO_TRUCE`, `DIPLO_VENDETTA`, `DIPLO_COMMLINK`, `DIPLO_HAVE_SURRENDERED`, `DIPLO_HAVE_INFILTRATOR`
- `Factions[other].diplo_patience[player]` — AI patience (int8_t: <=0 thin, <=3 moderate, >3 patient)
