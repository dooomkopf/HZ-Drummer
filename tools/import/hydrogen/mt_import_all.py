# MT Power komplett: Hi-Hat, Ride und China je Familie (8tel, 4tel, 16tel, 6/8, Shuffle) und die Tom-Grooves.
# Je Groove das Original (8 Takte) und eine Fassung mit Fill im 8. Takt. "Simple"-Figuren, deren Kick/Snare-Figur
# es in der Bibliothek oder den eigenen Bausteinen schon gibt, fallen weg. Schreibt nur in den Zielordner.
# Aufruf: python3 -I mt_import_all.py <songs-ordner> <zielordner> <bibliothek> <eigene-bausteine>
import collections
import glob
import hashlib
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
from fractions import Fraction as F

SONGS, OUT, LIBRARY, OWN = sys.argv[1:5]
TPQ = 48
GRIDS = [F(1, 16), F(1, 12), F(1, 24), F(1, 32), F(1, 48)]
GM = {36: "kick", 38: "snare", 37: "snare.side", 40: "snare.rim", 42: "hihat", 44: "hihat.half", 46: "hihat.open",
      65: "hihat.pedal", 49: "crash1", 57: "crash2", 51: "ride", 53: "ride.bell", 52: "china",
      48: "tom1", 45: "tom2", 43: "tom3", 41: "tom4"}
FAMILIES = [
    ("8th Closed Hi-Hat", "MT Power 8tel", "Hi-Hat"), ("8th Ride", "MT Power 8tel", "Ride"), ("8th China", "MT Power 8tel", "China"),
    ("4th Closed Hi-Hat", "MT Power 4tel", "Hi-Hat"), ("4th Ride", "MT Power 4tel", "Ride"), ("4th China", "MT Power 4tel", "China"),
    ("16th Closed Hi-Hat", "MT Power 16tel", "Hi-Hat"),
    ("8th Closed Hi-Hat 6_8", "MT Power 6/8", "Hi-Hat"), ("8th Ride 6_8", "MT Power 6/8", "Ride"), ("8th China 6_8", "MT Power 6/8", "China"),
    ("16th Closed Hi-Hat 6_8", "MT Power 6/8", "16tel Hi-Hat"),
    ("8th Closed Hi-Hat SHUFFLE", "MT Power Shuffle", "Hi-Hat"), ("8th Ride SHUFFLE", "MT Power Shuffle", "Ride"),
    ("8th China SHUFFLE", "MT Power Shuffle", "China"),
    ("4th Tom Grooves", "MT Power Toms", "4tel"), ("8th Tom Grooves", "MT Power Toms", "8tel"),
    ("8th _ 16th Tom Grooves", "MT Power Toms", "8tel und 16tel"), ("8th Tom Grooves 6_8", "MT Power Toms", "6/8"),
]
CREDIT = "MT Power Drum Kit 2 (Manda Audio), Groove-Bibliothek als Hydrogen-Songs (github.com/torgeir/drumgizmo-aasimonster-reaper), nur privat"
stats = collections.Counter()


def frac(x):
    return str(x.numerator) if x.denominator == 1 else f"{x.numerator}/{x.denominator}"


def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")


def why(e):
    """Warum eine Datei nicht lesbar ist, kurz und deutsch."""
    if isinstance(e, ET.ParseError):
        return "kein gültiges XML"
    if isinstance(e, OSError):
        return "Datei fehlt" if isinstance(e, FileNotFoundError) else f"nicht lesbar ({e.strerror})"
    return f"Aufbau unbekannt ({type(e).__name__})"


def fit_bar(bar, bar_ticks, tempo):
    """Das Raster eines Takts. Passen die Schläge in keines der Menü-Raster gemeinsam (eine 32tel vor einem
    Triolen-Lauf), gilt das Raster, in das die meisten passen; die übrigen kommen auf dessen nächsten Platz,
    der Abstand bleibt als Versatz in ms, dieselbe Stimme dort macht einen Flam."""
    steps = [(g, int(g * 4 * TPQ)) for g in GRIDS if bar_ticks % int(g * 4 * TPQ) == 0]
    positions = [p for (p, _) in bar]
    for g, step in steps:
        if all(p % step == 0 for p in positions):
            return g, bar
    g, step = max(steps, key=lambda gs: (sum(1 for p in positions if p % gs[1] == 0), gs[0]))
    out = {}
    for (p, v), (vel, lag, flam) in sorted(bar.items(), key=lambda kv: (kv[0][0] % step != 0, kv[0][0])):
        if p % step:
            snapped = min(round(p / step) * step, bar_ticks - step)
            lag = max(-100.0, min(100.0, round(lag + (p - snapped) / TPQ * 60000.0 / tempo, 1)))
            stats["aufs Takt-Raster mit Versatz"] += 1
            p = snapped
        if (p, v) in out:
            out[(p, v)] = [max(vel, out[(p, v)][0]), out[(p, v)][1], True]
        else:
            out[(p, v)] = [vel, lag, flam]
    return g, out


def bars_of(pattern, bar_ticks, tempo):
    """Takte eines Hydrogen-Patterns: je Takt {(Position in Ticks, Stimme): [Lautstärke, Versatz ms, Flam]}."""
    n = int(pattern.findtext("size")) // bar_ticks
    bars = [dict() for _ in range(n)]
    notes = []
    for note in pattern.iter("note"):
        voice = GM.get(int(note.findtext("instrument")) + 36)
        pos = int(note.findtext("position"))
        if voice is None or pos >= n * bar_ticks:   # unbekannt, oder die "1" des nächsten Takts am Ende jedes Grooves
            stats["Note weggelassen"] += 1
            continue
        notes.append((pos, voice, max(0.0, min(1.0, float(note.findtext("velocity") or 0.8)))))
    for pos, voice, vel in sorted(notes, key=lambda x: (x[0] % 4 != 0 and x[0] % 6 != 0, x[0])):
        lag = 0.0
        if pos % 4 and pos % 6:   # ein Tick neben der 16tel: auf die 16tel, Abstand als Versatz
            snapped = min(round(pos / 12) * 12, n * bar_ticks - 12)
            if voice == "tom4" and abs(pos - snapped) == 1:
                stats["Stand-Tom aufs Raster (Fehler der Quelle)"] += 1   # nur sie liegt so oft einen Tick daneben
            else:
                lag = max(-100.0, min(100.0, round((pos - snapped) / TPQ * 60000.0 / tempo, 1)))
                stats["auf die 16tel mit Versatz"] += 1
            pos = snapped
        key = (pos % bar_ticks, voice)
        bar = bars[pos // bar_ticks]
        if key in bar:
            bar[key] = [max(vel, bar[key][0]), bar[key][1], bar[key][2] or lag != 0.0]
        else:
            bar[key] = [vel, lag, False]
    return bars


def core(bar):
    return frozenset((F(p, TPQ), v) for (p, v) in bar if v in ("kick", "snare"))


def fill_score(bar):
    return sum(1 for (_, v) in bar if v.startswith("tom") or v == "snare") + len({v for (_, v) in bar})


def pattern_json(pid, name, tags, tempo, meter, bar_ticks, bars, origin):
    length = F(bar_ticks, TPQ)
    out_bars, hands, feet, eid = [], [], [], 1
    for i, bar in enumerate(bars):
        grid, bar = fit_bar(bar, bar_ticks, tempo)
        out_bars.append({"meter": meter, "grid": frac(grid)})
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


# Kick/Snare-Figuren, die es schon gibt (Bibliothek und eigene Bausteine, ohne frühere MT-Power-Importe)
known = set()
for path in glob.glob(os.path.join(LIBRARY, "*.json")) + glob.glob(os.path.join(OWN, "*.json")):
    if os.path.basename(path).startswith("user.mtpower-"):
        continue
    try:   # a damaged or foreign file among the own Bausteine only lacks in this comparison
        d = json.load(open(path, encoding="utf-8"))
        start = F(0)
        for b in d["bars"]:
            num, den = (int(x) for x in re.match(r"(\d+)/(\d+)", b["meter"]).groups())
            length = F(num * 4, den)
            hits = frozenset((F(e["pos"]) - start, e["voice"]) for l in d["layers"] for e in l["events"]
                             if e["voice"] in ("kick", "snare") and start <= F(e["pos"]) < start + length)
            known.add((f"{num}/{den}", hits))
            start += length
    except (OSError, ValueError, KeyError, TypeError, AttributeError, ZeroDivisionError):
        stats["vorhandene Bausteine nicht lesbar (für den Vergleich übergangen)"] += 1

os.makedirs(os.path.join(OUT, "patterns"), exist_ok=True)
skipped_simple = []
for fname, band, group in FAMILIES:
    todo = []   # written only when the whole file could be read
    try:
        root = ET.parse(os.path.join(SONGS, fname + ".h2song")).getroot()
        tempo = min(320.0, max(30.0, float(root.findtext("bpm") or 115)))   # the tempo HZ-Drummer can play
        patterns = list(root.find("patternList"))
        for groove in (p for p in patterns if int(p.findtext("size")) in (1536, 1152)):
            name = groove.findtext("name")
            g = name.split()[0]
            bar_ticks = int(groove.findtext("size")) // 8
            meter = "4/4" if bar_ticks == 192 else "6/8"
            gbars = bars_of(groove, bar_ticks, tempo)
            gtype = re.sub(r"^G\d+ .*?\d+ \d+ ?", "", name).strip() or "Groove"
            gtype = "Shuffle" if gtype.upper() == "SHUFFLE" else gtype
            figure = collections.Counter(core(b) for b in gbars).most_common(1)[0][0]
            if gtype == "Simple" and (meter, figure) in known:
                skipped_simple.append(f"{band} {group} {g}")
                stats["Simple, schon vorhanden"] += 1
                continue
            fills = [p for p in patterns if re.match(rf"^{re.escape(g)} F\d+ ", p.findtext("name") or "")
                     and int(p.findtext("size")) == bar_ticks]
            tags = ["metal", "rock", "mt-power", f"band:{band}", f"song:{group}"]
            base = f"user.mtpower-{slug(band.replace('MT Power ', ''))}-{slug(group)}-{g.lower()}"
            versions = [(base, f"{band} – {group} – {g} {gtype}", gbars, name)]
            if fills:
                best = max(fills, key=lambda p: fill_score(bars_of(p, bar_ticks, tempo)[0]))
                versions.append((base + "-fill", f"{band} – {group} – {g} {gtype} mit Fill",
                                 gbars[:7] + bars_of(best, bar_ticks, tempo), name + " + " + best.findtext("name")))
            for pid, title, bars, note in versions:
                todo.append(pattern_json(pid, title, tags, tempo, meter, bar_ticks, bars, ["hydrogen-import", CREDIT, note]))
    except (OSError, ET.ParseError, ValueError, KeyError, TypeError, AttributeError, IndexError) as e:   # broken or odd
        print(f"Übersprungen: {fname}.h2song ({why(e)})")
        continue
    for pattern in todo:
        if not any(layer["events"] for layer in pattern["layers"]):
            stats["ohne Schlag, nicht geschrieben"] += 1
            continue
        with open(os.path.join(OUT, "patterns", pattern["id"] + ".json"), "w", encoding="utf-8") as f:
            json.dump(pattern, f, ensure_ascii=False, indent=2)
        stats[f"{band} / {group}"] += 1
        stats["Bausteine"] += 1

for k, v in sorted(stats.items()):
    print(f"{k:40} {v}")
print("weggelassen (Simple, schon vorhanden):", ", ".join(skipped_simple) or "keine")
