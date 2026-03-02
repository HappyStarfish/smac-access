/*
 * DatalinksHandler — F1 Datalinks accessible browser.
 *
 * Full modal replacement: intercepts F1 before the game, reads
 * Tech[], Facility[], Weapon[], Armor[], Chassis[], Reactor[],
 * Ability[] arrays directly from memory. Nine browseable categories
 * with detail view. Runs own PeekMessage loop.
 */

#include "datalinks_handler.h"
#include "screen_reader.h"
#include "localization.h"
#include "modal_utils.h"
#include "gui.h"
#include "tech.h"

static bool _active = false;
static bool _wantClose = false;

// Category definitions
enum DLCategory {
    DLCAT_TECH = 0,
    DLCAT_FACILITY,
    DLCAT_PROJECT,
    DLCAT_WEAPON,
    DLCAT_ARMOR,
    DLCAT_CHASSIS,
    DLCAT_REACTOR,
    DLCAT_ABILITY,
    DLCAT_CONCEPT,
    DLCAT_COUNT
};

static int _currentCat = DLCAT_TECH;
static int _currentIndex = 0;

// Item lists per category (pre-built on open)
static int _techIds[MaxTechnologyNum];
static int _techCount = 0;

static int _facIds[Fac_ID_Last]; // 1..64
static int _facCount = 0;

static int _projIds[SP_ID_Last - SP_ID_First + 1];
static int _projCount = 0;

// Simple components: indices 0..Max-1
static int _weaponCount = 0;
static int _armorCount = 0;
static int _chassisCount = 0;
static int _reactorCount = 0;
static int _abilityCount = 0;

// Concepts: basic (0..49) + advanced (50..64)
// We store concept indices sequentially.
static const int MAX_CONCEPTS = 65;
static int _conceptCount = 0;

// Concept titles loaded from conceptsx.txt
static char _conceptTitles[MAX_CONCEPTS][128];

// Concept full texts (up to 2KB each)
static const int CONCEPT_TEXT_LEN = 16384;
static char _conceptTexts[MAX_CONCEPTS][CONCEPT_TEXT_LEN];

/// Get item count for current category.
static int GetItemCount() {
    switch (_currentCat) {
    case DLCAT_TECH:    return _techCount;
    case DLCAT_FACILITY: return _facCount;
    case DLCAT_PROJECT: return _projCount;
    case DLCAT_WEAPON:  return _weaponCount;
    case DLCAT_ARMOR:   return _armorCount;
    case DLCAT_CHASSIS: return _chassisCount;
    case DLCAT_REACTOR: return _reactorCount;
    case DLCAT_ABILITY: return _abilityCount;
    case DLCAT_CONCEPT: return _conceptCount;
    default: return 0;
    }
}

/// Get localized category name.
static const char* GetCategoryName() {
    switch (_currentCat) {
    case DLCAT_TECH:     return loc(SR_DL_CAT_TECH);
    case DLCAT_FACILITY: return loc(SR_DL_CAT_FACILITY);
    case DLCAT_PROJECT:  return loc(SR_DL_CAT_PROJECT);
    case DLCAT_WEAPON:   return loc(SR_DL_CAT_WEAPON);
    case DLCAT_ARMOR:    return loc(SR_DL_CAT_ARMOR);
    case DLCAT_CHASSIS:  return loc(SR_DL_CAT_CHASSIS);
    case DLCAT_REACTOR:  return loc(SR_DL_CAT_REACTOR);
    case DLCAT_ABILITY:  return loc(SR_DL_CAT_ABILITY);
    case DLCAT_CONCEPT:  return loc(SR_DL_CAT_CONCEPT);
    default: return "?";
    }
}

/// Get tech category label (Explore/Discover/Build/Conquer).
static const char* GetTechCategoryLabel(int tech_id) {
    int cat = tech_category(tech_id);
    switch (cat) {
    case TCAT_GROWTH: return loc(SR_DL_TCAT_EXPLORE);
    case TCAT_TECH:   return loc(SR_DL_TCAT_DISCOVER);
    case TCAT_WEALTH: return loc(SR_DL_TCAT_BUILD);
    case TCAT_POWER:  return loc(SR_DL_TCAT_CONQUER);
    default: return "?";
    }
}

/// Get prerequisite tech name, handling special values.
static const char* GetPreqName(int preq_tech) {
    if (preq_tech == TECH_None) return loc(SR_DL_PREQ_NONE);
    if (preq_tech == TECH_Disable) return loc(SR_DL_PREQ_DISABLED);
    if (preq_tech >= 0 && preq_tech < MaxTechnologyNum && Tech[preq_tech].name) {
        return sr_game_str(Tech[preq_tech].name);
    }
    return "?";
}

/// Build all item lists from game memory.
static void BuildLists() {
    int owner = MapWin->cOwner;

    // Technologies: all valid techs
    _techCount = 0;
    for (int i = 0; i < MaxTechnologyNum; i++) {
        if (Tech[i].name != NULL && Tech[i].preq_tech1 != TECH_Disable) {
            _techIds[_techCount++] = i;
        }
    }

    // Base facilities: IDs 1..Fac_ID_Last (not secret projects)
    _facCount = 0;
    for (int i = 1; i <= Fac_ID_Last; i++) {
        if (Facility[i].name != NULL) {
            _facIds[_facCount++] = i;
        }
    }

    // Secret projects: SP_ID_First..SP_ID_Last
    _projCount = 0;
    for (int i = SP_ID_First; i <= SP_ID_Last; i++) {
        if (Facility[i].name != NULL) {
            _projIds[_projCount++] = i;
        }
    }

    // Component counts (use all valid entries)
    _weaponCount = 0;
    for (int i = 0; i < MaxWeaponNum; i++) {
        if (Weapon[i].name != NULL) _weaponCount = i + 1;
    }

    _armorCount = 0;
    for (int i = 0; i < MaxArmorNum; i++) {
        if (Armor[i].name != NULL) _armorCount = i + 1;
    }

    _chassisCount = 0;
    for (int i = 0; i < MaxChassisNum; i++) {
        if (Chassis[i].offsv1_name != NULL) _chassisCount = i + 1;
    }

    _reactorCount = MaxReactorNum; // always 4

    _abilityCount = 0;
    for (int i = 0; i < MaxAbilityNum; i++) {
        if (Ability[i].name != NULL) _abilityCount = i + 1;
    }

    // Concepts: load titles from conceptsx.txt
    _conceptCount = 0;
    memset(_conceptTitles, 0, sizeof(_conceptTitles));
    memset(_conceptTexts, 0, sizeof(_conceptTexts));

    // Try to find and parse conceptsx.txt for titles and texts
    char path[512];
    GetModuleFileNameA(NULL, path, sizeof(path));
    // Strip exe name to get game directory
    char* last_slash = strrchr(path, '\\');
    if (last_slash) *(last_slash + 1) = '\0';
    strncat(path, "conceptsx.txt", sizeof(path) - strlen(path) - 1);

    FILE* f = fopen(path, "r");
    if (f) {
        char line[256];
        bool in_titles = false;
        bool in_advtitles = false;
        int title_idx = 0;
        int cur_concept = -1; // which concept text we're reading (-1 = none)

        while (fgets(line, sizeof(line), f)) {
            // Strip newline
            int len = (int)strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                line[--len] = '\0';
            }

            // Check for section headers
            if (line[0] == '#') {
                in_titles = false;
                in_advtitles = false;
                cur_concept = -1;

                if (strncmp(line, "#TITLES", 7) == 0) {
                    in_titles = true;
                    title_idx = 0;
                } else if (strncmp(line, "#ADVTITLES", 10) == 0) {
                    in_advtitles = true;
                    title_idx = 0;
                } else if (strncmp(line, "#CONCEPT", 8) == 0 && line[8] != '\0') {
                    // #CONCEPT0 .. #CONCEPT49
                    int num = atoi(line + 8);
                    if (num >= 0 && num < 50) cur_concept = num;
                } else if (strncmp(line, "#ADVCONCEPT", 11) == 0 && line[11] != '\0') {
                    // #ADVCONCEPT0 .. #ADVCONCEPT14
                    int num = atoi(line + 11);
                    if (num >= 0 && num < 15) cur_concept = 50 + num;
                }
                continue;
            }

            // Skip comments
            if (line[0] == ';') continue;

            // Parse titles
            if (in_titles && len > 0 && title_idx < 50) {
                int ci = title_idx;
                if (ci < MAX_CONCEPTS) {
                    strncpy(_conceptTitles[ci], line, sizeof(_conceptTitles[0]) - 1);
                    if (ci + 1 > _conceptCount) _conceptCount = ci + 1;
                }
                title_idx++;
            }
            if (in_advtitles && len > 0 && title_idx < 15) {
                int ci = 50 + title_idx;
                if (ci < MAX_CONCEPTS) {
                    strncpy(_conceptTitles[ci], line, sizeof(_conceptTitles[0]) - 1);
                    if (ci + 1 > _conceptCount) _conceptCount = ci + 1;
                }
                title_idx++;
            }

            // Parse concept body text
            if (cur_concept >= 0 && cur_concept < MAX_CONCEPTS && len > 0) {
                char* dst = _conceptTexts[cur_concept];
                int cur_len = (int)strlen(dst);
                int remaining = CONCEPT_TEXT_LEN - cur_len - 1;
                if (remaining <= 0) continue;

                // ^ is paragraph separator in concepts file — replace with space
                if (line[0] == '^') {
                    if (cur_len > 0 && remaining > 1) {
                        strncat(dst, " ", remaining);
                    }
                    continue;
                }

                // Strip $LINK<text=id> markers, keep just the text part
                // Also strip {curly braces} used for bold/emphasis in game
                char clean[256];
                int ci_out = 0;
                for (int i = 0; i < len && ci_out < (int)sizeof(clean) - 1; ) {
                    if (strncmp(line + i, "$LINK<", 6) == 0) {
                        // Extract text before '='
                        i += 6;
                        while (i < len && line[i] != '=' && line[i] != '>'
                               && ci_out < (int)sizeof(clean) - 1) {
                            clean[ci_out++] = line[i++];
                        }
                        // Skip past closing >
                        while (i < len && line[i] != '>') i++;
                        if (i < len) i++; // skip >
                    } else if (line[i] == '{' || line[i] == '}') {
                        i++; // skip braces
                    } else {
                        clean[ci_out++] = line[i++];
                    }
                }
                clean[ci_out] = '\0';

                // Append to concept text with space separator
                if (cur_len > 0 && remaining > 1) {
                    strncat(dst, " ", remaining);
                    remaining--;
                }
                strncat(dst, clean, remaining);
            }
        }
        fclose(f);
    }

    if (_conceptCount == 0) {
        sr_debug_log("DatalinksHandler: no concepts loaded from %s\n", path);
    } else {
        sr_debug_log("DatalinksHandler: loaded %d concepts\n", _conceptCount);
    }
}

/// Announce current item. interrupt=true for direct nav, false for queued after category.
static void AnnounceItemImpl(bool interrupt) {
    int count = GetItemCount();
    if (count == 0) {
        sr_output(loc(SR_DL_EMPTY), interrupt);
        return;
    }

    int owner = MapWin->cOwner;
    char buf[512];

    switch (_currentCat) {
    case DLCAT_TECH: {
        int tid = _techIds[_currentIndex];
        const char* name = sr_game_str(Tech[tid].name);
        bool known = has_tech(tid, owner) != 0;
        snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_TECH),
                 _currentIndex + 1, count, name,
                 known ? loc(SR_DL_KNOWN) : loc(SR_DL_UNKNOWN));
        break;
    }
    case DLCAT_FACILITY: {
        int fid = _facIds[_currentIndex];
        const char* name = sr_game_str(Facility[fid].name);
        snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_FACILITY),
                 _currentIndex + 1, count, name,
                 Facility[fid].cost, Facility[fid].maint);
        break;
    }
    case DLCAT_PROJECT: {
        int pid = _projIds[_currentIndex];
        const char* name = sr_game_str(Facility[pid].name);
        int sp_idx = pid - SP_ID_First;
        int base_id = SecretProjects[sp_idx];
        if (base_id >= 0 && base_id < *BaseCount) {
            int fac_owner = Bases[base_id].faction_id;
            snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_PROJECT_BUILT),
                     _currentIndex + 1, count, name,
                     sr_game_str(MFactions[fac_owner].noun_faction));
        } else {
            snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_PROJECT),
                     _currentIndex + 1, count, name);
        }
        break;
    }
    case DLCAT_WEAPON: {
        const char* name = sr_game_str(Weapon[_currentIndex].name);
        snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_WEAPON),
                 _currentIndex + 1, count, name,
                 (int)Weapon[_currentIndex].offense_value);
        break;
    }
    case DLCAT_ARMOR: {
        const char* name = sr_game_str(Armor[_currentIndex].name);
        snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_ARMOR),
                 _currentIndex + 1, count, name,
                 (int)Armor[_currentIndex].defense_value);
        break;
    }
    case DLCAT_CHASSIS: {
        const char* name = sr_game_str(Chassis[_currentIndex].offsv1_name);
        const char* triad;
        switch (Chassis[_currentIndex].triad) {
        case TRIAD_LAND: triad = loc(SR_DESIGN_TRIAD_LAND); break;
        case TRIAD_SEA:  triad = loc(SR_DESIGN_TRIAD_SEA); break;
        case TRIAD_AIR:  triad = loc(SR_DESIGN_TRIAD_AIR); break;
        default: triad = "?"; break;
        }
        snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_CHASSIS),
                 _currentIndex + 1, count, name,
                 (int)Chassis[_currentIndex].speed, triad);
        break;
    }
    case DLCAT_REACTOR: {
        const char* name = sr_game_str(Reactor[_currentIndex].name);
        snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_REACTOR),
                 _currentIndex + 1, count, name,
                 (int)Reactor[_currentIndex].power);
        break;
    }
    case DLCAT_ABILITY: {
        const char* name = sr_game_str(Ability[_currentIndex].name);
        snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_ABILITY),
                 _currentIndex + 1, count, name);
        break;
    }
    case DLCAT_CONCEPT: {
        const char* title = _conceptTitles[_currentIndex][0]
            ? sr_game_str(_conceptTitles[_currentIndex]) : "?";
        snprintf(buf, sizeof(buf), loc(SR_DL_ITEM_CONCEPT),
                 _currentIndex + 1, count, title);
        break;
    }
    default:
        return;
    }

    sr_output(buf, interrupt);
}

/// Announce current item (interrupt mode, for Up/Down navigation).
static void AnnounceItem() { AnnounceItemImpl(true); }

/// Announce current item (queued mode, after category announcement).
static void AnnounceItemQueued() { AnnounceItemImpl(false); }

/// Announce detail info for current item (D key).
static void AnnounceDetail() {
    int count = GetItemCount();
    if (count == 0) return;

    int owner = MapWin->cOwner;
    char buf[16384];
    buf[0] = '\0';

    switch (_currentCat) {
    case DLCAT_TECH: {
        int tid = _techIds[_currentIndex];
        CTech& t = Tech[tid];
        const char* name = sr_game_str(t.name);
        const char* cat = GetTechCategoryLabel(tid);
        bool known = has_tech(tid, owner) != 0;
        int level = tech_level(tid, 0);

        // Build prerequisite string, avoiding "None, None" redundancy
        bool p1_none = (t.preq_tech1 == TECH_None || t.preq_tech1 == TECH_Disable);
        bool p2_none = (t.preq_tech2 == TECH_None || t.preq_tech2 == TECH_Disable);
        char preq_str[256];
        if (p1_none && p2_none) {
            snprintf(preq_str, sizeof(preq_str), "%s", loc(SR_DL_PREQ_NONE));
        } else if (p2_none) {
            snprintf(preq_str, sizeof(preq_str), "%s", GetPreqName(t.preq_tech1));
        } else if (p1_none) {
            snprintf(preq_str, sizeof(preq_str), "%s", GetPreqName(t.preq_tech2));
        } else {
            snprintf(preq_str, sizeof(preq_str), "%s, %s",
                     GetPreqName(t.preq_tech1), GetPreqName(t.preq_tech2));
        }

        snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_TECH_V2),
                 name, preq_str, cat, level,
                 known ? loc(SR_DL_KNOWN) : loc(SR_DL_UNKNOWN));

        // Append what this tech unlocks, with category labels
        char unlocks[512];
        unlocks[0] = '\0';
        int unlock_count = 0;
        char item_buf[128];

        // Check facilities
        for (int i = 1; i <= Fac_ID_Last; i++) {
            if (Facility[i].preq_tech == tid && Facility[i].name) {
                snprintf(item_buf, sizeof(item_buf), "%s (%s)",
                         sr_game_str(Facility[i].name), loc(SR_DL_CAT_FACILITY));
                if (unlock_count > 0) strncat(unlocks, ", ", sizeof(unlocks) - strlen(unlocks) - 1);
                strncat(unlocks, item_buf, sizeof(unlocks) - strlen(unlocks) - 1);
                unlock_count++;
            }
        }
        // Check secret projects
        for (int i = SP_ID_First; i <= SP_ID_Last; i++) {
            if (Facility[i].preq_tech == tid && Facility[i].name) {
                snprintf(item_buf, sizeof(item_buf), "%s (%s)",
                         sr_game_str(Facility[i].name), loc(SR_DL_CAT_PROJECT));
                if (unlock_count > 0) strncat(unlocks, ", ", sizeof(unlocks) - strlen(unlocks) - 1);
                strncat(unlocks, item_buf, sizeof(unlocks) - strlen(unlocks) - 1);
                unlock_count++;
            }
        }
        // Check weapons
        for (int i = 0; i < MaxWeaponNum; i++) {
            if (Weapon[i].preq_tech == tid && Weapon[i].name) {
                snprintf(item_buf, sizeof(item_buf), "%s (%s)",
                         sr_game_str(Weapon[i].name), loc(SR_DL_CAT_WEAPON));
                if (unlock_count > 0) strncat(unlocks, ", ", sizeof(unlocks) - strlen(unlocks) - 1);
                strncat(unlocks, item_buf, sizeof(unlocks) - strlen(unlocks) - 1);
                unlock_count++;
            }
        }
        // Check armor
        for (int i = 0; i < MaxArmorNum; i++) {
            if (Armor[i].preq_tech == tid && Armor[i].name) {
                snprintf(item_buf, sizeof(item_buf), "%s (%s)",
                         sr_game_str(Armor[i].name), loc(SR_DL_CAT_ARMOR));
                if (unlock_count > 0) strncat(unlocks, ", ", sizeof(unlocks) - strlen(unlocks) - 1);
                strncat(unlocks, item_buf, sizeof(unlocks) - strlen(unlocks) - 1);
                unlock_count++;
            }
        }
        // Check chassis
        for (int i = 0; i < MaxChassisNum; i++) {
            if (Chassis[i].preq_tech == tid && Chassis[i].offsv1_name) {
                snprintf(item_buf, sizeof(item_buf), "%s (%s)",
                         sr_game_str(Chassis[i].offsv1_name), loc(SR_DL_CAT_CHASSIS));
                if (unlock_count > 0) strncat(unlocks, ", ", sizeof(unlocks) - strlen(unlocks) - 1);
                strncat(unlocks, item_buf, sizeof(unlocks) - strlen(unlocks) - 1);
                unlock_count++;
            }
        }
        // Check abilities
        for (int i = 0; i < MaxAbilityNum; i++) {
            if (Ability[i].preq_tech == tid && Ability[i].name) {
                snprintf(item_buf, sizeof(item_buf), "%s (%s)",
                         sr_game_str(Ability[i].name), loc(SR_DL_CAT_ABILITY));
                if (unlock_count > 0) strncat(unlocks, ", ", sizeof(unlocks) - strlen(unlocks) - 1);
                strncat(unlocks, item_buf, sizeof(unlocks) - strlen(unlocks) - 1);
                unlock_count++;
            }
        }

        if (unlock_count > 0) {
            strncat(buf, ". ", sizeof(buf) - strlen(buf) - 1);
            strncat(buf, loc(SR_DL_UNLOCKS), sizeof(buf) - strlen(buf) - 1);
            strncat(buf, unlocks, sizeof(buf) - strlen(buf) - 1);
        }
        break;
    }
    case DLCAT_FACILITY: {
        int fid = _facIds[_currentIndex];
        CFacility& fac = Facility[fid];
        const char* name = sr_game_str(fac.name);
        const char* preq = GetPreqName(fac.preq_tech);
        const char* effect = fac.effect ? sr_game_str(fac.effect) : "";
        snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_FACILITY),
                 name, fac.cost, fac.maint, preq, effect);
        break;
    }
    case DLCAT_PROJECT: {
        int pid = _projIds[_currentIndex];
        CFacility& fac = Facility[pid];
        const char* name = sr_game_str(fac.name);
        const char* preq = GetPreqName(fac.preq_tech);
        const char* effect = fac.effect ? sr_game_str(fac.effect) : "";
        int sp_idx = pid - SP_ID_First;
        int base_id = SecretProjects[sp_idx];

        if (base_id >= 0 && base_id < *BaseCount) {
            int fac_owner = Bases[base_id].faction_id;
            snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_PROJECT_BUILT),
                     name, fac.cost, preq,
                     sr_game_str(MFactions[fac_owner].noun_faction),
                     sr_game_str(Bases[base_id].name),
                     effect);
        } else {
            snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_PROJECT),
                     name, fac.cost, preq, effect);
        }
        break;
    }
    case DLCAT_WEAPON: {
        CWeapon& w = Weapon[_currentIndex];
        const char* name = sr_game_str(w.name);
        const char* preq = GetPreqName(w.preq_tech);
        snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_WEAPON),
                 name, (int)w.offense_value, (int)w.cost, preq);
        break;
    }
    case DLCAT_ARMOR: {
        CArmor& a = Armor[_currentIndex];
        const char* name = sr_game_str(a.name);
        const char* preq = GetPreqName(a.preq_tech);
        snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_ARMOR),
                 name, (int)a.defense_value, (int)a.cost, preq);
        break;
    }
    case DLCAT_CHASSIS: {
        CChassis& c = Chassis[_currentIndex];
        const char* name = sr_game_str(c.offsv1_name);
        const char* preq = GetPreqName(c.preq_tech);
        const char* triad;
        switch (c.triad) {
        case TRIAD_LAND: triad = loc(SR_DESIGN_TRIAD_LAND); break;
        case TRIAD_SEA:  triad = loc(SR_DESIGN_TRIAD_SEA); break;
        case TRIAD_AIR:  triad = loc(SR_DESIGN_TRIAD_AIR); break;
        default: triad = "?"; break;
        }
        snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_CHASSIS),
                 name, (int)c.speed, triad, (int)c.range, preq);
        break;
    }
    case DLCAT_REACTOR: {
        CReactor& r = Reactor[_currentIndex];
        const char* name = sr_game_str(r.name);
        const char* preq = GetPreqName(r.preq_tech);
        snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_REACTOR),
                 name, (int)r.power, preq);
        break;
    }
    case DLCAT_ABILITY: {
        CAbility& a = Ability[_currentIndex];
        const char* name = sr_game_str(a.name);
        const char* desc = a.description ? sr_game_str(a.description) : "";
        const char* preq = GetPreqName(a.preq_tech);
        snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_ABILITY),
                 name, desc, preq);
        break;
    }
    case DLCAT_CONCEPT: {
        const char* title = _conceptTitles[_currentIndex][0]
            ? sr_game_str(_conceptTitles[_currentIndex]) : "?";
        // Use direct UTF-8 conversion for concept texts (can be many KB).
        // sr_game_str() is limited to 512 bytes and would truncate.
        char text_utf8[CONCEPT_TEXT_LEN];
        if (_conceptTexts[_currentIndex][0]) {
            sr_ansi_to_utf8(_conceptTexts[_currentIndex],
                            text_utf8, sizeof(text_utf8));
        } else {
            snprintf(text_utf8, sizeof(text_utf8), "%s",
                     loc(SR_DL_CONCEPT_NO_TEXT));
        }
        snprintf(buf, sizeof(buf), loc(SR_DL_DETAIL_CONCEPT),
                 title, text_utf8);
        break;
    }
    default:
        return;
    }

    sr_output(buf, true);
}

/// Announce category switch.
static void AnnounceCategory() {
    int count = GetItemCount();
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_DL_CATEGORY_FMT),
             GetCategoryName(), count);
    sr_output(buf, true);  // interrupt: announce category name first

    if (count > 0) {
        // Queue item announcement (non-interrupt so category is heard first)
        AnnounceItemQueued();
    }
}

/// Announce summary (S key).
static void AnnounceSummary() {
    int owner = MapWin->cOwner;
    int known = 0;
    for (int i = 0; i < _techCount; i++) {
        if (has_tech(_techIds[i], owner)) known++;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_DL_SUMMARY),
             GetCategoryName(), GetItemCount(),
             known, _techCount);
    sr_output(buf, true);
}

namespace DatalinksHandler {

bool IsActive() {
    return _active;
}

bool Update(UINT msg, WPARAM wParam) {
    if (msg != WM_KEYDOWN) return true; // consume WM_CHAR etc.

    int count = GetItemCount();

    switch (wParam) {
    case VK_UP:
        if (count == 0) return true;
        if (_currentIndex > 0) {
            _currentIndex--;
        } else {
            _currentIndex = count - 1;
        }
        AnnounceItem();
        return true;

    case VK_DOWN:
        if (count == 0) return true;
        if (_currentIndex < count - 1) {
            _currentIndex++;
        } else {
            _currentIndex = 0;
        }
        AnnounceItem();
        return true;

    case VK_LEFT:
        _currentCat = (_currentCat + DLCAT_COUNT - 1) % DLCAT_COUNT;
        _currentIndex = 0;
        AnnounceCategory();
        return true;

    case VK_RIGHT:
        _currentCat = (_currentCat + 1) % DLCAT_COUNT;
        _currentIndex = 0;
        AnnounceCategory();
        return true;

    case VK_HOME:
        if (count == 0) return true;
        _currentIndex = 0;
        AnnounceItem();
        return true;

    case VK_END:
        if (count == 0) return true;
        _currentIndex = count - 1;
        AnnounceItem();
        return true;

    case VK_ESCAPE:
        _wantClose = true;
        return true;

    case VK_F1:
        if (ctrl_key_down()) {
            sr_output(loc(SR_DL_HELP), true);
        }
        return true;

    default:
        break;
    }

    // D key = detail info
    if (wParam == 'D' && !ctrl_key_down() && !shift_key_down()) {
        AnnounceDetail();
        return true;
    }

    // S key or Tab = summary
    if ((wParam == 'S' || wParam == VK_TAB) && !ctrl_key_down() && !shift_key_down()) {
        AnnounceSummary();
        return true;
    }

    return true; // consume all keys while modal
}

void RunModal() {
    sr_debug_log("DatalinksHandler::RunModal enter\n");

    BuildLists();
    _active = true;
    _wantClose = false;
    _currentCat = DLCAT_TECH;
    _currentIndex = 0;

    // Opening announcement: "Datalinks, 9 Kategorien. ..."
    char buf[256];
    snprintf(buf, sizeof(buf), loc(SR_DL_OPEN), DLCAT_COUNT);
    sr_output(buf, true);

    // Queue category name + first item (non-interrupt so opening is heard)
    int count = GetItemCount();
    char catbuf[256];
    snprintf(catbuf, sizeof(catbuf), loc(SR_DL_CATEGORY_FMT),
             GetCategoryName(), count);
    sr_output(catbuf, false);

    if (count > 0) {
        AnnounceItemQueued();
    }

    sr_run_modal_pump(&_wantClose);

    _active = false;
    sr_output(loc(SR_DL_CLOSED), true);
    sr_debug_log("DatalinksHandler::RunModal exit\n");
}

} // namespace DatalinksHandler
