/*
 * Design Workshop accessibility handler.
 * Replaces the game's Design Workshop with a keyboard-navigable, screen-reader
 * accessible modal loop. Two levels: prototype list and component editor.
 *
 * Level 1: Browse/manage faction prototypes (Up/Down/Enter/N/Delete/Escape)
 * Level 2: Edit prototype components (Left/Right categories, Up/Down options)
 */

#include "design_handler.h"
#include "engine.h"
#include "gui.h"
#include "modal_utils.h"
#include "screen_reader.h"
#include "localization.h"
#include "tech.h"
#include "veh.h"

namespace DesignHandler {

static bool _active = false;
static bool _wantClose = false;

// Level 1: Prototype list
static int _protoList[MaxProtoNum];  // unit_ids of visible prototypes
static int _protoCount = 0;
static int _protoIndex = 0;

// Level 2: Component editor
static bool _editing = false;
static int _editUnitId = -1;         // unit_id being edited
static bool _editIsNew = false;      // true if creating new unit

// Editor state: 6 categories
enum EditCategory {
    ECAT_CHASSIS = 0,
    ECAT_WEAPON,
    ECAT_ARMOR,
    ECAT_REACTOR,
    ECAT_ABILITY1,
    ECAT_ABILITY2,
    ECAT_COUNT
};
static int _editCat = 0;

// Current component selections (indices into filtered lists)
static int _selChassis = 0;
static int _selWeapon = 0;
static int _selArmor = 0;
static int _selReactor = 0;
static int _selAbility1 = -1;  // -1 = none
static int _selAbility2 = -1;  // -1 = none

// Filtered available component lists
static int _availChassis[MaxChassisNum];
static int _availChassisCount = 0;
static int _availWeapon[MaxWeaponNum];
static int _availWeaponCount = 0;
static int _availArmor[MaxArmorNum];
static int _availArmorCount = 0;
static int _availReactor[MaxReactorNum];
static int _availReactorCount = 0;
static int _availAbility[MaxAbilityNum];
static int _availAbilityCount = 0;

// Delete confirmation state
static int _deleteConfirmId = -1;

static int get_faction_id() {
    return *CurrentPlayerFaction;
}

// Check if a component's tech is researched by the player
static bool tech_available(int preq_tech) {
    if (preq_tech == TECH_None) return true;
    if (preq_tech == TECH_Disable) return false;
    return has_tech(preq_tech, get_faction_id()) != 0;
}

// Check if an ability is compatible with a given chassis triad and weapon mode
static bool ability_compatible(int abl_idx, int chassis_id, int weapon_id) {
    int flags = Ability[abl_idx].flags;
    int triad = Chassis[chassis_id].triad;
    int wmode = Weapon[weapon_id].mode;
    bool is_combat = (wmode <= WMODE_MISSILE);
    bool is_terraform = (wmode == WMODE_TERRAFORM);
    bool is_probe = (wmode == WMODE_PROBE);
    bool is_psi = (Weapon[weapon_id].offense_value < 0);
    bool is_fast = (Chassis[chassis_id].speed > 1);

    // Triad checks
    if (triad == TRIAD_LAND && !(flags & AFLAG_ALLOWED_LAND_UNIT)) return false;
    if (triad == TRIAD_SEA && !(flags & AFLAG_ALLOWED_SEA_UNIT)) return false;
    if (triad == TRIAD_AIR && !(flags & AFLAG_ALLOWED_AIR_UNIT)) return false;

    // Combat/non-combat
    if (is_combat && !(flags & AFLAG_ALLOWED_COMBAT_UNIT)) return false;
    if (is_terraform && !(flags & AFLAG_ALLOWED_TERRAFORM_UNIT)) return false;
    if (!is_combat && !is_terraform && !(flags & AFLAG_ALLOWED_NONCOMBAT_UNIT)) return false;

    // Exclusions
    if (is_probe && (flags & AFLAG_NOT_ALLOWED_PROBE_TEAM)) return false;
    if (is_psi && (flags & AFLAG_NOT_ALLOWED_PSI_UNIT)) return false;
    if (is_fast && (flags & AFLAG_NOT_ALLOWED_FAST_UNIT)) return false;

    // Transport-only
    if ((flags & AFLAG_TRANSPORT_ONLY_UNIT) && wmode != WMODE_TRANSPORT) return false;

    // Probe-only
    if ((flags & AFLAG_ONLY_PROBE_TEAM) && !is_probe) return false;

    return true;
}

// Build filtered lists of available components
static void build_available_lists() {
    int fid = get_faction_id();

    _availChassisCount = 0;
    for (int i = 0; i < MaxChassisNum; i++) {
        if (tech_available(Chassis[i].preq_tech)) {
            _availChassis[_availChassisCount++] = i;
        }
    }

    _availWeaponCount = 0;
    for (int i = 0; i < MaxWeaponNum; i++) {
        if (tech_available(Weapon[i].preq_tech)) {
            _availWeapon[_availWeaponCount++] = i;
        }
    }

    _availArmorCount = 0;
    for (int i = 0; i < MaxArmorNum; i++) {
        if (tech_available(Armor[i].preq_tech)) {
            _availArmor[_availArmorCount++] = i;
        }
    }

    _availReactorCount = 0;
    for (int i = 0; i < MaxReactorNum; i++) {
        if (tech_available(Reactor[i].preq_tech)) {
            _availReactor[_availReactorCount++] = i;
        }
    }

    // Abilities are filtered per-chassis/weapon in announce, but build
    // the base tech-available list here
    _availAbilityCount = 0;
    for (int i = 0; i < MaxAbilityNum; i++) {
        if (tech_available(Ability[i].preq_tech)) {
            _availAbility[_availAbilityCount++] = i;
        }
    }
}

// Build filtered ability list for current chassis+weapon selection
static int _filteredAbility[MaxAbilityNum];
static int _filteredAbilityCount = 0;

static void build_filtered_abilities() {
    int chs = _availChassis[_selChassis];
    int wpn = _availWeapon[_selWeapon];
    _filteredAbilityCount = 0;
    for (int i = 0; i < _availAbilityCount; i++) {
        int abl_idx = _availAbility[i];
        if (ability_compatible(abl_idx, chs, wpn)) {
            _filteredAbility[_filteredAbilityCount++] = abl_idx;
        }
    }
}

// Get the VehAblFlag for current ability selections
static VehAblFlag get_selected_abilities() {
    uint32_t flags = ABL_NONE;
    if (_selAbility1 >= 0 && _selAbility1 < _filteredAbilityCount) {
        flags |= (1u << _filteredAbility[_selAbility1]);
    }
    if (_selAbility2 >= 0 && _selAbility2 < _filteredAbilityCount) {
        int abl2 = _filteredAbility[_selAbility2];
        if (_selAbility1 < 0 || abl2 != _filteredAbility[_selAbility1]) {
            flags |= (1u << abl2);
        }
    }
    return (VehAblFlag)flags;
}

// Get current cost based on editor selections
static int get_edit_cost() {
    if (_availChassisCount == 0 || _availWeaponCount == 0
        || _availArmorCount == 0 || _availReactorCount == 0) return 0;
    return mod_proto_cost(
        (VehChassis)_availChassis[_selChassis],
        (VehWeapon)_availWeapon[_selWeapon],
        (VehArmor)_availArmor[_selArmor],
        get_selected_abilities(),
        (VehReactor)_availReactor[_selReactor]);
}

// Build the prototype list for the current faction
static void build_proto_list() {
    int fid = get_faction_id();
    _protoCount = 0;

    // Default units (0..MaxProtoFactionNum-1) that player has tech for
    for (int i = 0; i < MaxProtoFactionNum; i++) {
        if (Units[i].is_active() && tech_available(Units[i].preq_tech)
            && !(Units[i].obsolete_factions & (1 << fid))) {
            _protoList[_protoCount++] = i;
        }
    }

    // Faction's custom units (faction_id * MaxProtoFactionNum .. +63)
    int base = fid * MaxProtoFactionNum;
    for (int i = base; i < base + MaxProtoFactionNum; i++) {
        if (Units[i].is_active()) {
            _protoList[_protoCount++] = i;
        }
    }
}

// Find an empty prototype slot for the faction, returns unit_id or -1
static int find_empty_slot() {
    int fid = get_faction_id();
    int base = fid * MaxProtoFactionNum;
    for (int i = base; i < base + MaxProtoFactionNum; i++) {
        if (!Units[i].is_active()) {
            return i;
        }
    }
    return -1;
}

// Announce a prototype in the list (Level 1)
static void announce_proto() {
    if (_protoCount == 0) return;
    int uid = _protoList[_protoIndex];
    UNIT* u = &Units[uid];

    int atk = Weapon[u->weapon_id].offense_value;
    int def = Armor[u->armor_id].defense_value;
    int spd = Chassis[u->chassis_id].speed;
    int cost = u->cost;

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_DESIGN_PROTO_FMT),
        _protoIndex + 1, _protoCount,
        sr_game_str(u->name), atk, def, spd, cost);
    sr_output(buf, true);
}

// Announce the current editor category and selection (Level 2)
static void announce_editor_item() {
    char buf[512];

    switch (_editCat) {
    case ECAT_CHASSIS: {
        if (_availChassisCount == 0) break;
        int chs = _availChassis[_selChassis];
        const char* triad_str;
        switch (Chassis[chs].triad) {
            case TRIAD_LAND: triad_str = loc(SR_DESIGN_TRIAD_LAND); break;
            case TRIAD_SEA:  triad_str = loc(SR_DESIGN_TRIAD_SEA); break;
            case TRIAD_AIR:  triad_str = loc(SR_DESIGN_TRIAD_AIR); break;
            default: triad_str = "?"; break;
        }
        char detail[256];
        snprintf(detail, sizeof(detail), loc(SR_DESIGN_CHASSIS_FMT),
            sr_game_str(Chassis[chs].offsv1_name),
            (int)Chassis[chs].speed, triad_str);
        snprintf(buf, sizeof(buf), loc(SR_DESIGN_CATEGORY),
            loc(SR_DESIGN_CAT_CHASSIS), detail);
        break;
    }
    case ECAT_WEAPON: {
        if (_availWeaponCount == 0) break;
        int wpn = _availWeapon[_selWeapon];
        char detail[256];
        if (Weapon[wpn].mode <= WMODE_MISSILE) {
            snprintf(detail, sizeof(detail), loc(SR_DESIGN_WEAPON_FMT),
                sr_game_str(Weapon[wpn].name), (int)Weapon[wpn].offense_value);
        } else {
            snprintf(detail, sizeof(detail), loc(SR_DESIGN_EQUIPMENT),
                sr_game_str(Weapon[wpn].name));
        }
        snprintf(buf, sizeof(buf), loc(SR_DESIGN_CATEGORY),
            loc(SR_DESIGN_CAT_WEAPON), detail);
        break;
    }
    case ECAT_ARMOR: {
        if (_availArmorCount == 0) break;
        int arm = _availArmor[_selArmor];
        char detail[256];
        snprintf(detail, sizeof(detail), loc(SR_DESIGN_ARMOR_FMT),
            sr_game_str(Armor[arm].name), (int)Armor[arm].defense_value);
        snprintf(buf, sizeof(buf), loc(SR_DESIGN_CATEGORY),
            loc(SR_DESIGN_CAT_ARMOR), detail);
        break;
    }
    case ECAT_REACTOR: {
        if (_availReactorCount == 0) break;
        int rec = _availReactor[_selReactor];
        char detail[256];
        snprintf(detail, sizeof(detail), loc(SR_DESIGN_REACTOR_FMT),
            sr_game_str(Reactor[rec].name), (int)Reactor[rec].power);
        snprintf(buf, sizeof(buf), loc(SR_DESIGN_CATEGORY),
            loc(SR_DESIGN_CAT_REACTOR), detail);
        break;
    }
    case ECAT_ABILITY1:
    case ECAT_ABILITY2: {
        int abl_num = (_editCat == ECAT_ABILITY1) ? 1 : 2;
        int sel = (_editCat == ECAT_ABILITY1) ? _selAbility1 : _selAbility2;
        char detail[256];
        if (sel < 0 || _filteredAbilityCount == 0) {
            snprintf(detail, sizeof(detail), loc(SR_DESIGN_ABILITY_NONE));
        } else {
            int abl_idx = _filteredAbility[sel];
            snprintf(detail, sizeof(detail), "%s",
                sr_game_str(Ability[abl_idx].name));
        }
        char abl_label[64];
        snprintf(abl_label, sizeof(abl_label), loc(SR_DESIGN_ABILITY_FMT),
            abl_num, detail);
        snprintf(buf, sizeof(buf), "%s", abl_label);
        break;
    }
    default:
        return;
    }

    // Append total cost
    char cost_str[64];
    snprintf(cost_str, sizeof(cost_str), ". %s", loc(SR_DESIGN_COST));
    char cost_buf[64];
    snprintf(cost_buf, sizeof(cost_buf), cost_str, get_edit_cost());
    strncat(buf, cost_buf, sizeof(buf) - strlen(buf) - 1);

    sr_output(buf, true);
}

// Find the index of a value in a filtered list, returns 0 if not found
static int find_in_list(int* list, int count, int val) {
    for (int i = 0; i < count; i++) {
        if (list[i] == val) return i;
    }
    return 0;
}

// Initialize editor from an existing unit
static void init_editor_from_unit(int unit_id) {
    UNIT* u = &Units[unit_id];
    build_available_lists();

    _selChassis = find_in_list(_availChassis, _availChassisCount, u->chassis_id);
    _selWeapon = find_in_list(_availWeapon, _availWeaponCount, u->weapon_id);
    _selArmor = find_in_list(_availArmor, _availArmorCount, u->armor_id);
    _selReactor = find_in_list(_availReactor, _availReactorCount, u->reactor_id);

    build_filtered_abilities();

    // Map ability flags to selections
    _selAbility1 = -1;
    _selAbility2 = -1;
    int found = 0;
    for (int i = 0; i < _filteredAbilityCount && found < 2; i++) {
        if (u->ability_flags & (1u << _filteredAbility[i])) {
            if (found == 0) _selAbility1 = i;
            else _selAbility2 = i;
            found++;
        }
    }

    _editCat = ECAT_CHASSIS;
}

// Initialize editor for a new empty unit
static void init_editor_new() {
    build_available_lists();

    _selChassis = 0;
    _selWeapon = 0;
    _selArmor = 0;
    _selReactor = 0;
    _selAbility1 = -1;
    _selAbility2 = -1;

    build_filtered_abilities();
    _editCat = ECAT_CHASSIS;
}

// Save the edited design
static bool save_design() {
    if (_editUnitId < 0) return false;
    if (_availChassisCount == 0 || _availWeaponCount == 0
        || _availArmorCount == 0 || _availReactorCount == 0) return false;

    VehChassis chs = (VehChassis)_availChassis[_selChassis];
    VehWeapon wpn = (VehWeapon)_availWeapon[_selWeapon];
    VehArmor arm = (VehArmor)_availArmor[_selArmor];
    VehReactor rec = (VehReactor)_availReactor[_selReactor];
    VehAblFlag abls = get_selected_abilities();

    mod_make_proto(_editUnitId, chs, wpn, arm, abls, rec);

    char name[256] = {};
    mod_name_proto(name, _editUnitId, get_faction_id(), chs, wpn, arm, abls, rec);

    Units[_editUnitId].unit_flags |= UNIT_PROTOTYPED;
    Units[_editUnitId].unit_flags &= ~UNIT_CUSTOM_NAME_SET;

    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_DESIGN_SAVED), sr_game_str(Units[_editUnitId].name));
    sr_output(buf, true);

    sr_debug_log("DesignHandler: saved unit_id=%d name=%s\n", _editUnitId, Units[_editUnitId].name);
    return true;
}

// Handle key in Level 2 (editor)
static bool handle_editor_key(WPARAM wParam) {
    bool ctrl = ctrl_key_down();

    switch (wParam) {
    case VK_LEFT:
        if (ctrl) return false;
        _editCat = (_editCat + ECAT_COUNT - 1) % ECAT_COUNT;
        announce_editor_item();
        return true;

    case VK_RIGHT:
        if (ctrl) return false;
        _editCat = (_editCat + 1) % ECAT_COUNT;
        announce_editor_item();
        return true;

    case VK_UP:
        if (ctrl) return false;
        switch (_editCat) {
        case ECAT_CHASSIS:
            if (_availChassisCount > 0)
                _selChassis = (_selChassis + _availChassisCount - 1) % _availChassisCount;
            build_filtered_abilities();
            // Reset abilities if they became incompatible
            if (_selAbility1 >= _filteredAbilityCount) _selAbility1 = -1;
            if (_selAbility2 >= _filteredAbilityCount) _selAbility2 = -1;
            break;
        case ECAT_WEAPON:
            if (_availWeaponCount > 0)
                _selWeapon = (_selWeapon + _availWeaponCount - 1) % _availWeaponCount;
            build_filtered_abilities();
            if (_selAbility1 >= _filteredAbilityCount) _selAbility1 = -1;
            if (_selAbility2 >= _filteredAbilityCount) _selAbility2 = -1;
            break;
        case ECAT_ARMOR:
            if (_availArmorCount > 0)
                _selArmor = (_selArmor + _availArmorCount - 1) % _availArmorCount;
            break;
        case ECAT_REACTOR:
            if (_availReactorCount > 0)
                _selReactor = (_selReactor + _availReactorCount - 1) % _availReactorCount;
            break;
        case ECAT_ABILITY1:
            if (_filteredAbilityCount == 0) { _selAbility1 = -1; break; }
            if (_selAbility1 < 0) _selAbility1 = _filteredAbilityCount - 1;
            else if (_selAbility1 == 0) _selAbility1 = -1;
            else _selAbility1--;
            break;
        case ECAT_ABILITY2:
            if (_filteredAbilityCount == 0) { _selAbility2 = -1; break; }
            if (_selAbility2 < 0) _selAbility2 = _filteredAbilityCount - 1;
            else if (_selAbility2 == 0) _selAbility2 = -1;
            else _selAbility2--;
            break;
        }
        announce_editor_item();
        return true;

    case VK_DOWN:
        if (ctrl) return false;
        switch (_editCat) {
        case ECAT_CHASSIS:
            if (_availChassisCount > 0)
                _selChassis = (_selChassis + 1) % _availChassisCount;
            build_filtered_abilities();
            if (_selAbility1 >= _filteredAbilityCount) _selAbility1 = -1;
            if (_selAbility2 >= _filteredAbilityCount) _selAbility2 = -1;
            break;
        case ECAT_WEAPON:
            if (_availWeaponCount > 0)
                _selWeapon = (_selWeapon + 1) % _availWeaponCount;
            build_filtered_abilities();
            if (_selAbility1 >= _filteredAbilityCount) _selAbility1 = -1;
            if (_selAbility2 >= _filteredAbilityCount) _selAbility2 = -1;
            break;
        case ECAT_ARMOR:
            if (_availArmorCount > 0)
                _selArmor = (_selArmor + 1) % _availArmorCount;
            break;
        case ECAT_REACTOR:
            if (_availReactorCount > 0)
                _selReactor = (_selReactor + 1) % _availReactorCount;
            break;
        case ECAT_ABILITY1:
            if (_filteredAbilityCount == 0) { _selAbility1 = -1; break; }
            if (_selAbility1 < 0) _selAbility1 = 0;
            else if (_selAbility1 >= _filteredAbilityCount - 1) _selAbility1 = -1;
            else _selAbility1++;
            break;
        case ECAT_ABILITY2:
            if (_filteredAbilityCount == 0) { _selAbility2 = -1; break; }
            if (_selAbility2 < 0) _selAbility2 = 0;
            else if (_selAbility2 >= _filteredAbilityCount - 1) _selAbility2 = -1;
            else _selAbility2++;
            break;
        }
        announce_editor_item();
        return true;

    case VK_RETURN:
        // Save design and return to list
        if (save_design()) {
            _editing = false;
            build_proto_list();
            // Find the saved unit in the list
            for (int i = 0; i < _protoCount; i++) {
                if (_protoList[i] == _editUnitId) {
                    _protoIndex = i;
                    break;
                }
            }
        }
        return true;

    case VK_ESCAPE:
        // Cancel editing, return to list
        _editing = false;
        sr_output(loc(SR_DESIGN_CANCELLED), true);
        return true;

    case 'I':
        if (ctrl) {
            announce_editor_item();
            return true;
        }
        return false;

    case VK_F1:
        if (ctrl) {
            sr_output(loc(SR_DESIGN_EDIT_HELP), true);
            return true;
        }
        return false;

    case 'S':
        if (!ctrl) {
            // Summary: announce all current selections + cost
            char buf[1024];
            int pos = 0;
            if (_availChassisCount > 0 && _availWeaponCount > 0
                && _availArmorCount > 0 && _availReactorCount > 0) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s, ",
                    sr_game_str(Chassis[_availChassis[_selChassis]].offsv1_name));
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s, ",
                    sr_game_str(Weapon[_availWeapon[_selWeapon]].name));
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s, ",
                    sr_game_str(Armor[_availArmor[_selArmor]].name));
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s",
                    sr_game_str(Reactor[_availReactor[_selReactor]].name));
                if (_selAbility1 >= 0 && _selAbility1 < _filteredAbilityCount) {
                    pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s",
                        sr_game_str(Ability[_filteredAbility[_selAbility1]].name));
                }
                if (_selAbility2 >= 0 && _selAbility2 < _filteredAbilityCount) {
                    int a2 = _filteredAbility[_selAbility2];
                    if (_selAbility1 < 0 || a2 != _filteredAbility[_selAbility1]) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, ", %s",
                            sr_game_str(Ability[a2].name));
                    }
                }
                char cost_str[64];
                snprintf(cost_str, sizeof(cost_str), loc(SR_DESIGN_COST), get_edit_cost());
                pos += snprintf(buf + pos, sizeof(buf) - pos, ". %s", cost_str);
            }
            sr_output(buf, true);
            return true;
        }
        return false;

    default:
        return false;
    }
}

// Handle key in Level 1 (prototype list)
static bool handle_list_key(WPARAM wParam) {
    bool ctrl = ctrl_key_down();

    switch (wParam) {
    case VK_UP:
        if (ctrl) return false;
        if (_protoCount > 0) {
            _protoIndex = (_protoIndex + _protoCount - 1) % _protoCount;
            _deleteConfirmId = -1;
            announce_proto();
        }
        return true;

    case VK_DOWN:
        if (ctrl) return false;
        if (_protoCount > 0) {
            _protoIndex = (_protoIndex + 1) % _protoCount;
            _deleteConfirmId = -1;
            announce_proto();
        }
        return true;

    case VK_RETURN: {
        // Edit selected prototype
        if (_protoCount == 0) return true;
        int uid = _protoList[_protoIndex];
        // Only allow editing custom prototypes (faction's own slots)
        int fid = get_faction_id();
        int base = fid * MaxProtoFactionNum;
        if (uid < base || uid >= base + MaxProtoFactionNum) {
            // Default unit: create a copy in an empty slot
            int slot = find_empty_slot();
            if (slot < 0) {
                // No empty slots — cannot edit
                char buf[256];
                snprintf(buf, sizeof(buf), "Cannot: no empty prototype slots");
                sr_output(buf, true);
                return true;
            }
            // Copy the default unit to the new slot
            Units[slot] = Units[uid];
            Units[slot].unit_flags = UNIT_ACTIVE;
            Units[slot].icon_offset = -1;
            _editUnitId = slot;
        } else {
            _editUnitId = uid;
        }
        _editIsNew = false;
        _editing = true;
        _deleteConfirmId = -1;
        init_editor_from_unit(_editUnitId);

        char buf[256];
        snprintf(buf, sizeof(buf), "%s: %s. %s",
            loc(SR_DESIGN_WORKSHOP),
            sr_game_str(Units[_editUnitId].name),
            loc(SR_DESIGN_EDIT_HELP));
        sr_output(buf, true);
        return true;
    }

    case 'N':
        if (!ctrl) {
            // Create new unit
            int slot = find_empty_slot();
            if (slot < 0) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Cannot: no empty prototype slots");
                sr_output(buf, true);
                return true;
            }
            _editUnitId = slot;
            _editIsNew = true;
            _editing = true;
            _deleteConfirmId = -1;

            // Initialize the slot as active so mod_make_proto can work with it
            memset(&Units[slot], 0, sizeof(UNIT));
            Units[slot].unit_flags = UNIT_ACTIVE;
            Units[slot].icon_offset = -1;

            init_editor_new();

            char buf[256];
            snprintf(buf, sizeof(buf), "%s. %s",
                loc(SR_DESIGN_NEW), loc(SR_DESIGN_EDIT_HELP));
            sr_output(buf, true);
            return true;
        }
        return false;

    case VK_DELETE: {
        if (_protoCount == 0) return true;
        int uid = _protoList[_protoIndex];
        int fid = get_faction_id();
        int base = fid * MaxProtoFactionNum;

        if (uid < MaxProtoFactionNum) {
            // Default unit: mark as obsolete for this faction
            if (_deleteConfirmId == uid) {
                Units[uid].obsolete_factions |= (1 << fid);
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_DESIGN_RETIRED),
                    sr_game_str(Units[uid].name));
                sr_output(buf, true);
                _deleteConfirmId = -1;
                build_proto_list();
                if (_protoIndex >= _protoCount) _protoIndex = _protoCount - 1;
                if (_protoIndex < 0) _protoIndex = 0;
                if (_protoCount > 0) announce_proto();
            } else {
                _deleteConfirmId = uid;
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_DESIGN_RETIRE_CONFIRM),
                    sr_game_str(Units[uid].name));
                sr_output(buf, true);
            }
        } else if (uid >= base && uid < base + MaxProtoFactionNum) {
            // Custom unit: retire
            if (_deleteConfirmId == uid) {
                Units[uid].unit_flags &= ~UNIT_ACTIVE;
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_DESIGN_RETIRED),
                    sr_game_str(Units[uid].name));
                sr_output(buf, true);
                _deleteConfirmId = -1;
                build_proto_list();
                if (_protoIndex >= _protoCount) _protoIndex = _protoCount - 1;
                if (_protoIndex < 0) _protoIndex = 0;
                if (_protoCount > 0) announce_proto();
            } else {
                _deleteConfirmId = uid;
                char buf[256];
                snprintf(buf, sizeof(buf), loc(SR_DESIGN_RETIRE_CONFIRM),
                    sr_game_str(Units[uid].name));
                sr_output(buf, true);
            }
        }
        return true;
    }

    case VK_ESCAPE:
        _wantClose = true;
        return true;

    case 'I':
        if (ctrl) {
            if (_protoCount > 0) announce_proto();
            return true;
        }
        return false;

    case 'S':
    case VK_TAB:
        if (!ctrl || wParam == VK_TAB) {
            // Summary
            char buf[256];
            snprintf(buf, sizeof(buf), loc(SR_DESIGN_PROTO_LIST), _protoCount);
            sr_output(buf, true);
            return true;
        }
        return false;

    case VK_F1:
        if (ctrl) {
            sr_output(loc(SR_DESIGN_HELP), true);
            return true;
        }
        return false;

    default:
        return false;
    }
}

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return false;

    if (_editing) {
        return handle_editor_key(wParam);
    } else {
        return handle_list_key(wParam);
    }
}

void RunModal() {
    int fid = get_faction_id();
    if (fid < 0 || fid >= MaxPlayerNum) return;

    _active = true;
    _wantClose = false;
    _editing = false;
    _editUnitId = -1;
    _deleteConfirmId = -1;

    build_proto_list();
    _protoIndex = 0;

    sr_debug_log("DesignHandler::RunModal enter, %d prototypes\n", _protoCount);

    // Announce opening
    char buf[512];
    snprintf(buf, sizeof(buf), loc(SR_DESIGN_PROTO_LIST), _protoCount);
    strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
    strncat(buf, loc(SR_DESIGN_HELP), sizeof(buf) - strlen(buf) - 1);
    sr_output(buf, true);

    sr_run_modal_pump(&_wantClose);

    _active = false;
    _editing = false;
    sr_debug_log("DesignHandler::RunModal exit\n");

    draw_map(1);
}

} // namespace DesignHandler
