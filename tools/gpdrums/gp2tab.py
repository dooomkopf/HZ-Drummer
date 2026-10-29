#!/usr/bin/env python3
"""Übersetzt die Ausgabe von GpDrums.java (Schlagzeugspur einer Guitar-Pro-Datei) in HZ-Drummer-Tabs.

Aufruf: gp2tab.py <gpdrums-ausgabe.txt> <ausgabe.tab> <titel> <id-präfix> [phrasenlänge=4]

Erzeugt:
  - den ganzen Song als ein Pattern (alle Takte, Taktwechsel bleiben erhalten)
  - die Teile: Phrasen gleicher Taktart (Standard 4 Takte); identische Phrasen nur einmal
  - als Kommentar die Reihenfolge der Teile im Song (Grundlage für den Song-Arranger)
"""

import sys
from collections import OrderedDict
from math import gcd

TICKS_PER_QUARTER = 960

# General-MIDI-Schlagzeug -> (Tab-Spur, Zeichen bzw. None = nach Lautstärke)
GM = {
    35: ("bd", None), 36: ("bd", None),
    38: ("sn", None), 40: ("sn", None), 37: ("ss", None), 39: ("sn", None),
    42: ("hh", None), 46: ("hh", "o"), 44: ("hp", None),
    49: ("cr", None), 57: ("c2", None), 52: ("ch", None), 55: ("sp", None),
    51: ("rd", None), 59: ("rd", None), 53: ("rb", None),
    48: ("t1", None), 50: ("t1", None), 45: ("t2", None), 47: ("t2", None), 43: ("t3", None), 41: ("t4", None),
    56: ("cb", None),
}
LANE_ORDER = ["cr", "c2", "ch", "sp", "rd", "rb", "hh", "cb", "t1", "t2", "t3", "t4", "sn", "sr", "ss", "hp", "bd"]


def parse(path):
    bars = OrderedDict()   # nr -> dict(len, num, den, tempo, hits=[(offset, note, vel, ghost, accent)])
    unknown = {}
    for line in open(path, encoding="utf-8"):
        p = line.split()
        if not p:
            continue
        if p[0] == "BAR":
            nr = int(p[1])
            bars[nr] = dict(length=int(p[3]), num=int(p[4]), den=int(p[5]), tempo=int(p[6]), hits=[])
        elif p[0] == "HIT":
            nr, off, note, vel, ghost, acc = (int(x) for x in p[1:7])
            if note not in GM:
                unknown[note] = unknown.get(note, 0) + 1
                continue
            bars[nr]["hits"].append((off, note, vel, ghost, acc))
    return bars, unknown


def bar_text(bar, lanes, all_vel_equal):
    """Liefert {spur: zeichenkette} und Raster (zähler, nenner) für einen Takt."""
    length = bar["length"]
    g = length
    for off, *_ in bar["hits"]:
        g = gcd(g, off)
    g = gcd(g, TICKS_PER_QUARTER // 4) if g else TICKS_PER_QUARTER // 4   # mindestens 16tel-Raster (bequem im Editor)
    g = max(g, TICKS_PER_QUARTER // 16)                                       # nicht feiner als 1/64
    steps = length // g
    whole = TICKS_PER_QUARTER * 4
    gn, gd = g // gcd(g, whole), whole // gcd(g, whole)

    rows = {lane: ["."] * steps for lane in lanes}
    for off, note, vel, ghost, acc in bar["hits"]:
        lane, fixed = GM[note]
        s = off // g
        if ghost:
            ch = "g"
        elif acc or (not all_vel_equal and vel >= 110):
            ch = "X"
        elif not all_vel_equal and vel < 50:
            ch = "g"
        else:
            ch = "x"
        if fixed == "o":
            ch = "O" if ch == "X" else "o"
        if rows[lane][s] in ("X", "O"):
            continue
        rows[lane][s] = ch
    return rows, (gn, gd)


def pattern_text(name, pid, tags, tempo, bar_list, all_vel_equal):
    lanes = sorted({GM[n][0] for b in bar_list for _, n, *_ in b["hits"]}, key=LANE_ORDER.index) or ["bd"]
    meters, grids, rows_per_bar = [], [], []
    for b in bar_list:
        rows, grid = bar_text(b, lanes, all_vel_equal)
        meters.append(f"{b['num']}/{b['den']}")
        grids.append(f"{grid[0]}/{grid[1]}")
        rows_per_bar.append(rows)

    out = [f"name:  {name}", f"id:    {pid}", f"tags:  {tags}", f"tempo: {tempo}",
           f"meter: {', '.join(meters)}", f"grid:  {', '.join(grids)}", "humanize: 0.25"]
    width = max(len(l) for l in lanes)
    # lange Zeilen: je 4 Takte eine Zeile pro Spur
    for start in range(0, len(bar_list), 4):
        for lane in lanes:
            chunks = [" ".join(rows_per_bar[i][lane]) for i in range(start, min(start + 4, len(bar_list)))]
            out.append(f"{lane.ljust(width)} | " + " | ".join(chunks) + " |")
        out.append("")
    return "\n".join(out).rstrip() + "\n"


def signature(bars):
    return tuple((b["num"], b["den"], b["length"], tuple(sorted(b["hits"]))) for b in bars)


def main():
    src, dst, title, prefix = sys.argv[1:5]
    phrase = int(sys.argv[5]) if len(sys.argv) > 5 else 4

    bars, unknown = parse(src)
    nrs = [n for n in bars]
    # leere Takte am Anfang/Ende (Einzähler, Ausklang) weglassen
    while nrs and not bars[nrs[0]]["hits"]:
        nrs.pop(0)
    while nrs and not bars[nrs[-1]]["hits"]:
        nrs.pop()
    song = [bars[n] for n in nrs]
    tempo = round(sum(b["tempo"] for b in song) / max(1, len(song)))
    vels = {v for b in song for _, _, v, _, _ in b["hits"]}
    all_vel_equal = len(vels) <= 1

    parts = []
    # Phrasen: Läufe gleicher Taktart, darin Stücke à 'phrase' Takte
    i = 0
    while i < len(song):
        j = i
        while j < len(song) and (song[j]["num"], song[j]["den"]) == (song[i]["num"], song[i]["den"]) and j - i < phrase:
            j += 1
        parts.append(song[i:j])
        i = j

    unique = OrderedDict()
    order = []
    for p in parts:
        sig = signature(p)
        if sig not in unique:
            unique[sig] = (chr(ord("A") + len(unique)) if len(unique) < 26 else f"Z{len(unique)}", p)
        order.append(unique[sig][0])

    texts = [
        f"# {title} – aus Guitar Pro übersetzt (gp2tab.py); nur für den Eigengebrauch.",
        f"# Takte {nrs[0]}–{nrs[-1]} der Vorlage, {len(song)} Takte, Tempo {tempo} BPM.",
        f"# Reihenfolge der Teile ({phrase}-Takt-Phrasen): {' '.join(order)}",
    ]
    if unknown:
        texts.append(f"# Nicht zugeordnete MIDI-Noten (weggelassen): {unknown}")
    texts.append("")
    texts.append(pattern_text(f"{title} – ganzer Song", f"{prefix}-song", "song, ganzer-song", tempo, song, all_vel_equal))
    for letter, p in unique.values():
        meters = sorted({f"{b['num']}/{b['den']}" for b in p})
        texts.append("---")
        texts.append(pattern_text(f"{title} – Teil {letter} ({', '.join(meters)})", f"{prefix}-teil-{letter.lower()}",
                                  "song, teil", tempo, p, all_vel_equal))

    open(dst, "w", encoding="utf-8").write("\n".join(texts))
    print(f"{len(song)} Takte, {len(unique)} verschiedene Teile, Reihenfolge: {' '.join(order)}")
    if unknown:
        print("Nicht zugeordnet:", unknown)


if __name__ == "__main__":
    main()
