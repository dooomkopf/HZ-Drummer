# MT-Power-Grooves (Hydrogen, Instrument-ID + 36 = GM-Note) -> HZ-Drummer-Bausteine: je Groove das Original
# (8 Takte) und eine Fassung mit Fill im 8. Takt. Schreibt nur in den Zielordner (patterns/).
# Aufruf: python3 -I mt_import.py <songs-ordner> <zielordner> <datei:gruppe:Gnn,Gnn> ...
#   z. B.   "8th Closed Hi-Hat:8tel Hi-Hat:G13,G20"
import collections
import hashlib
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
from fractions import Fraction as F

SONGS, OUT = sys.argv[1], sys.argv[2]
TPQ = 48
GRIDS = [F(1, 16), F(1, 12), F(1, 24), F(1, 32), F(1, 48)]
GM = {36: "kick", 38: "snare", 37: "snare.side", 40: "snare.rim", 42: "hihat", 44: "hihat.half", 46: "hihat.open",
      65: "hihat.pedal", 49: "crash1", 57: "crash2", 51: "ride", 53: "ride.bell", 52: "china",
      48: "tom1", 45: "tom2", 43: "tom3", 41: "tom4"}
skipped = collections.Counter()


def frac(x):
    return str(x.numerator) if x.denominator == 1 else f"{x.numerator}/{x.denominator}"


def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")


def grid_for(length, rel):
    for g in GRIDS:
        step = g * 4
        if (length / step).denominator == 1 and all((p / step).denominator == 1 for p in rel):
            return g
    raise ValueError("kein Raster passt")


def bars_of(pattern, bar_ticks, tempo):
    """Ein Hydrogen-Pattern als Liste von Takten: je Takt {(Position in Ticks, Stimme): [Lautstärke, Versatz ms, Flam]}.
    Noten neben jedem Raster (ein Tick neben der 16tel) kommen auf die 16tel, der Abstand bleibt als Versatz in ms;
    trifft das dieselbe Stimme, wird ein Flam daraus."""
    n = int(pattern.findtext("size")) // bar_ticks
    bars = [dict() for _ in range(n)]
    notes = []
    for note in pattern.iter("note"):
        voice = GM.get(int(note.findtext("instrument")) + 36)
        if voice is None:
            skipped[int(note.findtext("instrument")) + 36] += 1
            continue
        pos = int(note.findtext("position"))
        if pos >= n * bar_ticks:   # the export closes every groove with the "1" of the next bar: the loop repeats it anyway
            skipped["Schlag hinter dem Ende"] += 1
            continue
        notes.append((pos, voice, max(0.0, min(1.0, float(note.findtext("velocity") or 0.8)))))
    for pos, voice, vel in sorted(notes, key=lambda x: (x[0] % 4 != 0 and x[0] % 6 != 0, x[0])):
        lag = 0.0
        if pos % 4 and pos % 6:
            snapped = min(round(pos / 12) * 12, n * bar_ticks - 12)
            lag = round((pos - snapped) / TPQ * 60000.0 / tempo, 1)
            skipped["auf die 16tel mit Versatz"] += 1
            pos = snapped
        key = (pos % bar_ticks, voice)
        bar = bars[pos // bar_ticks]
        if key in bar:
            bar[key] = [max(vel, bar[key][0]), bar[key][1], bar[key][2] or lag != 0.0]
        else:
            bar[key] = [vel, lag, False]
    return bars


def fill_score(bar):
    return sum(1 for (_, v) in bar if v.startswith("tom") or v == "snare") + len({v for (_, v) in bar})


def pattern_json(pid, name, tags, tempo, meter, bar_ticks, bars, origin):
    length = F(bar_ticks, TPQ)
    out_bars, hands, feet, eid = [], [], [], 1
    for i, bar in enumerate(bars):
        rel = [F(p, TPQ) for (p, _) in bar]
        out_bars.append({"meter": meter, "grid": frac(grid_for(length, rel))})
        for (p, voice), (vel, lag, flam) in sorted(bar.items()):
            ev = {"id": eid, "voice": voice, "pos": frac(i * length + F(p, TPQ)), "vel": round(vel, 3)}
            if flam:
                ev["flam"] = True
            if lag:
                ev["lag"] = lag
            (feet if voice in ("kick", "hihat.pedal") else hands).append(ev)
            eid += 1
    return {"formatVersion": 1, "id": pid, "name": name, "revision": 1, "tags": tags,
            "tempo": {"default": tempo, "min": 0, "max": 0}, "bars": out_bars,
            "swing": {"subdivision": "1/8", "ratio": 0.5},
            "humanize": {"amount": 0.25, "seed": int(hashlib.md5(pid.encode()).hexdigest()[:8], 16)},
            "layers": [{"id": 1, "role": "hands", "events": hands}, {"id": 2, "role": "feet", "events": feet}],
            "origin": {"parentId": "", "parentRevision": 0, "operators": origin, "seed": 0}}


os.makedirs(os.path.join(OUT, "patterns"), exist_ok=True)
written = 0
for spec in sys.argv[3:]:
    fname, group, wanted = spec.split(":")
    root = ET.parse(os.path.join(SONGS, fname + ".h2song")).getroot()
    tempo = float(root.findtext("bpm") or 115)
    patterns = list(root.find("patternList"))
    for g in wanted.split(","):
        groove = next(p for p in patterns if p.findtext("name").startswith(g + " ") and int(p.findtext("size")) in (1536, 1152))
        bar_ticks = int(groove.findtext("size")) // 8
        meter = "4/4" if bar_ticks == 192 else "6/8"
        gbars = bars_of(groove, bar_ticks, tempo)
        gtype = re.sub(r"^G\d+ .*?\d+ \d+ ?", "", groove.findtext("name")).strip() or "Groove"
        fills = [p for p in patterns if re.match(rf"^{g} F\d+ ", p.findtext("name")) and int(p.findtext("size")) == bar_ticks]
        best = max(fills, key=lambda p: fill_score(bars_of(p, bar_ticks, tempo)[0]))
        tags = ["rock", "metal", "mt-power", "band:MT Power", f"song:{group}"]
        base = f"user.mtpower-{slug(group)}-{g.lower()}"
        credit = "MT Power Drum Kit 2 (Manda Audio), Groove-Bibliothek als Hydrogen-Songs (github.com/torgeir/drumgizmo-aasimonster-reaper), nur privat"
        for pid, name, bars, note in [
            (base, f"MT Power – {group} – {g} {gtype}", gbars, groove.findtext("name")),
            (base + "-fill", f"MT Power – {group} – {g} {gtype} mit Fill", gbars[:7] + bars_of(best, bar_ticks, tempo), groove.findtext("name") + " + " + best.findtext("name"))]:
            with open(os.path.join(OUT, "patterns", pid + ".json"), "w", encoding="utf-8") as f:
                json.dump(pattern_json(pid, name, tags, tempo, meter, bar_ticks, bars, ["hydrogen-import", credit, note]),
                          f, ensure_ascii=False, indent=2)
            written += 1
            print(f"{pid:42} {name}  [{note}]")
print("geschrieben:", written, "| übersprungene Noten:", dict(skipped) or "keine")
