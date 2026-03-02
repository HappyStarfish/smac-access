/*
 * MonumentHandler — F9 Monuments accessible screen.
 *
 * Hooks all 16 mon_* functions to track achievements per faction.
 * F9 opens a modal screen listing which achievements are unlocked.
 */

#include "monument_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "gui.h"
#include "faction.h"
#include "patch.h"

// =========================================================================
// Monument hooks — track achievements via trampoline hooks on mon_* funcs
// =========================================================================

static const int MON_TYPE_COUNT = 16;

// Turn achieved per faction per type (0 = not achieved)
static int _monTurn[8][MON_TYPE_COUNT];

// Trampoline memory (executable) — each trampoline: 9 bytes prologue + 5 byte JMP
static unsigned char* _trampMem = NULL;
static const int TRAMP_SIZE = 16;
static const int PROLOGUE_SIZE = 9; // push ebp; mov ebp,esp; push ecx; mov eax,[imm32]

// Addresses of the 16 mon_* functions
static const DWORD MON_ADDRS[MON_TYPE_COUNT] = {
    0x476B70, // 0: colony_founded
    0x476C90, // 1: tech_discovered
    0x476DA0, // 2: secrets_of_tech
    0x476EE0, // 3: prototype_built
    0x476FE0, // 4: facility_built
    0x477100, // 5: secret_project
    0x477210, // 6: enemy_destroyed
    0x477320, // 7: conquer_base
    0x477440, // 8: naval_unit_built
    0x477540, // 9: air_unit_built
    0x477640, // 10: native_life_bred
    0x477740, // 11: first_in_space
    0x477840, // 12: built_preserve
    0x477940, // 13: winning_unify
    0x4779D0, // 14: winning_trans
    0x477A60, // 15: killed_faction
};

// Hook functions — record achievement then call original via trampoline.
// All use (int, int) signature; __cdecl means extra args are harmless.
#define DEF_MON_HOOK(n) \
static int __cdecl hook_mon_##n(int faction, int detail) { \
    if (faction >= 0 && faction < 8 && _monTurn[faction][n] == 0) { \
        _monTurn[faction][n] = *CurrentTurn > 0 ? *CurrentTurn : 1; \
        sr_debug_log("MONUMENT: type %d achieved by faction %d turn %d", \
                     n, faction, _monTurn[faction][n]); \
    } \
    typedef int (__cdecl *FT)(int, int); \
    FT tramp = (FT)(_trampMem + n * TRAMP_SIZE); \
    return tramp(faction, detail); \
}

DEF_MON_HOOK(0)  DEF_MON_HOOK(1)  DEF_MON_HOOK(2)  DEF_MON_HOOK(3)
DEF_MON_HOOK(4)  DEF_MON_HOOK(5)  DEF_MON_HOOK(6)  DEF_MON_HOOK(7)
DEF_MON_HOOK(8)  DEF_MON_HOOK(9)  DEF_MON_HOOK(10) DEF_MON_HOOK(11)
DEF_MON_HOOK(12) DEF_MON_HOOK(13) DEF_MON_HOOK(14) DEF_MON_HOOK(15)

typedef int (__cdecl *fp_hook)(int, int);
static const fp_hook HOOK_FUNCS[MON_TYPE_COUNT] = {
    hook_mon_0,  hook_mon_1,  hook_mon_2,  hook_mon_3,
    hook_mon_4,  hook_mon_5,  hook_mon_6,  hook_mon_7,
    hook_mon_8,  hook_mon_9,  hook_mon_10, hook_mon_11,
    hook_mon_12, hook_mon_13, hook_mon_14, hook_mon_15,
};

/// Patch a function with a JMP to our hook (handles VirtualProtect directly).
static void patch_jmp(DWORD addr, const void* target) {
    DWORD old_protect;
    VirtualProtect((void*)addr, 5, PAGE_EXECUTE_READWRITE, &old_protect);
    *(unsigned char*)addr = 0xE9;
    *(int32_t*)(addr + 1) = (int32_t)target - (int32_t)addr - 5;
    VirtualProtect((void*)addr, 5, old_protect, &old_protect);
}

// =========================================================================
// Modal handler state
// =========================================================================

static bool _active = false;
static bool _wantClose = false;
static int _currentIndex = 0;

static int _turnCache[MON_TYPE_COUNT]; // cached for current faction during modal

static void ReadMonumentData() {
    int owner = MapWin->cOwner;
    if (owner < 0 || owner >= 8) return;
    for (int i = 0; i < MON_TYPE_COUNT; i++) {
        _turnCache[i] = _monTurn[owner][i];
    }
}

static void AnnounceItem(int index, bool interrupt = true) {
    if (index < 0 || index >= MON_TYPE_COUNT) return;
    char buf[512];
    SrStr type_str = (SrStr)(SR_MONUMENT_TYPE_0 + index);
    if (_turnCache[index] > 0) {
        snprintf(buf, sizeof(buf), loc(SR_MONUMENT_ITEM_ACHIEVED),
                 index + 1, MON_TYPE_COUNT,
                 loc(type_str), _turnCache[index]);
    } else {
        snprintf(buf, sizeof(buf), loc(SR_MONUMENT_ITEM_LOCKED),
                 index + 1, MON_TYPE_COUNT,
                 loc(type_str));
    }
    sr_output(buf, interrupt);
}

static void AnnounceSummary() {
    int count = 0;
    for (int i = 0; i < MON_TYPE_COUNT; i++) {
        if (_turnCache[i] > 0) count++;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_MONUMENT_SUMMARY),
             count, MON_TYPE_COUNT);
    sr_output(buf, true);
}

// =========================================================================
// Public API
// =========================================================================

namespace MonumentHandler {

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return true;

    switch (wParam) {
    case VK_UP:
        _currentIndex = (_currentIndex > 0) ? _currentIndex - 1 : MON_TYPE_COUNT - 1;
        AnnounceItem(_currentIndex);
        return true;

    case VK_DOWN:
        _currentIndex = (_currentIndex < MON_TYPE_COUNT - 1) ? _currentIndex + 1 : 0;
        AnnounceItem(_currentIndex);
        return true;

    case VK_HOME:
        _currentIndex = 0;
        AnnounceItem(_currentIndex);
        return true;

    case VK_END:
        _currentIndex = MON_TYPE_COUNT - 1;
        AnnounceItem(_currentIndex);
        return true;

    case VK_ESCAPE:
        _wantClose = true;
        return true;

    case VK_F1:
        if (ctrl_key_down()) {
            sr_output(loc(SR_MONUMENT_HELP), true);
        }
        return true;

    default:
        break;
    }

    if (wParam == 'S' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceSummary();
        return true;
    }

    return true;
}

void RunModal() {
    ReadMonumentData();
    _active = true;
    _wantClose = false;
    _currentIndex = 0;

    int count = 0;
    for (int i = 0; i < MON_TYPE_COUNT; i++) {
        if (_turnCache[i] > 0) count++;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_MONUMENT_OPEN), count, MON_TYPE_COUNT);
    sr_output(buf, true);

    AnnounceItem(0, false);

    sr_run_modal_pump(&_wantClose);

    _active = false;
    sr_output(loc(SR_MONUMENT_CLOSED), true);
}

void InstallHooks() {
    memset(_monTurn, 0, sizeof(_monTurn));

    // Allocate executable memory for trampolines
    _trampMem = (unsigned char*)VirtualAlloc(NULL,
        MON_TYPE_COUNT * TRAMP_SIZE,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!_trampMem) {
        debug("MONUMENT: Failed to allocate trampoline memory\n");
        return;
    }

    for (int i = 0; i < MON_TYPE_COUNT; i++) {
        unsigned char* orig = (unsigned char*)MON_ADDRS[i];
        unsigned char* tramp = _trampMem + i * TRAMP_SIZE;

        // Copy first 9 bytes of original function prologue to trampoline
        // All mon_* functions start with: push ebp; mov ebp,esp; push ecx; mov eax,[imm32]
        memcpy(tramp, orig, PROLOGUE_SIZE);

        // Add JMP from trampoline to original+9 (rest of function)
        tramp[PROLOGUE_SIZE] = 0xE9;
        int32_t jmp_rel = (int32_t)(orig + PROLOGUE_SIZE)
                        - (int32_t)(tramp + PROLOGUE_SIZE + 5);
        memcpy(tramp + PROLOGUE_SIZE + 1, &jmp_rel, 4);

        // Patch original function with JMP to our hook
        patch_jmp(MON_ADDRS[i], (void*)HOOK_FUNCS[i]);
    }

    debug("MONUMENT: %d hooks installed\n", MON_TYPE_COUNT);
}

} // namespace MonumentHandler
