#!/usr/bin/env python3
"""
Create German smac_mod/de/alphax.txt by copying the German original
and applying the same Disable changes as the English mod.
"""
import os, re, sys

GAME = os.path.join("C:", os.sep, "Program Files (x86)", "Sid Meier's Alpha Centauri Planetary Pack")
PROJ = os.path.join("C:", os.sep, "Users", "Sonja", "Documents", "Modprojekte", "thinker")

en_path = os.path.join(GAME, "smac_mod", "alphax.txt")
de_path = os.path.join(GAME, "alphax.txt")
out_path = os.path.join(PROJ, "smac_mod", "de", "alphax.txt")

def read_file(path):
    with open(path, 'rb') as f:
        return f.read()

en_text = read_file(en_path).decode('cp1252')
de_text = read_file(de_path).decode('cp1252')

en_lines = en_text.replace('\r\n', '\n').split('\n')
de_lines = de_text.replace('\r\n', '\n').split('\n')

def parse_sections(lines):
    result = []
    current_name = None
    current_lines = []
    for line in lines:
        m = re.match(r'^#(\w+)', line)
        if m:
            if current_lines or current_name is not None:
                result.append((current_name, current_lines))
            current_name = m.group(1)
            current_lines = [line]
        else:
            current_lines.append(line)
    if current_lines or current_name is not None:
        result.append((current_name, current_lines))
    return result

en_secs = parse_sections(en_lines)
de_secs = parse_sections(de_lines)

en_dict = {name: lines for name, lines in en_secs if name}
de_dict = {name: list(lines) for name, lines in de_secs if name}

def is_data_line(line):
    s = line.strip()
    return s and not s.startswith(';') and not s.startswith('#')

def get_data_lines(lines):
    return [(i, l) for i, l in enumerate(lines) if is_data_line(l)]

TRANSFER_SECTIONS = [
    'TERRAIN', 'TECHNOLOGY', 'WEAPONS', 'DEFENSES', 'ABILITIES',
    'UNITS', 'FACILITIES', 'PROJECTS', 'CITIZENS', 'PROPOSALS'
]

changes = 0

for sec_name in TRANSFER_SECTIONS:
    if sec_name not in en_dict or sec_name not in de_dict:
        print(f"WARNING: Section {sec_name} missing in one file")
        continue

    en_data = get_data_lines(en_dict[sec_name])
    de_data = get_data_lines(de_dict[sec_name])
    min_len = min(len(en_data), len(de_data))

    for j in range(min_len):
        en_idx, en_line = en_data[j]
        de_idx, de_line = de_data[j]

        en_fields = [f.strip() for f in en_line.split(',')]
        de_fields = [f.strip() for f in de_line.split(',')]

        # Use the shorter field count (EN may have extra fields from "; was" comments)
        compare_len = min(len(en_fields), len(de_fields))
        if compare_len == 0:
            continue

        modified = False
        for k in range(compare_len):
            if en_fields[k] == 'Disable' and de_fields[k] != 'Disable':
                de_fields[k] = 'Disable'
                modified = True

        if modified:
            orig_parts = de_line.split(',')
            new_parts = []
            for k, orig in enumerate(orig_parts):
                if k < len(de_fields) and de_fields[k] == 'Disable' and orig.strip() != 'Disable':
                    leading = len(orig) - len(orig.lstrip())
                    new_part = orig[:leading] + 'Disable'
                    new_parts.append(new_part)
                else:
                    new_parts.append(orig)
            de_dict[sec_name][de_idx] = ','.join(new_parts)
            changes += 1

print(f"Applied {changes} Disable changes")

# Transfer "; was :" comments
for sec_name in TRANSFER_SECTIONS:
    if sec_name not in en_dict:
        continue
    en_sec = en_dict[sec_name]
    for i, line in enumerate(en_sec):
        s = line.strip()
        if s.startswith('; was :') or s.startswith(';  was :') or s.startswith('; was:'):
            data_before = len([l for l in en_sec[:i] if is_data_line(l)])
            if sec_name in de_dict:
                de_sec = de_dict[sec_name]
                count = 0
                insert_after = -1
                for j2, dl in enumerate(de_sec):
                    if is_data_line(dl):
                        count += 1
                        if count == data_before:
                            insert_after = j2
                            break
                if insert_after >= 0 and insert_after + 1 < len(de_sec):
                    next_l = de_sec[insert_after + 1].strip()
                    if not next_l.startswith('; was'):
                        de_dict[sec_name].insert(insert_after + 1, line)

# Transfer comment blocks ("; Disabled smacx ..." etc.)
for sec_name in TRANSFER_SECTIONS:
    if sec_name not in en_dict:
        continue
    en_sec = en_dict[sec_name]
    for i, line in enumerate(en_sec):
        s = line.strip()
        if s.startswith('; Disabled') or s.startswith('; Reinstated') or s.startswith('; Original'):
            # Check if already in German section
            de_sec = de_dict.get(sec_name, [])
            if not any(s in dl for dl in de_sec):
                de_dict[sec_name].append(line)

# Handle UNITS - add extra Thinker units
en_units = get_data_lines(en_dict.get('UNITS', []))
de_units = get_data_lines(de_dict.get('UNITS', []))
print(f"Units: EN={len(en_units)} DE={len(de_units)}")

    # Translate English unit field names to German equivalents
    # Order matters: longer strings first to avoid partial matches
EN_TO_DE_FIELDS = {
    'Colony Pod': 'Koloniekapsel',
    'Trance Infantry': 'Trance-Infanterie',
    'Police Garrison': 'Polizei-Garnison',
    'Drop Colony Pod': 'Abwurf-Koloniekapsel',
    'Infantry': 'Infanterie',
    'Gun': 'Geschütz',
    'Transport': 'Transport',
    'Scout': 'Scout',
    'Speeder': 'Speeder',
    'Hovertank': 'Schwebefahrzeug',
    'Foil': 'Tragflügelboot',
    'Cruiser': 'Kreuzer',
    'Needlejet': 'Düsenjäger',
    'Missile': 'Rakete',
    'Copter': 'Hubschrauber',
    'Gravship': 'Grav-Schiff',
}

if len(en_units) > len(de_units):
    extra_start = len(de_units)
    de_sec = de_dict['UNITS']
    last_data = max(i for i, l in enumerate(de_sec) if is_data_line(l))
    for j in range(extra_start, len(en_units)):
        en_idx, en_line = en_units[j]
        # Translate field references to German (longest keys first)
        translated = en_line
        for en_name, de_name in sorted(EN_TO_DE_FIELDS.items(), key=lambda x: len(x[0]), reverse=True):
            translated = translated.replace(en_name, de_name)
        de_dict['UNITS'].insert(last_data + 1, translated)
        last_data += 1
        print(f"  Added unit: {translated[:50]}...")

    # Update count
    count_idx = de_units[0][0]
    de_dict['UNITS'][count_idx] = en_units[0][1]
    print(f"  Updated unit count to: {en_units[0][1].strip()}")

# Handle WORLDSIZE - add extra entry
en_ws = get_data_lines(en_dict.get('WORLDSIZE', []))
de_ws = get_data_lines(de_dict.get('WORLDSIZE', []))
print(f"WorldSize: EN={len(en_ws)} DE={len(de_ws)}")

if len(en_ws) > len(de_ws):
    de_sec = de_dict['WORLDSIZE']
    last_data = max(i for i, l in enumerate(de_sec) if is_data_line(l))
    for j in range(len(de_ws), len(en_ws)):
        en_idx, en_line = en_ws[j]
        line_to_add = en_line.replace('Very Huge Planet', 'Sehr riesiger Planet')
        de_dict['WORLDSIZE'].insert(last_data + 1, line_to_add)
        last_data += 1
        print(f"  Added: {line_to_add[:50]}...")
    count_idx = de_ws[0][0]
    de_dict['WORLDSIZE'][count_idx] = en_ws[0][1]

# Add mod header
header = [
    ';',
    '; *************************************',
    '; *  SMAC-in-SMACX Mod / Thinker Mod  *',
    '; *************************************',
    ';',
    '; Deutsche Version. Basierend auf der deutschen SMACX GOG-Version.',
    '; SMACX-Technologien, Waffen, Einheiten und Einrichtungen deaktiviert.',
    ';',
]

# Reassemble
out_lines = header[:]
for name, orig_lines in de_secs:
    if name and name in de_dict:
        out_lines.extend(de_dict[name])
    else:
        out_lines.extend(orig_lines)

out_text = '\r\n'.join(out_lines)
out_bytes = out_text.encode('cp1252', errors='replace')

os.makedirs(os.path.dirname(out_path), exist_ok=True)
with open(out_path, 'wb') as f:
    f.write(out_bytes)

print(f"\nWrote {len(out_bytes)} bytes to {out_path}")
print(f"Total lines: {len(out_lines)}")
