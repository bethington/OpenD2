#!/usr/bin/env python3
"""Generate D2SData.hpp from d07riv's reference data + local PD2 ItemStatCost.txt."""
import urllib.request, re, json, os

# --- Fetch d07riv for itemBases and followstats ---
html = (
    urllib.request.urlopen(
        "https://raw.githubusercontent.com/d07RiV/d07riv.github.io/master/d2r.html"
    )
    .read()
    .decode("utf-8")
)

m = re.search(r"const followstats\s*=\s*\n?\[([^\]]+)\]", html)
follows = [int(x.strip()) for x in m.group(1).split(",") if x.strip()]

m = re.search(r"const itemBases\s*=\s*\n?(\{[^}]+\})", html)
bases = json.loads(m.group(1))

# Extract d07riv's vanilla savebits (used for actual save encoding)
m = re.search(r"const savebits\s*=\s*\n?\[([^\]]+)\]", html)
vanilla_savebits = [int(x.strip()) for x in m.group(1).split(",") if x.strip()]
print(f"d07riv vanilla savebits: {len(vanilla_savebits)} entries")

# --- Load PD2 ItemStatCost.txt (prefer local file, fallback to GitHub S8) ---
local_isc = (
    r"C:\Users\benam\source\cpp\OpenD2\build\Release\data\global\excel\ItemStatCost.txt"
)

if os.path.exists(local_isc):
    print(f"Using LOCAL PD2 ISC: {local_isc}")
    with open(local_isc, "r") as f:
        pd2_raw = f.read()
else:
    print("Local ISC not found, fetching PD2 S8 from GitHub...")
    pd2_url = "https://raw.githubusercontent.com/BetweenWalls/PD2-Converter/main/src/main/TEXT/pd2_s8/ItemStatCost.txt"
    pd2_raw = urllib.request.urlopen(pd2_url).read().decode("utf-8")

pd2_lines = pd2_raw.replace("\r\n", "\n").split("\n")
header = pd2_lines[0].split("\t")


# Find columns by name (case-insensitive)
def find_col(header, name):
    for i, h in enumerate(header):
        if h.strip().lower() == name.lower():
            return i
    return -1


id_col = find_col(header, "ID")
sb_col = find_col(header, "Save Bits")
sp_col = find_col(header, "Save Param Bits")
enc_col = find_col(header, "Encode")
print(
    f"ISC columns: ID={id_col}, SaveBits={sb_col}, SaveParam={sp_col}, Encode={enc_col}"
)

stat_data = {}  # id -> (savebits, saveparambits, encode)
for line in pd2_lines[1:]:
    cols = line.split("\t")
    if len(cols) <= max(id_col, sb_col):
        continue
    try:
        sid = int(cols[id_col].strip())
    except (ValueError, IndexError):
        continue
    sb = (
        int(cols[sb_col].strip())
        if sb_col >= 0 and sb_col < len(cols) and cols[sb_col].strip()
        else 0
    )
    sp = (
        int(cols[sp_col].strip())
        if sp_col >= 0 and sp_col < len(cols) and cols[sp_col].strip()
        else 0
    )
    enc = (
        int(cols[enc_col].strip())
        if enc_col >= 0 and enc_col < len(cols) and cols[enc_col].strip()
        else 0
    )
    stat_data[sid] = (sb, sp, enc)

max_id = max(stat_data.keys()) if stat_data else 358
num_stats = max_id + 1

# Build savebits: start with PD2 ISC, then override specific stats with d07riv
# vanilla values where saves are confirmed to use vanilla encoding widths.
# PD2 expanded SaveBits in the ISC TXT but saves still use vanilla D2 bit widths
# for these stats (confirmed by brute-force bit analysis and d07riv reference).
VANILLA_OVERRIDE_STATS = {
    17,
    18,  # enhanced damage % (vanilla=9, PD2 ISC=10)
    21,
    22,  # physical min/max damage (vanilla=6/7, PD2 ISC=10)
    23,
    24,  # secondary min/max damage (vanilla=6/7, PD2 ISC=10)
    48,
    49,  # fire min/max damage (vanilla=8/9, PD2 ISC=10) - confirmed by brute force
    50,  # lightning min damage (vanilla=6, PD2 ISC=10) - confirmed by brute force
    52,
    53,  # magic min/max damage (vanilla=8/9, PD2 ISC=10)
    57,
    58,  # poison min/max damage (vanilla=10, PD2 ISC=14)
    74,  # hp regen (vanilla=6, PD2 ISC=10)
    78,  # attacker takes damage (vanilla=7, PD2 ISC=11)
    120,  # damage target AC (vanilla=7, PD2 ISC=10)
    128,  # attacker takes light damage (vanilla=5, PD2 ISC=11)
}
savebits = []
for i in range(num_stats):
    pd2_val = stat_data.get(i, (0, 0, 0))[0]
    vanilla_val = vanilla_savebits[i] if i < len(vanilla_savebits) else 0
    if i in VANILLA_OVERRIDE_STATS and vanilla_val > 0:
        savebits.append(vanilla_val)
    else:
        savebits.append(pd2_val)

sparams = [stat_data.get(i, (0, 0, 0))[1] for i in range(num_stats)]
print(f"PD2 ItemStatCost: {len(stat_data)} stats parsed, max ID={max_id}")

# Show overrides applied
overrides = []
for i in VANILLA_OVERRIDE_STATS:
    if i < len(vanilla_savebits):
        pd2_val = stat_data.get(i, (0, 0, 0))[0]
        overrides.append(
            f"  stat {i}: PD2 ISC={pd2_val} -> vanilla={vanilla_savebits[i]}"
        )
if overrides:
    print(f"Applied {len(overrides)} vanilla SaveBits overrides:")
    for o in sorted(overrides):
        print(o)

# --- Load PD2 Armor/Weapons/Misc for complete item base flags ---
local_data_dir = r"C:\Users\benam\source\cpp\OpenD2\build\Release\data\global\excel"
pd2_base = "https://raw.githubusercontent.com/BetweenWalls/PD2-Converter/main/src/main/TEXT/pd2_s8/"


def fetch_pd2_table(name):
    """Load a PD2 data table (prefer local, fallback to GitHub)."""
    local = os.path.join(local_data_dir, name)
    if os.path.exists(local):
        print(f"  Using local {name}")
        with open(local, "r") as f:
            raw = f.read()
    else:
        print(f"  Fetching {name} from GitHub...")
        raw = urllib.request.urlopen(pd2_base + name).read().decode("utf-8")
    lines = raw.replace("\r\n", "\n").split("\n")
    hdr = lines[0].split("\t")
    rows = [l.split("\t") for l in lines[1:] if l.strip()]
    return hdr, rows


def get_col(hdr, name, default=-1):
    """Find column index by name (case-insensitive)."""
    for i, h in enumerate(hdr):
        if h.strip().lower() == name.lower():
            return i
    return default


def safe_int(s, default=0):
    try:
        return int(s.strip())
    except (ValueError, IndexError):
        return default


IBASE_QUANTITY = 0x01
IBASE_DURABILITY = 0x02
IBASE_DEFENSE = 0x04
IBASE_TOME = 0x08

pd2_item_flags = {}  # code -> flags

# Armor: has defense + durability
try:
    hdr, rows = fetch_pd2_table("Armor.txt")
    c_code = get_col(hdr, "code")
    c_minac = get_col(hdr, "minac")
    c_maxac = get_col(hdr, "maxac")
    c_dur = get_col(hdr, "durability")
    c_nodur = get_col(hdr, "nodurability")
    count = 0
    for cols in rows:
        if c_code < 0 or c_code >= len(cols) or not cols[c_code].strip():
            continue
        code = cols[c_code].strip()
        flags = 0
        has_def = (
            c_minac >= 0 and safe_int(cols[c_minac] if c_minac < len(cols) else "0") > 0
        ) or (
            c_maxac >= 0 and safe_int(cols[c_maxac] if c_maxac < len(cols) else "0") > 0
        )
        if has_def:
            flags |= IBASE_DEFENSE
        indestructible = (
            c_nodur >= 0 and c_nodur < len(cols) and safe_int(cols[c_nodur]) == 1
        )
        has_dur = c_dur >= 0 and c_dur < len(cols) and safe_int(cols[c_dur]) > 0
        if has_dur or has_def:  # armor always has durability fields
            flags |= IBASE_DURABILITY
        if indestructible:
            flags |= IBASE_DURABILITY  # still has max dur field (reads 0)
        if flags == 0:
            flags = IBASE_DEFENSE | IBASE_DURABILITY  # safe default for armor
        pd2_item_flags[code] = flags
        count += 1
    print(f"PD2 Armor.txt: {count} items")
except Exception as e:
    print(f"PD2 Armor.txt: ERROR {e}")

# Weapons: has durability, some stackable
try:
    hdr, rows = fetch_pd2_table("Weapons.txt")
    c_code = get_col(hdr, "code")
    c_dur = get_col(hdr, "durability")
    c_nodur = get_col(hdr, "nodurability")
    c_stack = get_col(hdr, "stackable")
    c_minstack = get_col(hdr, "minstack")
    count = 0
    for cols in rows:
        if c_code < 0 or c_code >= len(cols) or not cols[c_code].strip():
            continue
        code = cols[c_code].strip()
        flags = 0
        has_dur = c_dur >= 0 and c_dur < len(cols) and safe_int(cols[c_dur]) > 0
        indestructible = (
            c_nodur >= 0 and c_nodur < len(cols) and safe_int(cols[c_nodur]) == 1
        )
        stackable = (
            c_stack >= 0 and c_stack < len(cols) and safe_int(cols[c_stack]) == 1
        ) or (
            c_minstack >= 0
            and c_minstack < len(cols)
            and safe_int(cols[c_minstack]) > 0
        )
        if has_dur or indestructible:
            flags |= IBASE_DURABILITY
        if stackable:
            flags |= IBASE_QUANTITY
        if flags == 0 and not stackable:
            flags = IBASE_DURABILITY  # safe default for weapons
        pd2_item_flags[code] = flags
        count += 1
    print(f"PD2 Weapons.txt: {count} items")
except Exception as e:
    print(f"PD2 Weapons.txt: ERROR {e}")

# Misc: stackable items, tomes, etc.
try:
    hdr, rows = fetch_pd2_table("Misc.txt")
    c_code = get_col(hdr, "code")
    c_stack = get_col(hdr, "stackable")
    c_minstack = get_col(hdr, "minstack")
    c_name = get_col(hdr, "name")
    c_type = get_col(hdr, "type")
    count = 0
    for cols in rows:
        if c_code < 0 or c_code >= len(cols) or not cols[c_code].strip():
            continue
        code = cols[c_code].strip()
        flags = 0
        stackable = (
            c_stack >= 0 and c_stack < len(cols) and safe_int(cols[c_stack]) == 1
        ) or (
            c_minstack >= 0
            and c_minstack < len(cols)
            and safe_int(cols[c_minstack]) > 0
        )
        if stackable:
            flags |= IBASE_QUANTITY
        # Tomes
        if code in ("tbk", "ibk"):
            flags |= IBASE_TOME | IBASE_QUANTITY
        pd2_item_flags[code] = flags
        count += 1
    print(f"PD2 Misc.txt: {count} items")
except Exception as e:
    print(f"PD2 Misc.txt: ERROR {e}")

# Merge: d07riv bases + PD2 overrides (PD2 takes priority for PD2-only items,
# d07riv values are kept if item exists in both since d07riv is well-tested)
merged_bases = dict(bases)  # start with d07riv
added = 0
for code, flags in pd2_item_flags.items():
    if code not in merged_bases:
        merged_bases[code] = flags
        added += 1
print(
    f"Merged item bases: {len(merged_bases)} (d07riv={len(bases)} + PD2-only={added})"
)

outpath = r"C:\Users\benam\source\cpp\OpenD2\Tools\D2SaveParser\D2SData.hpp"

with open(outpath, "w") as f:
    f.write("#pragma once\n")
    f.write("#include <cstdint>\n")
    f.write("#include <cstring>\n\n")

    # Item quality enum
    f.write("enum D2SItemQuality : uint8_t\n{\n")
    f.write("\tQUALITY_NONE     = 0,\n")
    f.write("\tQUALITY_LOW      = 1,\n")
    f.write("\tQUALITY_NORMAL   = 2,\n")
    f.write("\tQUALITY_SUPERIOR = 3,\n")
    f.write("\tQUALITY_MAGIC    = 4,\n")
    f.write("\tQUALITY_SET      = 5,\n")
    f.write("\tQUALITY_RARE     = 6,\n")
    f.write("\tQUALITY_UNIQUE   = 7,\n")
    f.write("\tQUALITY_CRAFTED  = 8,\n")
    f.write("};\n\n")

    f.write("static const char* QualityNames[] = {\n")
    f.write('\t"???", "Low", "Normal", "Superior", "Magic",\n')
    f.write('\t"Set", "Rare", "Unique", "Crafted"\n')
    f.write("};\n\n")

    f.write("inline const char* GetQualityName(uint8_t q) {\n")
    f.write('\treturn (q <= 8) ? QualityNames[q] : "???";\n')
    f.write("}\n\n")

    # Item base flags
    f.write("// Item base type flags:\n")
    f.write("//   bit 0 (0x01): has quantity (9 bits)\n")
    f.write("//   bit 1 (0x02): has durability\n")
    f.write("//   bit 2 (0x04): has defense (11 bits)\n")
    f.write("//   bit 3 (0x08): has tome extra field (5 bits)\n")
    f.write("static constexpr uint8_t IBASE_QUANTITY   = 0x01;\n")
    f.write("static constexpr uint8_t IBASE_DURABILITY = 0x02;\n")
    f.write("static constexpr uint8_t IBASE_DEFENSE    = 0x04;\n")
    f.write("static constexpr uint8_t IBASE_TOME       = 0x08;\n\n")

    # Item base lookup table (sorted for binary search)
    f.write("struct ItemBaseEntry {\n")
    f.write("\tchar code[5]; // 4-char code + null (3-char codes are space-padded)\n")
    f.write("\tuint8_t flags;\n")
    f.write("};\n\n")

    sorted_bases = sorted(merged_bases.items())
    f.write("static const ItemBaseEntry g_ItemBases[] = {\n")
    for i, (code, flags) in enumerate(sorted_bases):
        # Space-pad to 4 chars (D2S format stores 4 bytes: 3-char items use ' ' as 4th)
        c = code.ljust(4)
        comma = "," if i < len(sorted_bases) - 1 else ""
        f.write('\t{{"{}", {}}}{}\n'.format(c, flags, comma))
    f.write("};\n")
    f.write(
        "static constexpr int NUM_ITEM_BASES = sizeof(g_ItemBases) / sizeof(g_ItemBases[0]);\n\n"
    )

    f.write("// Binary search for item base flags. Returns -1 if not found.\n")
    f.write(
        "// code must be a 4-char string (3-char items should have ' ' as 4th char).\n"
    )
    f.write("inline int GetItemBaseFlags(const char* code) {\n")
    f.write("\tint lo = 0, hi = NUM_ITEM_BASES - 1;\n")
    f.write("\twhile (lo <= hi) {\n")
    f.write("\t\tint mid = (lo + hi) / 2;\n")
    f.write("\t\tint cmp = strncmp(code, g_ItemBases[mid].code, 4);\n")
    f.write("\t\tif (cmp == 0) return g_ItemBases[mid].flags;\n")
    f.write("\t\tif (cmp < 0) hi = mid - 1;\n")
    f.write("\t\telse lo = mid + 1;\n")
    f.write("\t}\n")
    f.write("\treturn -1;\n")
    f.write("}\n\n")

    # savebits
    f.write("// Item stat bit widths (SaveBits from ItemStatCost.txt)\n")
    f.write("// Index = stat ID, value = number of value bits. 0 = invalid/unused.\n")
    f.write("static const uint8_t g_SaveBits[%d] = {\n" % len(savebits))
    for i in range(0, len(savebits), 20):
        chunk = savebits[i : i + 20]
        f.write("\t" + ",".join("%2d" % v for v in chunk))
        if i + 20 < len(savebits):
            f.write(",")
        f.write("\n")
    f.write("};\n")
    f.write("static constexpr int NUM_SAVE_BITS = %d;\n\n" % len(savebits))

    # saveparambits
    f.write("// Item stat parameter bit widths (SaveParamBits from ItemStatCost.txt)\n")
    f.write("static const uint8_t g_SaveParamBits[%d] = {\n" % len(sparams))
    for i in range(0, len(sparams), 20):
        chunk = sparams[i : i + 20]
        f.write("\t" + ",".join("%2d" % v for v in chunk))
        if i + 20 < len(sparams):
            f.write(",")
        f.write("\n")
    f.write("};\n\n")

    # followstats (padded to match savebits length)
    padded = follows + [0] * (num_stats - len(follows))
    f.write(
        "// Follow stats: number of additional stats that immediately follow this one\n"
    )
    f.write("static const uint8_t g_FollowStats[%d] = {\n" % len(padded))
    for i in range(0, len(padded), 20):
        chunk = padded[i : i + 20]
        f.write("\t" + ",".join("%d" % v for v in chunk))
        if i + 20 < len(padded):
            f.write(",")
        f.write("\n")
    f.write("};\n\n")

    # Helper
    f.write(
        "// Get total bits to read for a stat (param + value). Returns -1 if unknown.\n"
    )
    f.write("inline int GetItemStatTotalBits(uint16_t statId) {\n")
    f.write("\tif (statId >= NUM_SAVE_BITS) return -1;\n")
    f.write("\tif (g_SaveBits[statId] == 0) return -1;\n")
    f.write("\treturn g_SaveParamBits[statId] + g_SaveBits[statId];\n")
    f.write("}\n")

print("Generated", outpath)
print("  savebits:", len(savebits), "entries")
print("  saveparambits:", len(sparams), "entries")
print("  followstats:", len(follows), "entries (padded to", len(padded), ")")
print("  itemBases:", len(merged_bases), "entries (d07riv +", added, "PD2-only)")
