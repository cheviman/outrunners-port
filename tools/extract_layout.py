"""
Name:        extract_layout.py
Description: writes runtime/src/extract_layout.c, the pieces of the programs' data the game's first run
             takes out of the player's ROM set (runtime/src/extract.c), from the layouts extract_assets.py
             writes
Version:     0.1.0
Created:     29-10-2026
Modified:    29-10-2026
Author:      Sebastian Toyos

Usage: py tools/extract_layout.py [ASSETS]   (ASSETS: the folder extract_assets.py wrote, assets/ if none)

Positions and names only, as assets.json: every piece of ASSETS/data/layout.json and
ASSETS/sound/data/layout.json but the code (the C replaces it, and the game does not load it), with its
address, its size and its name, in the layouts' order. The table follows the layouts: when they change
(symbols.txt, sound_symbols.txt, src/ram.h, read_crossings.txt), extract_assets.py and then this write it
again, and orunners_assets_check compares the first run's folder with extract_assets.py's. The file is
written only when its table changes.
"""
import json
import re
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / "runtime" / "src" / "extract_layout.c"
REGIONS = [("program", "data", "the V70's program space"), ("sound", "sound/data", "the sound program's chip")]

HEADER = """/**
 *    Name:        extract_layout.c
 *    Description: the pieces of the programs' data the first run takes out of the player's ROM set
 *                 (extract.c), without their bytes
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    {modified}
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Written by tools/extract_layout.py from the layouts extract_assets.py writes: positions and names only,
 *    as assets.json, no byte of the ROM. Each region's pieces in the order of their addresses, the code left
 *    out (the C replaces it).
 */
"""


def is_code(piece_format):
    """Whether a piece of this format is code: the game does not load it (runtime/src/assets.c is_code)."""
    return piece_format == "kept" or piece_format.endswith("_code")


def table(region, folder, where, assets):
    layout = json.loads((assets / folder / "layout.json").read_text(encoding="ascii"))
    lines = [f"/* {where} ({folder}/layout.json) */", f"const extract_piece extract_{region}_pieces[] = {{"]
    for piece in layout["pieces"]:
        if is_code(piece.get("format", "")):
            continue
        if not re.fullmatch(r"[A-Za-z0-9_.]+", piece["name"]):
            raise SystemExit(f"{folder}/layout.json: a name this table cannot hold: {piece['name']!r}")
        lines.append(f"\t{{ 0x{int(piece['address'], 16):06X}u, 0x{piece['size']:X}u, \"{piece['name']}\" }},")
    lines.append("};")
    lines.append(f"const size_t extract_{region}_piece_count = sizeof(extract_{region}_pieces) / "
                 f"sizeof(extract_{region}_pieces[0]);")
    return "\n".join(lines) + "\n"


def main():
    assets = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets"
    body = "#include \"game/extract.h\"\n\n" + "\n".join(table(region, folder, where, assets)
                                                      for region, folder, where in REGIONS)
    old = TARGET.read_text(encoding="ascii") if TARGET.exists() else ""
    if old.split(" */\n", 1)[-1] == body:
        print(f"{TARGET.relative_to(ROOT)}: as it was")
        return 0
    TARGET.write_text(HEADER.format(modified=date.today().strftime("%d-%m-%Y")) + body, encoding="ascii",
                      newline="\n")
    print(f"{TARGET.relative_to(ROOT)}: written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
