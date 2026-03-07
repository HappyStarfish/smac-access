#!/usr/bin/env python3
"""
Fix corrupted German umlauts in smac_mod/de/alphax.txt.

The German GOG version of SMAC has ALL bytes > 0x7F replaced with 0x20 (space).
This means all umlauts (ä, ö, ü, Ä, Ö, Ü) and ß are replaced with spaces.
This script restores them using a comprehensive replacement dictionary.

Usage: python tools/fix_de_umlauts.py
"""
import os
import re
import sys

PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALPHAX = os.path.join(PROJ, "smac_mod", "de", "alphax.txt")

# Replacement dictionary: (corrupted, correct)
# Sorted longest-first within each group to avoid partial matches.
# Applied as simple string replacements.
#
# Note: The game uses pre-1996 German spelling (daß, muß, etc.)
# Double corruptions (two umlauts = two spaces): ö+ß, ä+ß, ü+ü

REPLACEMENTS = [
    # === Double corruptions (two adjacent umlauts both → space) ===
    # ö+ß → two spaces
    ("Gr  e", "Größe"),
    ("gr  e", "größe"),
    ("Gr  er", "Größer"),
    ("gr  er", "größer"),
    ("vergr  er", "vergrößer"),
    # ä+ß → two spaces
    ("gem  ", "gemäß"),       # gemäß (old spelling has ß)
    ("rechtm  ig", "rechtmäßig"),
    # ü+ü → two spaces
    ("berfl ssig", "überflüssig"),   # starts with space (ü→space at word start)
    # ü+ä
    ("Bed rfniss", "Bedürfniss"),   # Bedürfnisse → "Bed rfniss"... wait
    # Actually Bedürfnisse: B-e-d-ü-r-f-n-i-s-s-e, only one umlaut

    # === ß (0xDF) replacements ===
    ("einschlie lich", "einschließlich"),
    ("abschlie end", "abschließend"),
    ("anschlie end", "anschließend"),
    ("Abschlie end", "Abschließend"),
    ("Au erirdisch", "Außerirdisch"),
    ("au erirdisch", "außerirdisch"),
    ("Au erhalb", "Außerhalb"),
    ("au erhalb", "außerhalb"),
    ("Au er.", "Außer."),             # "Außer. Artefakt"
    ("au er", "außer"),
    ("Au er", "Außer"),
    ("Mi erfolg", "Mißerfolg"),       # old spelling: Mißerfolg
    ("Sicherheitsma nahme", "Sicherheitsmaßnahme"),
    ("Ma nahme", "Maßnahme"),
    ("ma nahme", "maßnahme"),
    ("umweltbewu t", "umweltbewußt"),   # old spelling
    ("pflichtbewu t", "pflichtbewußt"),
    ("angepa t", "angepaßt"),           # old spelling
    ("Stra en", "Straßen"),
    ("Stra e", "Straße"),
    ("hei en", "heißen"),
    ("hei e", "heiße"),
    ("hei t", "heißt"),
    ("wei ", "weiß"),
    ("Schlie ", "Schließ"),
    ("schlie ", "schließ"),
    ("schlie en", "schließen"),
    ("genie en", "genießen"),
    ("Genie en", "Genießen"),
    ("Gro e", "Große"),
    ("gro e", "große"),
    ("Gro en", "Großen"),
    ("gro en", "großen"),
    ("Gro er", "Großer"),
    ("gro er", "großer"),
    ("gro ", "groß"),
    ("Gro ", "Groß"),
    # "daß" and "muß" - need careful handling (old spelling)
    # These appear as "da " and "mu " followed by space/comma/period
    ("da  ", "daß "),
    ("mu  ", "muß "),

    # === ü (0xFC) replacements ===
    ("Bev lkerungswachstum", "Bevölkerungswachstum"),  # wait, this is ö! Moving to ö section
    ("See berlegenheit", "Seeüberlegenheit"),
    ("Luft berlegenheit", "Luftüberlegenheit"),
    ("un bertroffen", "unübertroffen"),
    ("Umr stungsausnahme", "Umrüstungsausnahme"),
    ("Umr stungsmalus", "Umrüstungsmalus"),
    ("Umr stungsregel", "Umrüstungsregel"),
    ("Umr stung", "Umrüstung"),
    ("Aufr stungskosten", "Aufrüstungskosten"),
    ("Aufr stung", "Aufrüstung"),
    ("Abk hlung", "Abkühlung"),
    ("Abk rzung", "Abkürzung"),
    ("Unterst tzung", "Unterstützung"),
    ("Unterst tzt", "Unterstützt"),
    ("unterst tzt", "unterstützt"),
    ("Luftst tzpunkt", "Luftstützpunkt"),
    ("Luftst tzp", "Luftstützp"),
    ("Marinest tzpunkt", "Marinestützpunkt"),
    ("St tzen", "Stützen"),
    ("st tzt", "stützt"),
    ("Gesch tz", "Geschütz"),
    ("gesch tz", "geschütz"),
    ("B rgerwehr", "Bürgerwehr"),
    ("B rger", "Bürger"),
    ("Anf hrerin", "Anführerin"),
    ("Anf hrers", "Anführers"),
    ("Anf hrer", "Anführer"),
    ("R ckgrat", "Rückgrat"),
    ("zur ck", "zurück"),
    ("Zur ck", "Zurück"),
    ("H gel", "Hügel"),
    ("h gel", "hügel"),
    ("H lle", "Hülle"),
    ("Fl sse", "Flüsse"),
    ("fl sse", "flüsse"),
    ("Gl ck", "Glück"),
    ("Men s", "Menüs"),
    ("bed rfniss", "bedürfniss"),
    ("Bed rfniss", "Bedürfniss"),
    ("S dwest", "Südwest"),
    ("S dost", "Südost"),
    ("S d", "Süd"),
    ("s d", "süd"),
    ("m ssen", "müssen"),
    ("M ssen", "Müssen"),
    ("k ndigen", "kündigen"),
    ("K ndigen", "Kündigen"),
    ("w nscht", "wünscht"),
    ("w rde", "würde"),
    ("W rde", "Würde"),
    ("w rden", "würden"),
    ("W sten", "Wüsten"),
    ("W ste", "Wüste"),
    ("fr her", "früher"),
    ("Fr her", "Früher"),
    ("daf r", "dafür"),
    ("Daf r", "Dafür"),
    ("hierf r", "hierfür"),
    ("wof r", "wofür"),
    ("F r", "Für"),
    ("f r", "für"),
    ("M nnlich", "Männlich"),    # wait - Männlich has ä! Moving...
    ("K ste", "Küste"),
    ("k ste", "küste"),
    ("sch tzen", "schützen"),      # context: "zu schützen" (protect)
    ("Sch tzen", "Schützen"),
    ("Kalk l", "Kalkül"),
    ("nat rlich", "natürlich"),
    ("Nat rlich", "Natürlich"),
    ("urspr nglich", "ursprünglich"),
    ("Urspr nglich", "Ursprünglich"),
    ("einzuf gen", "einzufügen"),
    ("hinzuf gen", "hinzufügen"),
    ("Gr nen", "Grünen"),
    ("gr nen", "grünen"),
    ("Gr n", "Grün"),
    ("gr n", "grün"),
    (" berrasch", "überrasch"),
    (" blich", "üblich"),
    (" ber", "über"),

    # === ö (0xF6) replacements ===
    ("Bev lkerungswachstum", "Bevölkerungswachstum"),
    ("Bev lkerungsgrenze", "Bevölkerungsgrenze"),
    ("Bev lkerung", "Bevölkerung"),
    ("bev lkerung", "bevölkerung"),
    ("Bewegungsm glichkeiten", "Bewegungsmöglichkeiten"),
    ("Sch pfungsgeh", "Schöpfungsgeheimnisse"[:len("Schöpfungsgeh")]),  # just the stem
    ("Sch pfung", "Schöpfung"),
    ("Handelsb rse", "Handelsbörse"),
    ("Bruth hle", "Bruthöhle"),
    ("St rsender", "Störsender"),
    ("Funkst rer", "Funkstörer"),
    ("Funkst rger", "Funkstörger"),   # might be Funkstörer
    ("H heneinheit", "Höheneinheit"),
    ("H here", "Höhere"),
    ("h here", "höhere"),
    ("h chsten", "höchsten"),
    ("h chste", "höchste"),
    ("H he", "Höhe"),
    ("h her", "höher"),
    ("R hre", "Röhre"),
    ("r hre", "röhre"),
    ("Zerst rung", "Zerstörung"),
    ("zerst rung", "zerstörung"),
    ("zerst rt", "zerstört"),
    ("Zerst rt", "Zerstört"),
    ("zerst r", "zerstör"),
    ("F rdert", "Fördert"),
    ("f rdert", "fördert"),
    ("F rder", "Förder"),
    ("f rder", "förder"),
    ("k nnen", "können"),
    ("K nnen", "Können"),
    ("k nnte", "könnte"),
    ("K nnte", "Könnte"),
    ("m glich", "möglich"),
    ("M glich", "Möglich"),
    ("Erh ht", "Erhöht"),
    ("erh ht", "erhöht"),
    ("Erh hte", "Erhöhte"),
    ("erh hte", "erhöhte"),
    ("Erh hter", "Erhöhter"),
    ("ben tigt", "benötigt"),
    ("Ben tigt", "Benötigt"),
    ("ben tigten", "benötigten"),
    ("Gel scht", "Gelöscht"),
    ("gel scht", "gelöscht"),
    ("L scht", "Löscht"),
    ("l scht", "löscht"),
    ("L sung", "Lösung"),
    ("Gew hnlich", "Gewöhnlich"),
    ("gew hnlich", "gewöhnlich"),
    ("ungew hnlich", "ungewöhnlich"),

    # === ä (0xE4) replacements ===
    ("Agressivit t", "Aggressivität"),
    ("Transportkapazit t", "Transportkapazität"),
    ("Singularit ts", "Singularitäts"),
    ("Singularit t", "Singularität"),
    ("Universit t", "Universität"),
    ("Eventualit t", "Eventualität"),
    ("Flexibilit t", "Flexibilität"),
    ("Immunit t", "Immunität"),
    ("Integrit t", "Integrität"),
    ("Intensit t", "Intensität"),
    ("Loyalit t", "Loyalität"),
    ("Mobilit t", "Mobilität"),
    ("Relativit t", "Relativität"),
    ("Kapazit t", "Kapazität"),
    ("Aktivit t", "Aktivität"),
    ("Priori t", "Priorität"),
    ("F higkeiten", "Fähigkeiten"),
    ("F higkeit", "Fähigkeit"),
    ("F higk", "Fähigk"),
    ("F hig", "Fähig"),
    ("f hig", "fähig"),
    ("Aufkl rung", "Aufklärung"),
    ("aufkl rung", "aufklärung"),
    ("Kampfverh ltnis", "Kampfverhältnis"),
    ("Verh ltnis", "Verhältnis"),
    ("verh ltnis", "verhältnis"),
    ("Gel nde", "Gelände"),
    ("gel nde", "gelände"),
    ("Erw rmungseffekt", "Erwärmungseffekt"),
    ("Erw rmung", "Erwärmung"),
    ("erw rmung", "erwärmung"),
    ("Bin rpanzerung", "Binärpanzerung"),
    ("Bin r", "Binär"),
    ("bin r", "binär"),
    ("Prim r", "Primär"),
    ("prim r", "primär"),
    ("milit risch", "militärisch"),
    ("Milit rwert", "Militärwert"),
    ("Milit r", "Militär"),
    ("M nnlich", "Männlich"),
    ("m nnlich", "männlich"),
    ("Aufst nde", "Aufstände"),
    ("aufst nde", "aufstände"),
    ("Str nde", "Strände"),
    ("str nde", "strände"),
    ("Beh lter", "Behälter"),
    ("beh lter", "behälter"),
    ("Sch den", "Schäden"),
    ("sch den", "schäden"),
    ("sch dlich", "schädlich"),
    ("St rkere", "Stärkere"),
    ("st rkere", "stärkere"),
    ("St rke", "Stärke"),
    ("st rke", "stärke"),
    ("Gl ubige", "Gläubige"),
    ("gl ubig", "gläubig"),
    ("Geh rtet", "Gehärtet"),
    ("geh rtet", "gehärtet"),
    ("zus tzliche", "zusätzliche"),
    ("Zus tzliche", "Zusätzliche"),
    ("zus tzlich", "zusätzlich"),
    ("zus tzl", "zusätzl"),
    ("Tr gerdeck", "Trägerdeck"),
    ("Tr ger", "Träger"),
    ("tr ger", "träger"),
    ("Erh lt", "Erhält"),
    ("erh lt", "erhält"),
    ("enth lt", "enthält"),
    ("Enth lt", "Enthält"),
    ("verh lt", "verhält"),
    ("erb rmlich", "erbärmlich"),
    ("Erb rmlich", "Erbärmlich"),
    ("gew hlten", "gewählten"),
    ("gew hlt", "gewählt"),
    ("ausgew hlt", "ausgewählt"),
    ("Ausgew hlt", "Ausgewählt"),
    ("w hlten", "wählten"),
    ("w hlen", "wählen"),
    ("W hlen", "Wählen"),
    ("H ufigkeit", "Häufigkeit"),
    ("h ufig", "häufig"),
    ("H ufig", "Häufig"),
    ("Eud monie", "Eudämonie"),
    ("eud monie", "eudämonie"),
    ("sp ter", "später"),
    ("Sp ter", "Später"),
    ("n chsten", "nächsten"),
    ("n chste", "nächste"),
    ("N chst", "Nächst"),
    ("K mpfe", "Kämpfe"),
    ("K mpfen", "Kämpfen"),
    ("k mpf", "kämpf"),
    ("J ger", "Jäger"),
    ("j ger", "jäger"),
    ("Ger ts", "Geräts"),
    ("Ger t", "Gerät"),
    ("ger t", "gerät"),
    ("Strafsph re", "Strafsphäre"),
    ("Sph re", "Sphäre"),
    ("sph re", "sphäre"),
    ("Atmosph re", "Atmosphäre"),
    ("atmosph re", "atmosphäre"),
    ("Z hler", "Zähler"),
    ("z hler", "zähler"),
    ("z hlt", "zählt"),
    ("Z hlt", "Zählt"),
    ("unz hlig", "unzählig"),
    ("erz hlt", "erzählt"),
    ("w hrend", "während"),
    ("W hrend", "Während"),
    ("w re", "wäre"),
    ("W re", "Wäre"),
    ("Ver nderung", "Veränderung"),
    ("ver nderung", "veränderung"),
    ("ver ndern", "verändern"),
    ("Ver ndern", "Verändern"),
    ("Kosten nderung", "Kostenänderung"),
    # Änderung at word start (Ä → space → " nderung")
    (" nderung", "Änderung"),
    (" ndern", "Ändern"),
    ("K ste", "Küste"),         # Küste has ü! Moving to ü section... actually already here, fixing:
    ("verl ngert", "verlängert"),
    ("Verl nger", "Verlänger"),
    ("empf nger", "empfänger"),
    ("Empf nger", "Empfänger"),
    ("Thermalb nder", "Thermalbänder"),
    ("b nder", "bänder"),
    ("B nder", "Bänder"),
    ("H lfte", "Hälfte"),
    ("h lfte", "hälfte"),
    ("sch n", "schön"),           # schön has ö
    ("Sch n", "Schön"),

    # === Ü (0xDC) at word start ===
    # When Ü starts a word, it becomes a space, making " ber" etc.
    # Handled above in ü section with " ber" → "über" etc.
    # Also: "UNTERST TZUNG" → "UNTERSTÜTZUNG" (in all-caps context)
    ("UNTERST TZUNG", "UNTERSTÜTZUNG"),
    ("VERST RKUNG", "VERSTÄRKUNG"),

    # === Ä (0xC4) at word start ===
    # " nderung" → "Änderung" - already handled above
    # " ra" → "Ära"
    (" ra", "Ära"),

    # === Ö (0xD6) at word start ===
    # " ko" → "Öko"
    (" kologie", "Ökologie"),
    (" konomie", "Ökonomie"),
]

# Remove the Bevölkerungswachstum duplicate (was in wrong section first)
# Clean up: remove duplicates, keeping first occurrence
seen = set()
clean_replacements = []
for old, new in REPLACEMENTS:
    if old not in seen:
        seen.add(old)
        clean_replacements.append((old, new))

# Fix: K ste should be Küste (ü), not in ä section
# Move to correct spot - it's already handled by general patterns


def fix_umlauts(text):
    """Apply all umlaut replacements to text."""
    # Sort by length of corrupted string (longest first) to avoid partial matches
    replacements = sorted(clean_replacements, key=lambda x: len(x[0]), reverse=True)

    for old, new in replacements:
        text = text.replace(old, new)

    # Handle remaining edge cases with regex
    # "da " → "daß" (ß was single byte, replaced with single space)
    # Match "da" + space + non-letter (space, comma, period, bracket, etc.)
    text = re.sub(r'\bda (?=[^a-zA-ZäöüÄÖÜß])', 'daß', text)
    text = re.sub(r'\bmu (?=[^a-zA-ZäöüÄÖÜß])', 'muß', text)
    text = re.sub(r'\bla (?=[^a-zA-ZäöüÄÖÜß])', 'laß', text)

    return text


def main():
    if not os.path.exists(ALPHAX):
        print(f"ERROR: {ALPHAX} not found")
        sys.exit(1)

    with open(ALPHAX, 'r', encoding='latin-1') as f:
        text = f.read()

    original = text
    text = fix_umlauts(text)

    # Count changes
    changes = 0
    for i, (a, b) in enumerate(zip(original, text)):
        if a != b:
            changes += 1

    # Check for remaining suspicious patterns (space between word chars)
    remaining = []
    for m in re.finditer(r'([A-Za-z]) ([a-z])', text):
        pos = m.start()
        # Get surrounding word
        start = pos
        while start > 0 and text[start-1].isalpha():
            start -= 1
        end = pos + 2
        while end < len(text) and text[end].isalpha():
            end += 1
        word = text[start:end]
        # Skip if it looks like legitimate two words
        left = text[start:pos+1].strip()
        right = text[pos+2:end].strip()
        if len(left) <= 3 or len(right) <= 2:
            ctx = text[max(0,pos-30):pos+30].replace('\n', '\\n')
            remaining.append((word, ctx))

    with open(ALPHAX, 'wb') as f:
        f.write(text.encode('cp1252', errors='replace'))

    print(f"Fixed {changes} characters in {ALPHAX}")
    print(f"Remaining suspicious patterns: {len(remaining)}")
    if remaining:
        print("\nFirst 30 remaining:")
        seen_words = set()
        count = 0
        for word, ctx in remaining:
            if word not in seen_words:
                seen_words.add(word)
                print(f"  {word:30s} | ...{ctx}...")
                count += 1
                if count >= 30:
                    break


if __name__ == "__main__":
    main()
