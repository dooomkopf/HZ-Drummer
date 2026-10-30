#!/usr/bin/env python3
"""Übersetzt die Ausgabe von GpDrums.java in HZ-Drummer-Patterns (JSON) – mit Dynamik.

Aufruf:
  gp2json.py <gpdrums-ausgabe.txt> <zielordner> <titel> <id-präfix> [abschnitte]

  abschnitte: "1:Intro,25:Strophe,41:Strophe 2,..."  (Taktnummern im Song, d.h. nach Weglassen
              leerer Anfangstakte; ohne Angabe: Marker aus der Guitar-Pro-Datei, sonst 8-Takt-Teile)

Erzeugt:
  <zielordner>/<id-präfix>-NN-<name>.json   ein Pattern je Abschnitt
  <zielordner>/../songs/<id-präfix>.json    der ganze Song als Struktur (Abschnitte nacheinander)

Dynamik (nur wenn die Vorlage keine eigenen Lautstärken hat):
  - Snare: Backbeat (auf der Zählzeit, alleinstehend) kräftig; Fills mit Crescendo zum Taktende
  - Kick: Eins am stärksten, Zählzeiten stark, schnelle Folgen etwas leiser
  - Becken/HiHat/Ride: auf der Zählzeit lauter, dazwischen leiser; offene HiHat direkt nach Crash leise
  - Toms: Crescendo im Fill
"""

import json
import os
import re
import sys
from collections import OrderedDict
from fractions import Fraction
from math import gcd

TPQ = 960   # Ticks pro Viertel

VOICE = {
    35: "kick", 36: "kick", 38: "snare", 40: "snare", 39: "snare", 37: "snare.side",
    42: "hihat", 46: "hihat.open", 44: "hihat.pedal",
    49: "crash1", 57: "crash2", 52: "china", 55: "splash",
    51: "ride", 59: "ride", 53: "ride.bell",
    48: "tom1", 50: "tom1", 45: "tom2", 47: "tom2", 43: "tom3", 41: "tom4", 56: "cowbell",
}
FEET = {"kick", "hihat.pedal"}
CYMBALS = {"hihat", "hihat.open", "ride", "ride.bell", "cowbell"}
CRASHES = {"crash1", "crash2", "china", "splash"}


def parse(path):
    bars = OrderedDict()
    markers = {}
    unknown = {}
    for line in open(path, encoding="utf-8"):
        p = line.split()
        if not p:
            continue
        if p[0] == "BAR":
            bars[int(p[1])] = dict(length=int(p[3]), num=int(p[4]), den=int(p[5]), tempo=int(p[6]), hits=[])
        elif p[0] == "MARKER":
            markers[int(p[1])] = " ".join(p[2:])
        elif p[0] == "HIT":
            nr, off, note, vel, ghost, acc = (int(x) for x in p[1:7])
            if note in VOICE:
                bars[nr]["hits"].append(dict(off=off, voice=VOICE[note], vel=vel, ghost=ghost, acc=acc))
            else:
                unknown[note] = unknown.get(note, 0) + 1
    return bars, markers, unknown


def beat_ticks(bar):
    if bar["den"] >= 8 and bar["num"] % 3 == 0 and bar["num"] > 3:
        return TPQ * 4 // bar["den"] * 3          # zusammengesetzte Taktart: punktierte Zählzeit
    return TPQ * 4 // bar["den"]


def dynamics(bar):
    """Setzt 'v' (0..1) für jeden Schlag des Taktes nach Spielregeln."""
    length = bar["length"]
    beat = beat_ticks(bar)
    hits = bar["hits"]
    by_voice = {}
    for h in hits:
        by_voice.setdefault(h["voice"], []).append(h["off"])
    snares = sorted(by_voice.get("snare", []))
    kicks = sorted(by_voice.get("kick", []))
    crash_at = set()
    for v in CRASHES:
        crash_at.update(by_voice.get(v, []))

    for h in hits:
        off, voice = h["off"], h["voice"]
        on_beat = off % beat == 0
        on_half = off % (beat // 2 if beat % 2 == 0 else beat) == 0
        progress = off / length

        if h["ghost"]:
            v = 0.3
        elif voice == "snare":
            neighbours = [s for s in snares if s != off and abs(s - off) <= TPQ // 4]
            if on_beat and not neighbours:
                v = 1.0                                   # Backbeat
            elif neighbours:
                v = 0.68 + 0.27 * progress                # Fill: Crescendo
            else:
                v = 0.7
        elif voice == "kick":
            prev = [k for k in kicks if 0 < off - k <= TPQ // 8]
            v = 0.95 if off == 0 else 0.88 if on_beat else 0.8
            if prev:
                v -= 0.07                                  # schnelle Doppelschläge etwas leiser
        elif voice.startswith("tom"):
            v = 0.75 + 0.2 * progress
        elif voice in CRASHES:
            v = 1.0 if on_beat else 0.9
        elif voice in CYMBALS:
            v = 0.85 if on_beat else 0.7 if on_half else 0.58
            if voice == "hihat.open" and any(0 < off - c <= beat // 2 for c in crash_at):
                v = 0.45                                   # leise offene HiHat nach dem Crash
        else:
            v = 0.75
        if h["acc"]:
            v = min(1.0, v + 0.15)
        h["v"] = round(max(0.05, min(1.0, v)), 3)


def frac(ticks):
    f = Fraction(ticks, TPQ)
    return str(f.numerator) if f.denominator == 1 else f"{f.numerator}/{f.denominator}"


def grid_of(bar):
    g = bar["length"]
    for h in bar["hits"]:
        g = gcd(g, h["off"])
    g = gcd(g, TPQ // 4) if g else TPQ // 4
    g = max(g, TPQ // 16)
    f = Fraction(g, TPQ * 4)
    return f"{f.numerator}/{f.denominator}"


def make_pattern(pid, name, tags, tempo, bars, seed):
    hands, feet = [], []
    pos = 0
    eid = 1
    for b in bars:
        for h in sorted(b["hits"], key=lambda h: (h["off"], h["voice"])):
            ev = {"id": eid, "voice": h["voice"], "pos": frac(pos + h["off"]), "vel": h["v"]}
            (feet if h["voice"] in FEET else hands).append(ev)
            eid += 1
        pos += b["length"]

    return {
        "formatVersion": 1, "id": pid, "name": name, "revision": 1, "tags": tags,
        "tempo": {"default": tempo, "min": 0, "max": 0},
        "bars": [{"meter": f"{b['num']}/{b['den']}", "grid": grid_of(b)} for b in bars],
        "swing": {"subdivision": "1/8", "ratio": 0.5},
        "humanize": {"amount": 0.25, "seed": seed},
        "layers": [{"id": 1, "role": "hands", "events": hands}, {"id": 2, "role": "feet", "events": feet}],
        "origin": {"parentId": "", "parentRevision": 0, "operators": ["import:guitar-pro"], "seed": 0},
    }


def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower().replace("ä", "ae").replace("ö", "oe").replace("ü", "ue").replace("ß", "ss")).strip("-")


def main():
    src, outdir, title, prefix = sys.argv[1:5]
    section_arg = sys.argv[5] if len(sys.argv) > 5 else ""

    bars, markers, unknown = parse(src)
    nrs = list(bars)
    while nrs and not bars[nrs[0]]["hits"]:
        nrs.pop(0)
    while nrs and not bars[nrs[-1]]["hits"]:
        nrs.pop()
    first = nrs[0]
    song = [bars[n] for n in nrs]

    has_dynamics = len({h["vel"] for b in song for h in b["hits"]}) > 1
    for b in song:
        if has_dynamics:
            for h in b["hits"]:
                h["v"] = round(h["vel"] / 127, 3) if not h["ghost"] else 0.3
        else:
            dynamics(b)

    tempo = round(sum(b["tempo"] for b in song) / len(song))

    # Abschnitte: Angabe > Marker der Datei > 8-Takt-Teile (Taktnummern im Song, 1-basiert)
    sections = []
    if section_arg:
        for item in section_arg.split(","):
            n, name = item.split(":", 1)
            sections.append((int(n), name.strip()))
    elif markers:
        sections = [(n - first + 1, name) for n, name in sorted(markers.items()) if n >= first]
    else:
        sections = [(i + 1, f"Teil {i // 8 + 1}") for i in range(0, len(song), 8)]

    os.makedirs(outdir, exist_ok=True)
    seed = abs(hash(prefix)) % 100000 + 1

    written = []
    song_parts = []
    for k, (start, name) in enumerate(sections):
        end = sections[k + 1][0] - 1 if k + 1 < len(sections) else len(song)
        part = song[start - 1:end]
        if not part:
            continue
        pid = f"{prefix}-{k + 1:02d}-{slug(name)}"
        pat = make_pattern(pid, f"{title} – {k + 1:02d} {name} (T{start}-{end})", ["song", "abschnitt"], tempo, part, seed + k)
        path = os.path.join(outdir, f"{pid}.json")
        json.dump(pat, open(path, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        written.append((path, len(part)))
        song_parts.append({"pattern": pid, "label": name, "repeats": 1})

    # der ganze Song als Struktur: Abschnitte nacheinander (für den Song-Arranger)
    songs_dir = os.path.join(os.path.dirname(os.path.abspath(outdir)), "songs")
    os.makedirs(songs_dir, exist_ok=True)
    song_id = prefix.split(".", 1)[-1]
    song = {"formatVersion": 1, "id": song_id, "name": f"{title} (Song)", "tempo": tempo, "parts": song_parts}
    path = os.path.join(songs_dir, f"{song_id}.json")
    json.dump(song, open(path, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    written.append((path, len(song)))

    print(f"{len(song)} Takte (Vorlage {nrs[0]}–{nrs[-1]}), Tempo {tempo}, "
          f"{'Dynamik aus der Vorlage' if has_dynamics else 'Dynamik nach Spielregeln'}")
    for p, n in written:
        print(f"  {os.path.basename(p)}  ({n} Takte)")
    if unknown:
        print("Nicht zugeordnete MIDI-Noten (weggelassen):", unknown)


if __name__ == "__main__":
    main()
