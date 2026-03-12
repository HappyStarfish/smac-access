# SMAC Popup Menu Internals (Reverse-Engineered)

How the game's popup menus work internally and how to replace them
with accessible alternatives. Based on terranx.exe (Alien Crossfire).

---

## Architecture Overview

The game uses a three-layer system for popup menus (TOPMENU, MAPMENU, etc.):

1. **Win Object Construction** — `Popup_start` (0x406380) / `BasePop_start` (0x601BF0)
2. **Menu Display + Input** — Menu runner function (0x4ADB20)
3. **Win Object Destruction** — Destructors (0x404900, 0x4064D0)

The caller allocates a Win object on the stack (~27KB / 0x6A2C bytes),
passes it through all three layers, then the function returns.

---

## Layer 1: Win Object Construction

### Popup_start (0x406380)

Wrapper that calls `BasePop_start` internally. Signature:

```c
int __thiscall Popup_start(
    Win* This,           // Stack-allocated, ~27KB
    const char* filename, // Script file ("SCRIPT.txt")
    const char* label,    // Menu label ("TOPMENU")
    int a4, int a5, int a6, int a7);
```

Calls `BasePop_start` at three internal addresses:
- 0x4063CB (normal path)
- 0x40649A (multiplayer path)
- 0x4064BE (fallback path)

All three are already patched by Thinker to `mod_BasePop_start`.

### BasePop_start (0x601BF0)

Does the actual construction:
- Allocates draw buffer (the bitmap the menu is rendered to)
- Initializes the Win object fields
- Parses the script file for menu items

**CRITICAL:** If you skip BasePop_start (early return), the draw buffer
is never allocated, but the destructor will still try to free it → crash
("Unable to allocate draw-buffer; terminating program").

### Win Object Structure

- Total size: ~0x6A2C bytes (~27KB)
- Offset +0x0000: Primary vtable pointer (set to 0x6695C8 before destruction)
- Offset +0x4444: Secondary vtable pointer (set to 0x6695C0 before destruction)
- Contains embedded sub-objects with their own vtables

---

## Layer 2: Menu Display (0x4ADB20)

This is the function that actually shows the menu and handles mouse input.
It blocks until the user makes a selection.

```c
int __thiscall MenuRunner(
    void* ContainerThis,  // Container object (different from Win!)
    void* WinObj,         // The constructed Win object
    int flag);            // 1 for TOPMENU, 0 for MAPMENU
```

**Return value:** 0-based index of selected item, or negative for cancel.

The return value is used by the caller as an index into a jump table
that dispatches to different handlers (start game, load game, etc.).

### Known Call Sites

| Address    | Menu           | Container offset | Win offset  | Flag |
|------------|----------------|------------------|-------------|------|
| 0x58E71D   | TOPMENU        | [ebp-0x16B0]     | [ebp-0x6A2C]| 1    |
| 0x58D74E   | MAPMENU        | [ebp-0x16AC]     | [ebp-0x6A28]| 0    |

### TOPMENU Caller Details (around 0x58E6D0)

```asm
; Choose label based on parameter
mov  edi, [ebp+0x8]        ; outer function arg1
mov  eax, 0x68f988          ; "HOTSEAT"
cmp  edi, ebx               ; ebx = 0
jne  short_skip
mov  eax, 0x68f990          ; "TOPMENU" (when arg1 == 0)

; Construct Win object
push ebx, ebx, ebx          ; 0, 0, 0
push 0xFFFFFFFF              ; -1
push eax                     ; label
push 0x9B8AA8                ; filename
lea  ecx, [ebp-0x6A2C]      ; this (Win on stack)
call 0x406380                ; Popup_start — return value IGNORED

; Show menu
lea  eax, [ebp-0x6A2C]      ; Win object
push 0x1                     ; flag
push eax
lea  ecx, [ebp-0x16B0]      ; container this
call 0x4ADB20                ; menu display → EAX = selected index
mov  esi, eax                ; save result

; Dispatch on result
cmp  esi, ebx                ; < 0?
jl   cleanup
cmp  esi, 0x6                ; == 6 (quit)?
je   cleanup
; ... jump table for items 0-5 at 0x58EE34
```

### TOPMENU Return Value Dispatch

The selected index maps to game actions via a jump table at 0x58EE34:
- 0: Start Game / Schnellstart
- 1: Spiel starten (custom)
- 2: Szenario
- 3: Spiel laden
- 4: Multiplayer (if applicable)
- 5: (varies)
- 6: Quit → cleanup path
- negative: cancel → cleanup path

### MAPMENU Return Value

Stored in global `[0x94B35C]`, dispatched via jump table at 0x58DB18
for items 0-4.

---

## Layer 3: Destruction

Both exit paths (normal and cancel) run the same cleanup:

```asm
; Reset vtables to base class (for safe virtual dispatch during destruction)
mov  DWORD PTR [ebp-0x6A2C], 0x6695C8    ; primary vtable
mov  DWORD PTR [ebp-0x65E8], 0x6695C0    ; secondary vtable (+0x4444)

; Destroy inner objects
lea  ecx, [ebp-0x6A2C]
call 0x404900                              ; sub-object destructor

; ... destroy other stack objects ...

; Destroy Win object
lea  ecx, [ebp-0x6A2C]
call 0x4064D0                              ; main Win destructor
```

**Key insight:** The destructors are NOT null-safe. They expect a fully
constructed Win object. Zeroing (memset) or skipping construction causes
crashes. The vtable pointers are explicitly reset before destruction,
so even if you change them, the destructor uses the correct vtable.

---

## Variant Menu Runner (0x4ADAF0) — SCENARIOMENU / MULTIMENU

SCENARIOMENU and MULTIMENU use a DIFFERENT display function (0x4ADAF0)
than TOPMENU/MAPMENU (0x4ADB20). This is 0x30 bytes before 0x4ADB20 —
same class, different method (wrapper with arg validation).

### Disassembly

```asm
; Signature: int __thiscall func(void* container, const char* label, int arg2, int arg3)
; Callee cleanup: ret 0xC (3 stack args = 12 bytes)
0x4ADAF0: push ebp              ; 55
          mov  ebp, esp          ; 8B EC
          mov  eax, [ebp+0xc]   ; 8B 45 0C  — arg2
          dec  eax               ; 48
          je   +ok               ; 74 xx     — if arg2 == 1 → continue
          or   eax, -1           ; 83 C8 FF  — else return -1
          pop  ebp               ; 5D
          ret  0xc               ; C2 0C 00
ok:       mov  eax, [ebp+0x10]  ; arg3
          mov  edx, [ebp+0x8]   ; arg1 (label string)
          push eax
          push edx
          call 0x4ADB70          ; internal menu function (NOT 0x4ADB20!)
          pop  ebp
          ret  0xc
```

Key observations:
- Only proceeds when arg2 == 1 (otherwise returns -1)
- Calls 0x4ADB70 internally (different from 0x4ADB20 used by TOPMENU)
- Prologue: 6 bytes (`55 8B EC 8B 45 0C`) — 3 complete instructions

### SCENARIOMENU Call Site (0x58E80D)

```asm
push ebx          ; 0 (arg3)
push edi          ; 1 (arg2)
push 0x68F998     ; "SCENARIOMENU" (label)
lea  ecx, [ebp-0x16b0]  ; container this
call 0x4ADAF0
```

### MULTIMENU Call Site

Same pattern, different label string ("MULTIMENU"). Both are dispatched
from the TOPMENU caller function when the user selects item 2 (Scenario)
or item 4 (Multiplayer).

### Hook Implementation (2026-03-12)

**Approach:** Global inline hook via `write_jump(0x4ADAF0, mod_menu_variant_runner)`.
Unlike TOPMENU/MAPMENU (which use `write_call` at specific sites), this hooks
the function globally because both SCENARIOMENU and MULTIMENU go through it.

**Trampoline:** 6 bytes copied from original prologue + JMP to 0x4ADAF6.
Allocated via `VirtualAlloc(PAGE_EXECUTE_READWRITE)` before `write_jump`
overwrites the prologue. Called from `sr_menu_variant_init_trampoline()`.

**Menu items:** Parsed from SCRIPT.TXT sections (`#SCENARIOMENU`, `#MULTIMENU`)
via `sr_popup_list_parse("SCRIPT", label)`.

**Return value:** 0-based index (same as 0x4ADB20), or -1 for cancel.

**Files:** gui.cpp (hook + trampoline), gui.h (declarations), patch.cpp (installation)

---

## How to Hook: The Correct Pattern

### What DOESN'T work

1. **Early return from BasePop_start** — Popup_start continues with
   uninitialized object, then destructor crashes
2. **memset(This, 0, size)** — Destructor is not null-safe, crashes
3. **Skipping Popup_start entirely** — Caller has explicit destructor
   calls that still run on the stack object

### What DOES work

**Hook the menu display function (Layer 2), not the constructor (Layer 1).**

1. Let `Popup_start`/`BasePop_start` run normally → Win object fully constructed
2. Use `write_call` at the specific call site to redirect the 0x4ADB20 call
3. In your hook: show accessible modal, return selected index
4. Destructors run normally on the properly constructed object

```c
// In mod_BasePop_start: set a flag when startup menu detected
if (sr_is_startup_menu(label)) {
    sr_popup_list_parse(filename, label);  // parse menu items
    sr_startup_menu_pending = true;        // flag for display hook
}
// Let BasePop_start run normally — DON'T early return

// Separate hook for the display call:
int __thiscall mod_startup_menu_runner(void* Container, void* Win, int flag) {
    if (sr_startup_menu_pending) {
        sr_startup_menu_pending = false;
        return sr_accessible_menu_modal(title, items, count);
    }
    return OrigMenuRunner(Container, Win, flag);  // fallback
}

// In patch_setup():
write_call(0x58E71D, (int)mod_startup_menu_runner);  // TOPMENU
write_call(0x58D74E, (int)mod_startup_menu_runner);  // MAPMENU

// For SCENARIOMENU/MULTIMENU (0x4ADAF0 path):
sr_menu_variant_init_trampoline();  // save prologue before overwriting
write_jump(0x4ADAF0, (int)mod_menu_variant_runner);
```

### Why This Works

- The Win object is fully constructed → destructor is safe
- The menu display function is the blocking point → replacing it
  gives us full control over input
- `write_call` at specific sites means we only affect the menus
  we want, not every call to 0x4ADB20
- For 0x4ADAF0: `write_jump` is safe because all callers pass
  through the same arg2==1 validation, and our hook checks
  `sr_is_startup_menu(label)` before intercepting

---

## String Addresses in terranx.exe

| Address    | String         | Section |
|------------|----------------|---------|
| 0x68F990   | TOPMENU        | .data   |
| 0x68F988   | HOTSEAT        | .data   |
| 0x68F8A0   | MAPMENU        | .data   |
| 0x68F998   | SCENARIOMENU   | .data   |
| 0x9B8AA8   | Script filename| .data   |

MULTIMENU was NOT found in terranx.exe — may be mod-added or
dynamically constructed.

---

## Related Functions

| Address    | Function               | Notes                          |
|------------|------------------------|--------------------------------|
| 0x406380   | Popup_start            | Win constructor wrapper         |
| 0x601BF0   | BasePop_start          | Actual Win construction         |
| 0x4ADB20   | Menu runner            | Shows menu, returns selection   |
| 0x4ADAF0   | Menu runner variant    | Used by SCENARIOMENU/MULTIMENU (hooked) |
| 0x4ADB70   | Internal menu display  | Called by 0x4ADAF0 internally   |
| 0x404900   | Sub-object destructor  | Called before main destructor   |
| 0x4064D0   | Win destructor         | Main cleanup, frees draw buffer |
| 0x5D4DD0   | GraphicWin destructor  | Referenced in earlier analysis  |

## Global State

| Address    | Variable       | Notes                          |
|------------|----------------|--------------------------------|
| 0x94B360   | TOPMENU state  | Last selected TOPMENU item     |
| 0x94B35C   | MAPMENU state  | Last selected MAPMENU item     |
| 0x94B464   | AlphaIniPref   | Array of INI preferences       |
| 0x9B2074   | Game loaded flag | Set by load_daemon, checked by TOPMENU caller |
| 0x9B2070   | Faction/player   | Copied from 0x939284 after load_game returns 0 |
| 0x9B2068   | Unknown flag     | Checked by TOPMENU loop; if != 0, exits loop  |

---

## TOPMENU load_game Dispatch (0x58E796)

When the user selects "Load Game" (index 3), the TOPMENU caller calls
`load_game(0, 0)` at 0x58E7A1. The return value controls the next step:

```asm
call   load_game           ; 0x58E7A1
mov    esi, eax            ; save return value
cmp    esi, ebx            ; compare with 0
jne    0x58e77a            ; != 0 → BACK TO MENU LOOP ✓

; return == 0:
mov    eax, [0x9B2074]     ; "game loaded" flag
cmp    eax, ebx
je     0x58e772            ; flag == 0 → EXIT FUNCTION (quit!)

; flag != 0 → init game and start playing
call   0x50f440            ; init
...
```

At 0x58E772 (shared with new-game code):
```asm
cmp    esi, ebx            ; esi still 0 from load_game
je     0x58eb18            ; → EXIT caller function entirely
```

**Three-way dispatch:**
- Return != 0 → back to menu (user cancelled, wants to try again)
- Return == 0, flag != 0 → game loaded, start playing
- Return == 0, flag == 0 → exit menu function (quit to desktop)

The second call site at 0x58E879 (scenario/multiplayer load) has the
same pattern.

**Lesson learned (2026-03-12):** Our hook originally returned 0 on cancel,
which the caller interpreted as "quit." Crashed because no valid game state
existed. Fix: return -1 (non-zero) on cancel → caller loops back to menu.

---

## Rule: Always Disassemble the Caller

When replacing a game function with an inline hook, the return value
semantics are defined by the CALLER, not by what seems logical.

**The mistake pattern:**
1. Hook function X
2. Assume return 0 = "nothing happened"
3. Crash or wrong behavior because the caller interprets 0 differently

**The correct process:**
1. Find all call sites: search for `E8` (call) instructions targeting the function
2. Disassemble 10-20 instructions AFTER each call site
3. Trace what happens for each possible return value
4. Match your hook's return values to the caller's expected semantics

This applies to ALL inline hooks that replace blocking game functions
(load_game, save_game, popup menus, council votes, etc.).
