#!/usr/bin/env python3
"""Erzeugt die Abzeichen-Symbole aus der Namensliste.

    python3 -I tools/material_badges.py [--svg <ordner>]

Liest assets/material-symbols/auswahl.txt (ein Symbolname pro Zeile, '#' = Kommentar) und schreibt
  src/app/views/MaterialBadgeCount.h   die Anzahl (klein, für SourceBadge.h)
  src/app/views/MaterialBadgePaths.h   Namen und Umrisse (nur SourceBadge.cpp bindet ihn ein)

Die Umrisse kommen aus den exportierten SVGs der Material Symbols (Rounded, Stärke 700, gefüllt,
optische Größe 20), Standard: assets-src/material-symbols/svg/rounded-wght700-fill1-20px.
Wie man diesen Ordner erzeugt, steht in assets/material-symbols/README.md.

Die Reihenfolge der Liste ist die Bedeutung: Symbol i ist Abzeichen-Form 17 + i, und
~/.config/HZ-Drummer/abzeichen.json speichert diese Nummern. Nach der endgültigen Auswahl
deshalb nur noch anhängen.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIST = ROOT / "assets/material-symbols/auswahl.txt"
DEFAULT_SVG = ROOT / "assets-src/material-symbols/svg/rounded-wght700-fill1-20px"
OUT_COUNT = ROOT / "src/app/views/MaterialBadgeCount.h"
OUT_PATHS = ROOT / "src/app/views/MaterialBadgePaths.h"


def read_names(path):
    names = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            names.append(line)
    return names


def path_data(svg_file):
    text = svg_file.read_text(encoding="utf-8")
    viewbox = re.search(r'viewBox="([^"]+)"', text)
    paths = re.findall(r'<path[^>]* d="([^"]+)"', text)
    if viewbox is None or viewbox.group(1).split() != ["0", "-960", "960", "960"]:
        raise ValueError(f"{svg_file.name}: unerwartete viewBox")
    if len(paths) != 1 or not paths[0].strip():
        raise ValueError(f"{svg_file.name}: genau ein nicht leerer Pfad erwartet, gefunden {len(paths)}")
    if not re.fullmatch(r"[0-9MmLlHhVvCcSsQqTtAaZz .,eE+-]+", paths[0]):
        raise ValueError(f"{svg_file.name}: unerwartete Zeichen im Pfad")
    return paths[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--svg", type=Path, default=DEFAULT_SVG, help="Ordner mit <name>.svg")
    args = parser.parse_args()

    names = read_names(LIST)
    duplicates = sorted({n for n in names if names.count(n) > 1})
    if duplicates:
        sys.exit("Doppelt in der Liste: " + ", ".join(duplicates))
    missing = [n for n in names if not (args.svg / f"{n}.svg").exists()]
    if missing:
        sys.exit(f"Ohne SVG in {args.svg}: " + ", ".join(missing))

    data = [path_data(args.svg / f"{n}.svg") for n in names]
    n = len(names)

    OUT_COUNT.write_text(
        "// Erzeugt von tools/material_badges.py aus assets/material-symbols/auswahl.txt, nicht von Hand ändern.\n"
        "#pragma once\n\n"
        "namespace materialbadges\n{\n"
        f"    inline constexpr int count = {n};   // Material Symbols als Abzeichen-Formen 17 .. {16 + n}\n"
        "}\n", encoding="utf-8")

    lines = [
        "// Erzeugt von tools/material_badges.py aus assets/material-symbols/auswahl.txt, nicht von Hand ändern.",
        "// Material Symbols von Google, Apache-2.0, siehe assets/material-symbols/LICENSE.",
        "// Variante: Rounded, Stärke 700, gefüllt, optische Größe 20; viewBox 0 -960 960 960.",
        "#pragma once",
        "",
        "#include \"app/views/MaterialBadgeCount.h\"",
        "",
        "#include <array>",
        "",
        "namespace materialbadges",
        "{",
        f"    inline constexpr std::array<const char*, {n}> names {{{{",
    ]
    lines += [f'        "{name}",' for name in names]
    lines += ["    }};", "", f"    inline constexpr std::array<const char*, {n}> svgPaths {{{{"]
    lines += [f'        "{d}",   // {name}' for name, d in zip(names, data)]
    lines += ["    }};", "", "    static_assert (names.size() == (size_t) count && svgPaths.size() == (size_t) count);", "}", ""]
    OUT_PATHS.write_text("\n".join(lines), encoding="utf-8")
    print(f"{n} Symbole: {OUT_COUNT.relative_to(ROOT)}, {OUT_PATHS.relative_to(ROOT)} ({OUT_PATHS.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
