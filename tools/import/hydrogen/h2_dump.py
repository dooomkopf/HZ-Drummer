"""Liest Hydrogen-.h2song-Dateien (nur lesen) und schreibt alle Patterns als JSON.

Aufruf: python3 -I h2_dump.py <ausgabe.json> <datei.h2song> [...]
Je Pattern: datei, name, size_ticks, ticks_per_step, steps, tracks {Instrument: [Schritt ab 1]}, vel.
Hydrogen rechnet mit 48 Ticks pro Viertel; Raster 1/16 = 12 Ticks, Triolen-Achtel 1/12 = 16 Ticks.
"""
import json
import sys
import xml.etree.ElementTree as ET

RASTER = (12, 16, 6, 8, 4, 3, 1)  # 1/16, 1/12, 1/32, 1/24, 1/48, 1/64, Tick


def load(path):
    root = ET.parse(path).getroot()
    inst = {i.findtext('id'): i.findtext('name')
            for i in root.find('instrumentList').findall('instrument')}
    out = []
    for pat in root.find('patternList').findall('pattern'):
        size = int(pat.findtext('size'))
        notes = [(int(n.findtext('position')), inst.get(n.findtext('instrument'), '?'),
                  float(n.findtext('velocity')))
                 for n in pat.find('noteList').findall('note')]
        tps = next(r for r in RASTER if size % r == 0 and all(p % r == 0 for p, _, _ in notes))
        tracks, vel = {}, {}
        for p, name_i, v in sorted(notes):
            tracks.setdefault(name_i, []).append(p // tps + 1)
            vel.setdefault(name_i, []).append(v)
        out.append({'datei': path.rsplit('/', 1)[-1], 'name': pat.findtext('name'),
                    'size_ticks': size, 'ticks_per_step': tps, 'steps': size // tps,
                    'tracks': tracks, 'vel': vel, 'info': pat.findtext('info') or ''})
    return out


def main():
    res = []
    for p in sys.argv[2:]:
        res.extend(load(p))
    with open(sys.argv[1], 'w', encoding='utf-8') as f:
        json.dump(res, f, ensure_ascii=False, indent=1)
    print(len(res), 'Patterns')


if __name__ == '__main__':
    main()
