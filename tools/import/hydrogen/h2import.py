# Hydrogen-Songs -> HZ-Drummer: R.P. Bardet (je Stil ein Ordner, dazu 4-Takt-Fassungen) und Band-Songs
# (Bausteine je Takt-Inhalt plus ein Arrangement). Schreibt nur in den Zielordner (patterns/, songs/).
# Aufruf: python3 -I h2import.py <hydrogen-repo> <zielordner> [vorhandener-patterns-ordner]
import hashlib
import json
import os
import re
import sys
import collections
import xml.etree.ElementTree as ET
from fractions import Fraction as F

SRC, OUT = sys.argv[1], sys.argv[2]
EXISTING = sys.argv[3] if len(sys.argv) > 3 else None
TPQ = 48                                              # Hydrogen: Ticks je Viertel
GRIDS = [F(1, 16), F(1, 12), F(1, 24), F(1, 32), F(1, 48)]   # Vorzug: 16 Schritte wie die Drum-Machine, sonst Triolen
mapping = collections.Counter()
unmapped = collections.Counter()
notes_log = []


def voice_of(name):
    n = name.lower()
    if "cowbell" in n:
        return "cowbell"
    if "kick" in n or "bass drum" in n:
        return "kick"
    if "clap" in n:
        return "snare.side"                           # kein Handclap im Kit: als Sidestick
    if "snare" in n:
        if "rim" in n:
            return "snare.rim"
        if "stick" in n or "side" in n:
            return "snare.side"
        return "snare"
    if "stick" in n or n.strip() == "rim":
        return "snare.side"
    if "hat" in n or "hh" in n:
        if "pedal" in n or "foot" in n:
            return "hihat.pedal"
        if "semi" in n or "half" in n:
            return "hihat.half"
        if "open" in n:
            return "hihat.open"
        return "hihat"
    if "bell" in n:
        return "ride.bell"
    if "ride" in n:
        return "ride"
    if "china" in n:
        return "china"
    if "splash" in n:
        return "splash"
    if "crash" in n:
        return "crash2" if "2" in n else "crash1"
    if "tom" in n:
        if "floor" in n or "low" in n:
            return "tom3"
        if "1" in n or "hi" in n:
            return "tom1"
        if "2" in n or "mid" in n:
            return "tom2"
        if "3" in n:
            return "tom3"
        if "4" in n:
            return "tom4"
        return "tom2"
    return None


def slug(s):
    s = s.lower().replace("&", "-").replace("/", "")
    return re.sub(r"[^a-z0-9]+", "-", s).strip("-")


def frac(x):
    return str(x.numerator) if x.denominator == 1 else f"{x.numerator}/{x.denominator}"


def why(e):
    """Warum eine Datei nicht lesbar ist, kurz und deutsch."""
    if isinstance(e, ET.ParseError):
        return "kein gültiges XML"
    if isinstance(e, OSError):
        return "Datei fehlt" if isinstance(e, FileNotFoundError) else f"nicht lesbar ({e.strerror})"
    if isinstance(e, ValueError) and not str(e).startswith(("could not convert", "invalid literal")):
        return str(e)   # one of ours
    return f"Aufbau unbekannt ({type(e).__name__})"


def seed_of(pid):
    return int(hashlib.md5(pid.encode()).hexdigest()[:8], 16)


def read_song(path):
    root = ET.parse(path).getroot()
    bpm = min(320.0, max(30.0, float(root.findtext("bpm") or 120)))   # the tempo HZ-Drummer can play
    instruments = {i.findtext("id"): i.findtext("name") or "" for i in root.iter("instrument") if i.findtext("id") is not None}
    patterns = collections.OrderedDict()
    for p in root.find("patternList"):
        hits = {}   # (Position in Ticks, Stimme) -> [Lautstärke, Versatz in ms, Flam]
        notes = []
        size = int(p.findtext("size") or 192)
        for n in p.iter("note"):
            name = instruments.get(n.findtext("instrument"), "?")
            voice = voice_of(name)
            if voice is None:
                unmapped[name] += 1
                continue
            mapping[(name, voice)] += 1
            notes.append((int(n.findtext("position")), voice, max(0.0, min(1.0, float(n.findtext("velocity") or 0.8)))))
        # Noten neben jedem Raster (64tel, z. B. Snare + Rimshot 3 Ticks später): auf die nächste 16tel mit
        # Versatz in ms; trifft das dieselbe Stimme, wird ein Flam daraus
        for pos, voice, vel in sorted(notes, key=lambda x: (x[0] % 4 != 0, x[0])):
            lag = 0.0
            if pos % 4:
                snapped = round(pos / 12) * 12
                lag = max(-100.0, min(100.0, round((pos - snapped) / TPQ * 60000.0 / bpm, 1)))
                notes_log.append(f"{os.path.basename(path)} / {p.findtext('name')}: {voice} {pos - snapped:+d} Ticks -> {lag:+.1f} ms")
                pos = snapped
            if not 0 <= pos < size:
                notes_log.append(f"{os.path.basename(path)} / {p.findtext('name')}: {voice} bei Tick {pos}, hinter dem Ende ({size}), weggelassen")
                continue
            key = (pos, voice)
            if key in hits:
                hits[key][0] = max(vel, hits[key][0])   # doppelte Note: die lautere zählt ...
                hits[key][2] = hits[key][2] or lag != 0.0   # ... und ein Versatz dazu macht sie zum Flam
            else:
                hits[key] = [vel, lag, False]
        patterns[p.findtext("name")] = (size, hits)
    seq = root.find("patternSequence")
    columns = [[x.text for x in g.iter("patternID") if x.text] for g in (seq if seq is not None else [])]
    return bpm, patterns, columns


def bar_of(size, hits, triplet_style, waltz):
    """Ein Hydrogen-Pattern als Takt: (Taktart, Länge in Vierteln, [(pos, voice, vel)])."""
    if size == 144 and triplet_style:
        per_quarter, meter = 36, "4/4"                # 12 Triolen-Schritte, in Hydrogen auf 144 Ticks gestaucht
    elif size == 144 and waltz:
        per_quarter, meter = TPQ, "3/4"
    else:
        per_quarter = TPQ
        quarters = F(size, TPQ)
        if quarters.denominator != 1 or not 0 < quarters <= 64:
            raise ValueError(f"Pattern-Länge {size} Ticks ist kein Takt von 1 bis 64 Vierteln")
        meter = f"{quarters.numerator}/4"
    length = F(size, per_quarter)
    events = sorted((F(pos, per_quarter), voice, vel, lag, flam) for (pos, voice), (vel, lag, flam) in hits.items())
    return meter, length, events


def grid_for(length, rel_positions):
    for g in GRIDS:
        step = g * 4
        if (length / step).denominator == 1 and all((p / step).denominator == 1 for p in rel_positions):
            return g
    raise ValueError("kein Raster passt")


def pattern_json(pid, name, tags, tempo, bars, origin):
    """bars: [(meter, length, events)] hintereinander."""
    out_bars, hands, feet = [], [], []
    start, eid = F(0), 1
    for meter, length, events in bars:
        out_bars.append({"meter": meter, "grid": frac(grid_for(length, [e[0] for e in events]))})
        for pos, voice, vel, lag, flam in events:
            ev = {"id": eid, "voice": voice, "pos": frac(start + pos), "vel": round(vel, 3)}
            if flam:
                ev["flam"] = True
            if lag:
                ev["lag"] = lag
            (feet if voice in ("kick", "hihat.pedal") else hands).append(ev)
            eid += 1
        start += length
    return {"formatVersion": 1, "id": pid, "name": name, "revision": 1, "tags": tags,
            "tempo": {"default": round(tempo, 2), "min": 0, "max": 0}, "bars": out_bars,
            "swing": {"subdivision": "1/8", "ratio": 0.5},
            "humanize": {"amount": 0.25, "seed": seed_of(pid)},
            "layers": [{"id": 1, "role": "hands", "events": hands}, {"id": 2, "role": "feet", "events": feet}],
            "origin": {"parentId": "", "parentRevision": 0, "operators": origin, "seed": 0}}


written = {}
empty = []


def write_pattern(p):
    if p["id"] in written:
        print("Übersprungen: doppelte Kennung " + p["id"])
        return
    written[p["id"]] = p
    with open(os.path.join(OUT, "patterns", p["id"] + ".json"), "w", encoding="utf-8") as f:
        json.dump(p, f, ensure_ascii=False, indent=2)


os.makedirs(os.path.join(OUT, "patterns"), exist_ok=True)
os.makedirs(os.path.join(OUT, "songs"), exist_ok=True)

# ---------------------------------------------------------------- R.P. Bardet
BARDET = os.path.join(SRC, "Songs", "260_Drum_Machine_Patterns_from_RP_Bardet")
credit = "R.P. Bardet: 260 Drum Machine Patterns, Hydrogen-Fassung von Albert Phelipot (CC-BY-SA)"
stats = collections.Counter()
for fname in sorted(os.listdir(BARDET)) if os.path.isdir(BARDET) else []:
    if not fname.endswith(".h2song"):
        continue
    todo = []   # written only when the whole file could be read
    try:
        style = fname[:-7].replace("_", " ")
        tempo, patterns, _ = read_song(os.path.join(BARDET, fname))
        # 144 Ticks sind bei Bardet 12 Triolen-Schritte (Shuffle, Swing, Jazz, Reggae 7-9), nur der Walzer ist 3/4
        triplet_style = style != "Waltz"
        bars = collections.OrderedDict()
        sizes = {}
        for pname, (size, hits) in patterns.items():
            bars[pname] = bar_of(size, hits, triplet_style, style == "Waltz")
            sizes[pname] = size
        base_tags = [style.lower(), "bardet", "drum-machine", "band:R.P. Bardet"]
        for i, (pname, bar) in enumerate(bars.items(), 1):
            pid = f"user.bardet-{slug(style)}-{i:02d}"
            todo.append(("einzeln", pattern_json(pid, f"R.P. Bardet – {style} – {pname}", base_tags + [f"song:{style}"], tempo, [bar],
                                                 ["hydrogen-import", credit, f"{fname} / {pname}"])))
        # 4 Takte: dreimal das Muster, dann ein Break mit derselben Taktart und demselben Feel (Triolen zu Triolen),
        # die Breaks reihum
        grooves = [n for n in bars if not n.lower().startswith("break")]
        breaks = [n for n in bars if n.lower().startswith("break")]
        used = collections.Counter()
        k = 0
        for g in grooves:
            kind = (sizes[g], bars[g][0])
            fitting = [b for b in breaks if (sizes[b], bars[b][0]) == kind]
            if not fitting:
                continue
            b = fitting[used[kind] % len(fitting)]
            used[kind] += 1
            k += 1
            pid = f"user.bardet-{slug(style)}-4t-{k:02d}"
            todo.append(("4 Takte", pattern_json(pid, f"R.P. Bardet – {style} 4 Takte – {g} + {b}", base_tags + [f"song:{style} 4 Takte"],
                                                 tempo, [bars[g], bars[g], bars[g], bars[b]],
                                                 ["hydrogen-import", credit, f"{fname}: 3x {g} + {b}"])))
    except (OSError, ET.ParseError, ValueError, KeyError, TypeError, AttributeError, ZeroDivisionError) as e:   # a broken or odd file
        print(f"Übersprungen: {fname} ({why(e)})")
        continue
    for what, pattern in todo:
        if not any(layer["events"] for layer in pattern["layers"]):
            empty.append(pattern["id"])   # not one hit on a known drum: no Baustein
            continue
        write_pattern(pattern)
        stats[what] += pattern["id"] in written

# ---------------------------------------------------------------- Band-Songs
SONGS = [("ACDC/Back_in_Black/Back_in_Black.h2song", "AC/DC", "Back in Black", "rock"),
         ("Led_Zeppelin/Immigrant_Song/Immigrant_Song.h2song", "Led Zeppelin", "Immigrant Song", "rock"),
         ("Nirvana/Smells_like_teen_spirit/Smells_Like_Teen_Spirit.h2song", "Nirvana", "Smells Like Teen Spirit", "grunge"),
         ("ZZTop/LaGrange/LaGrange.h2song", "ZZ Top", "La Grange", "rock")]
song_ids = []
for rel, band, song, genre in SONGS:
    todo = []   # written only when the whole song could be read
    try:
        tempo, patterns, columns = read_song(os.path.join(SRC, "Songs", rel))
        while columns and not columns[0]:
            columns.pop(0)
        while columns and not columns[-1]:
            columns.pop()
        if any(not c for c in columns):
            raise ValueError(f"{song}: leere Spalte mitten im Ablauf")
        contents = list(collections.OrderedDict.fromkeys(tuple(c) for c in columns))
        ids = {}
        for i, content in enumerate(contents, 1):
            merged, size = {}, None
            for pname in content:
                psize, hits = patterns[pname]
                if size is not None and psize != size:
                    raise ValueError(f"{song}: übereinanderliegende Patterns verschieden lang")
                size = psize
                for key, (vel, lag, flam) in hits.items():
                    if key in merged:
                        old = merged[key]
                        merged[key] = [max(vel, old[0]), old[1] or lag, old[2] or flam]
                    else:
                        merged[key] = [vel, lag, flam]
            label = " + ".join(content)
            pid = f"user.{slug(band)}-{slug(song)}-{i:02d}"
            ids[content] = (pid, label)
            todo.append(pattern_json(pid, f"{band} – {song} – {label}",
                                     [genre, "song", "abschnitt", f"band:{band}", f"song:{song}", "hydrogen"], tempo,
                                     [bar_of(size, merged, False, False)],
                                     ["hydrogen-import", f"Hydrogen Song-and-pattern-repository: {rel}", label]))
    except (OSError, ET.ParseError, ValueError, KeyError, TypeError, AttributeError, ZeroDivisionError) as e:   # a broken or odd file
        print(f"Übersprungen: {rel} ({why(e)})")
        continue
    if not any(layer["events"] for pattern in todo for layer in pattern["layers"]):
        print(f"Kein Arrangement für {band} – {song}: keine Schläge")
        continue
    for pattern in todo:   # a column without a known drum stays: a pause in the song
        write_pattern(pattern)
        stats["Band-Takte"] += pattern["id"] in written
    parts = []
    for content in columns:
        pid, label = ids[tuple(content)]
        if parts and parts[-1]["pattern"] == pid and parts[-1]["repeats"] < 64:
            parts[-1]["repeats"] += 1
        else:
            parts.append({"pattern": pid, "label": label, "repeats": 1})
    bars = sum(len(written[q["pattern"]]["bars"]) * q["repeats"] for q in parts)
    hits = sum(len(layer["events"]) * q["repeats"] for q in parts for layer in written[q["pattern"]]["layers"])
    if bars > 512 or hits > 8192:
        print(f"Kein Arrangement für {band} – {song}: {bars} Takte, {hits} Schläge, zu lang")
        continue
    sid = f"{slug(band)}-{slug(song)}"
    song_ids.append((band, "song." + sid))
    with open(os.path.join(OUT, "songs", sid + ".json"), "w", encoding="utf-8") as f:
        json.dump({"formatVersion": 1, "id": sid, "name": f"{band} – {song}", "tempo": round(tempo, 2), "parts": parts},
                  f, ensure_ascii=False, indent=2)
    stats["Arrangements"] += 1
    stats["Arrangement-Blöcke"] += len(parts)

# ---------------------------------------------------------------- Bericht
if EXISTING:
    clash = [i for i in written if os.path.exists(os.path.join(EXISTING, i + ".json"))]
    print("Kollisionen mit vorhandenen Bausteinen:", clash or "keine")
print("Geschrieben:", dict(stats))
print("Zuordnung der Instrumente:")
for (name, voice), n in sorted(mapping.items(), key=lambda x: -x[1]):
    print(f"  {name:28} -> {voice:12} {n:5}")
print("Nicht zugeordnet:", dict(unmapped) or "nichts")
if empty:
    print("Ohne Schlag auf einer bekannten Trommel, nicht geschrieben:", ", ".join(empty))
for line in notes_log:
    print("Hinweis:", line)
print("Song-Ordner:", song_ids)
grids = collections.Counter(b["grid"] for p in written.values() for b in p["bars"])
meters = collections.Counter(b["meter"] for p in written.values() for b in p["bars"])
print("Raster:", dict(grids), "| Taktarten:", dict(meters))
