#!/usr/bin/env python3
"""Material Symbols aus der variablen Schrift als SVG exportieren (wie Googles symbols/web).

Aufruf: python3 -I tools/material_symbols_export.py <font.ttf> <codepoints> <outdir> FILL WGHT OPSZ GRAD [name ...]
Ohne Namen: alle Symbole der codepoints-Datei. Ausgabe: <outdir>/<name>.svg, viewBox 0 -960 960 960.
"""
import sys
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont


def main():
    font_path, cp_path, outdir = sys.argv[1], sys.argv[2], Path(sys.argv[3])
    fill, wght, opsz, grad = (float(v) for v in sys.argv[4:8])
    wanted = set(sys.argv[8:])
    outdir.mkdir(parents=True, exist_ok=True)

    font = TTFont(font_path)
    font = instantiateVariableFont(font, {"FILL": fill, "wght": wght, "opsz": opsz, "GRAD": grad}, inplace=True)
    upm = font["head"].unitsPerEm
    cmap = font.getBestCmap()
    glyph_set = font.getGlyphSet()
    size = int(opsz) if opsz in (20, 24, 40, 48) else 24

    names = []
    for line in Path(cp_path).read_text().splitlines():
        if not line.strip():
            continue
        name, hexcode = line.split()
        if wanted and name not in wanted:
            continue
        names.append((name, int(hexcode, 16)))

    written = 0
    for name, code in names:
        glyph_name = cmap.get(code)
        if glyph_name is None:
            continue
        pen = SVGPathPen(glyph_set, ntos=lambda v: f"{v:.0f}")
        # Schrift: y nach oben. Google-SVG: viewBox 0 -960 960 960, y nach unten gespiegelt.
        glyph_set[glyph_name].draw(TransformPen(pen, (1, 0, 0, -1, 0, 0)))
        d = pen.getCommands()
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" height="{size}" viewBox="0 -{upm} {upm} {upm}" width="{size}">'
               f'<path d="{d}"/></svg>')
        (outdir / f"{name}.svg").write_text(svg)
        written += 1
    print(f"{written} SVGs nach {outdir} (FILL {fill:g}, wght {wght:g}, opsz {opsz:g}, GRAD {grad:g})")


if __name__ == "__main__":
    main()
