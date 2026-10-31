# Groove MIDI Dataset (echte Einspielungen, CC BY 4.0) -> HZ-Drummer: je Einspielung bis zu zwei verschiedene
# 8-Takt-Ausschnitte; die abwechslungsreichsten langen Einspielungen komplett, in 8-Takt-Blöcken plus Arrangement.
# Timing bleibt echt: je Takt das Raster, das am besten passt (16tel, Triolen, feiner), jeder Schlag behält seinen
# Abstand zum Rasterplatz als Versatz in ms; "Menschlich" steht auf 0. Schreibt nur in den Zielordner.
# Aufruf: python3 -I gmd_import.py <groove-ordner> <zielordner> <stil-präfix> "<Ordnername>"
import collections
import csv
import hashlib
import json
import os
import re
import sys
from fractions import Fraction as F

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # midiscan.py
from midiscan import MidiError, read_smf, MAP_GMD  # noqa: E402

ROOT, OUT, STYLE, BAND = sys.argv[1:5]
STYLES = STYLE.split(",")
ONLY_COMPLEX = "--nur-komplex" in sys.argv   # nur Ausschnitte über dem 70. Perzentil der Komplexität
MAX_EXCERPTS, CHUNK, ARRANGE_MIN_BARS, ARRANGE_MAX = 2, 8, 48, 3
GRIDS = [(F(1, 16), 0.0), (F(1, 12), 0.0), (F(1, 24), 4.0), (F(1, 32), 6.0), (F(1, 48), 10.0)]   # Strafe in ms je Schlag für feine Raster
CREDIT = "Groove MIDI Dataset (Google Magenta, Gillick u. a. 2019), CC BY 4.0, magenta.tensorflow.org/datasets/groove"
MAINS = {"hiphop": "Hip-Hop", "soul": "Soul", "afrobeat": "Afrobeat", "afrocuban": "Afro-Cuban", "neworleans": "New Orleans",
         "pop": "Pop", "dance": "Dance", "punk": "Punk", "reggae": "Reggae", "blues": "Blues", "country": "Country",
         "highlife": "Highlife", "gospel": "Gospel", "middleeastern": "Middle Eastern", "latin": "Latin", "funk": "Funk", "rock": "Rock"}
SUBS = {"": "", "secondline": "Second Line", "chacha": "Cha-Cha", "purdieshuffle": "Purdie-Shuffle", "halftime": "Halftime"}
SUBSTYLES = {"": "Allgemein", "swing": "Swing", "fusion": "Fusion", "march": "March", "funk": "Jazz-Funk",
             "mediumfast": "Medium-fast", "linear": "Linear", "fast": "Fast", "klezmer": "Klezmer",
             "brazilian-songo": "Songo", "brazilian-samba": "Samba", "brazilian": "Brasilianisch", "brazilian-baiao": "Baião",
             "chacarera": "Chacarera", "brazilian-sambareggae": "Samba-Reggae", "brazilian-ijexa": "Ijexá", "samba": "Samba",
             "purdieshuffle": "Purdie-Shuffle", "shuffle": "Shuffle", "prog": "Prog", "indie": "Indie", "halftime": "Halftime",
             "rockabilly": "Rockabilly", "folk": "Folk", "fast": "Fast", "latin": "Latin", "rock": "Rock"}
DRUMS = {"snare", "snare.rim", "snare.side", "tom1", "tom2", "tom3", "tom4", "kick"}
stats = collections.Counter()
grid_stats = collections.Counter()
swing_stats = collections.Counter()
swing_debug = []
lags = []


def frac(x):
    return str(x.numerator) if x.denominator == 1 else f"{x.numerator}/{x.denominator}"


def read_notes(path):
    smf = read_smf(path)
    ppq = smf["ppq"]
    tempo_us = next((v for tr in smf["tracks"] for (t, k, v) in tr if k == "tempo"), 500000)
    notes = sorted((t / ppq, v[1], v[2]) for tr in smf["tracks"] for (t, k, v) in tr if k == "on")
    return notes, tempo_us / 1000.0   # Position in Vierteln, ms je Viertel


SWING_OPTIONS = [0.5, 0.58, 0.667, 0.72]
CYMBALS = {"ride", "ride.bell", "hihat", "hihat.open", "hihat.half"}


def estimate_swing(notes):
    """Das Swing-Verhältnis der Einspielung aus den Becken-Schlägen: auf den 16teln ("e" und "a" verschoben) oder
    auf den Achteln (das "und"); (Unterteilung in Vierteln, Verhältnis); 0,5 = gerade."""
    import statistics
    between, sixteenth_shift = [], []
    for q, note, _ in notes:
        if MAP_GMD.get(note) in CYMBALS:
            f = q - int(q)
            if 0.42 <= f <= 0.78:
                between.append(f)
            for base in (0.25, 0.75):
                if base - 0.05 <= f <= base + 0.2:
                    sixteenth_shift.append(f - base)
    m16 = 0.5 + 2.0 * statistics.median(sixteenth_shift) if len(sixteenth_shift) >= 20 else 0.5
    m8 = statistics.median(between) if between else 0.5
    swing_debug.append((round(m8, 3), len(between), round(m16, 3), len(sixteenth_shift)))
    if m16 >= 0.55 and len(sixteenth_shift) >= 0.3 * max(1, len(between)):
        return (F(1, 4), min((0.58, 0.667), key=lambda r: abs(r - m16)))
    if len(between) < 20 or len(sixteenth_shift) > 0.3 * len(between) or m8 < 0.54:
        return (F(1, 2), 0.5)
    return (F(1, 2), min(SWING_OPTIONS[1:], key=lambda r: abs(r - m8)))


def places_for(g, b, bar_len, swing):
    step = g * 4
    sub, ratio = swing
    shift = float(sub) * 2.0 * (ratio - 0.5)   # wie hz::swingShift: der zweite Schlag jedes Paares kommt so viel später
    places = [b * bar_len + k * step for k in range(int(bar_len / step) + 1)]   # inkl. der "1" des nächsten Takts
    eff = [float(p) + (shift if ((p - b * bar_len) / (sub * 2)) % 1 == F(1, 2) else 0.0) for p in places]
    return step, places, eff


def assign(lst, places, eff, ms_per_q):
    out = []
    for q, voice, vel in lst:
        i = min(range(len(places)), key=lambda i: abs(q - eff[i]))
        out.append((places[i], voice, vel, (q - eff[i]) * ms_per_q, q))
    return out


def collisions(assigned, ms_per_q):
    """Gruppen von Schlägen derselben Stimme auf demselben Platz, die mindestens 15 ms auseinanderliegen."""
    groups = collections.defaultdict(list)
    for h in assigned:
        groups[(h[0], h[1])].append(h)
    return [g for g in groups.values() if len(g) > 1 and (max(x[4] for x in g) - min(x[4] for x in g)) * ms_per_q >= 15.0]


def snap_all(notes, bar_len, ms_per_q, swing):
    """Je Takt 16tel oder Triolen, was besser passt; ein feineres Raster nur, wenn mindestens zwei Schläge klar
    daneben liegen (über 30 % des Schritts) und im feinen Raster sauber sitzen (innerhalb 20 % von dessen Schritt).
    Jeder Schlag kommt auf seinen (geswingten) Rasterplatz, der Abstand bleibt als Versatz in ms."""
    by_bar = collections.defaultdict(list)
    for q, note, vel in notes:
        voice = MAP_GMD.get(note)
        if voice is None:
            stats[f"Note {note} ohne Stimme"] += 1
            continue
        by_bar[int(q // bar_len)].append((q, voice, vel / 127.0))
    grids, hits = {}, []
    for b, lst in sorted(by_bar.items()):
        coarse = []
        for g in (F(1, 16), F(1, 12)):
            step, places, eff = places_for(g, b, bar_len, swing)
            if (bar_len / step).denominator == 1:
                a = assign(lst, places, eff, ms_per_q)
                # zwei Schläge auf einem Platz kosten wie 60 ms Versatz: das Raster, das sie trennt, gewinnt
                clash = sum(len(grp) - 1 for grp in collisions(a, ms_per_q))
                coarse.append((sum(abs(h[3]) for h in a) + 60.0 * clash, g, step, a))
        _, g, step, a = min(coarse, key=lambda c: c[0])
        # daneben: weit vom Platz, oder zwei Schläge derselben Stimme auf einem Platz (Doppelschlag, Ghost + Hauptschlag)
        misfits = {(h[4], h[1], h[2]) for h in a if abs(h[3]) > 0.3 * float(step) * ms_per_q}
        for same in collisions(a, ms_per_q):
            misfits.update((h[4], h[1], h[2]) for h in same)
        misfits = sorted(misfits)
        if len(misfits) >= 2:
            for fg in (F(1, 24), F(1, 32), F(1, 48)):
                fstep, fplaces, feff = places_for(fg, b, bar_len, swing)
                if (bar_len / fstep).denominator != 1:
                    continue
                fa = assign(misfits, fplaces, feff, ms_per_q)
                if all(abs(h[3]) <= 0.4 * float(fstep) * ms_per_q for h in fa) and not collisions(fa, ms_per_q):
                    g, a = fg, assign(lst, fplaces, feff, ms_per_q)
                    stats["Takt mit feinem Raster"] += 1
                    break
        grids[b] = g
        hits.extend(a)
    return merge(hits, ms_per_q), grids


def merge(hits, ms_per_q):
    """Zwei Schläge derselben Stimme auf einem Platz: ab 15 ms Abstand ein Flam (Trommeln), sonst zählt der lautere."""
    groups = collections.defaultdict(list)
    for h in hits:
        groups[(h[0], h[1])].append(h)
    out = []
    for (pos, voice), lst in groups.items():
        lst.sort(key=lambda h: h[4])
        loudest = max(lst, key=lambda h: h[2])
        if len(lst) > 1 and voice in DRUMS and (lst[-1][4] - lst[0][4]) * ms_per_q >= 15.0:
            out.append((pos, voice, loudest[2], lst[-1][3], True))
            stats["Flam aus zwei dichten Schlägen"] += 1
            d = (lst[-1][4] - lst[0][4]) * ms_per_q
            stats["  Flam-Abstand " + ("unter 40 ms" if d < 40 else "40-80 ms" if d < 80 else "über 80 ms")] += 1
        else:
            if len(lst) > 1:
                stats["Doppelschlag zusammengelegt"] += 1
            out.append((pos, voice, loudest[2], loudest[3], False))
    return out


def bar_key(hits, b, bar_len):
    return frozenset((h[0] - b * bar_len, h[1]) for h in hits if int(h[0] // bar_len) == b)


def pattern_json(pid, name, tags, bpm, meter, bar_len, b0, n, hits, grids, origin, swing=(F(1, 2), 0.5)):
    out_bars = [{"meter": meter, "grid": frac(grids.get(b0 + i, F(1, 16)))} for i in range(n)]
    hands, feet, eid = [], [], 1
    for pos, voice, vel, lag, flam in sorted(hits, key=lambda h: (h[0], h[1])):
        if not (b0 <= int(pos // bar_len) < b0 + n):
            continue
        lag = max(-100.0, min(100.0, lag))
        lags.append(lag)
        ev = {"id": eid, "voice": voice, "pos": frac(pos - b0 * bar_len), "vel": round(max(0.05, vel), 3)}
        if flam:
            ev["flam"] = True
        if abs(lag) >= 0.05:
            ev["lag"] = round(lag, 1)
        (feet if voice in ("kick", "hihat.pedal") else hands).append(ev)
        eid += 1
    for b in out_bars:
        grid_stats[b["grid"]] += 1
    return {"formatVersion": 1, "id": pid, "name": name, "revision": 1, "tags": tags,
            "tempo": {"default": bpm, "min": 0, "max": 0}, "bars": out_bars,
            "swing": {"subdivision": "1/8" if swing[0] == F(1, 2) else "1/16", "ratio": swing[1]},
            "humanize": {"amount": 0.0, "seed": int(hashlib.md5(pid.encode()).hexdigest()[:8], 16)},   # das Timing ist echt
            "layers": [{"id": 1, "role": "hands", "events": hands}, {"id": 2, "role": "feet", "events": feet}],
            "origin": {"parentId": "", "parentRevision": 0, "operators": origin, "seed": 0}}


os.makedirs(os.path.join(OUT, "patterns"), exist_ok=True)
os.makedirs(os.path.join(OUT, "songs"), exist_ok=True)
rows = [r for r in csv.DictReader(open(os.path.join(ROOT, "info.csv"), encoding="utf-8"))
        if any((r["style"] or "") == st or (r["style"] or "").startswith(st + "/") for st in STYLES) and r["beat_type"] == "beat"]
recordings = []
for r in rows:
    try:   # a broken file or an odd line in info.csv: this recording only
        notes, ms_per_q = read_notes(os.path.join(ROOT, r["midi_filename"]))
        num, den = (int(x) for x in r["time_signature"].split("-"))
        if not 1 <= num <= 64 or den not in (1, 2, 4, 8, 16, 32):
            raise ValueError(f"Taktart {num}/{den} kann HZ-Drummer nicht darstellen")
        bar_len = F(num * 4, den)
        bpm = max(30, min(320, int(r["bpm"])))   # the tempo HZ-Drummer can play
        swing = estimate_swing(notes)
        hits, grids = snap_all(notes, bar_len, ms_per_q, swing)
    except (MidiError, OSError, KeyError, ValueError, ZeroDivisionError, AttributeError, TypeError) as e:
        why = ("Datei fehlt" if isinstance(e, FileNotFoundError) else
               "Zeile in info.csv unvollständig" if isinstance(e, (KeyError, AttributeError, TypeError)) else e)
        print(f"Übersprungen: {r.get('midi_filename') or r.get('id')} ({why})")
        continue
    swing_stats[("16tel" if swing[0] == F(1, 4) else "8tel", swing[1])] += 1
    counts = collections.Counter(int(h[0] // bar_len) for h in hits)
    full = [b for b, c in counts.items() if c >= 2]
    if not full:
        continue
    first, last = min(full), max(full)
    keys = {b: bar_key(hits, b, bar_len) for b in range(first, last + 1)}
    main, _, sub = r["style"].partition("/")
    if main == "rock" and not sub:
        sub = "langsam (unter 100 BPM)" if int(r["bpm"]) < 100 else "mittel (100-140 BPM)" if int(r["bpm"]) <= 140 else "schnell (über 140 BPM)"
    elif len(STYLES) > 1:
        sub = (MAINS.get(main, main.capitalize()) + " " + SUBS.get(sub, sub.replace("-", " ").capitalize())).strip()
    else:
        sub = SUBSTYLES.get(sub, sub.replace("-", " ").capitalize()) if sub or STYLES != ["jazz"] else "Jazz allgemein"
    drummer, session, nr = r["drummer"].replace("drummer", "d"), r["session"].split("/")[-1].replace("session", "s").replace("eval_", "e"), r["id"].split("/")[-1]
    recordings.append({"row": r, "hits": hits, "grids": grids, "bar_len": bar_len, "meter": f"{num}/{den}", "first": first, "last": last,
                       "keys": keys, "bpm": bpm, "swing": swing, "sub": sub,
                       "take": f"{drummer}-{session}-{nr}", "variety": len(set(keys.values())), "main": main})

# Komplexität: Schläge je Ausschnitt plus 8 je verschiedenem Takt; mit --nur-komplex nur das obere Drittel
COMPLEX_MIN = 0
if ONLY_COMPLEX:
    import statistics
    scores = []
    for x in recordings:
        L = x["bar_len"]
        for b0 in range(x["first"], x["last"] + 1 - CHUNK + 1, CHUNK):
            scores.append(sum(1 for h in x["hits"] if b0 <= int(h[0] // L) < b0 + CHUNK) + 8 * len({x["keys"][b] for b in range(b0, b0 + CHUNK)}))
    COMPLEX_MIN = statistics.quantiles(scores, n=10)[6] if scores else 0
    print("Komplexitätsschwelle (70. Perzentil):", COMPLEX_MIN, "von", len(scores), "Ausschnitten")

# die abwechslungsreichsten langen Einspielungen komplett
for x in recordings:   # Spielzeit der Einspielung
    x["seconds"] = (x["last"] - x["first"] + 1) * float(x["bar_len"]) * 60.0 / x["bpm"]
# nur Einspielungen bis 5 Minuten; längere liefern stattdessen mehr Ausschnitte
long_ones = sorted((x for x in recordings if ARRANGE_MIN_BARS <= x["last"] - x["first"] + 1 and x["seconds"] <= 300.0), key=lambda x: -x["variety"])
arranged = {id(x) for x in long_ones[:ARRANGE_MAX]}
song_ids = []
for x in recordings:
    r, bar_len, first, last = x["row"], x["bar_len"], x["first"], x["last"]
    tags = [x["main"], x["sub"].lower(), "groove-midi", f"drummer:{r['drummer']}", f"band:{BAND}", f"song:{x['sub']}"]
    base = f"user.gmd-{x['main']}-{x['take']}"
    title = f"{BAND} – {x['sub']} – {x['bpm']} BPM, Aufnahme {x['take']}"
    windows = [(b0, min(CHUNK, last + 1 - b0)) for b0 in range(first, last + 1, CHUNK)]
    windows = [(b0, n) for b0, n in windows if n >= 2]
    if id(x) in arranged:
        chosen = windows
    else:
        full_windows = [(b0, n) for b0, n in windows if n == CHUNK]
        def score(w):
            return sum(1 for h in x["hits"] if w[0] <= int(h[0] // bar_len) < w[0] + w[1]) + 8 * len({x["keys"][b] for b in range(w[0], w[0] + w[1])})
        def items(w):
            return {(int(h[0] // bar_len) - w[0], h[0] - int(h[0] // bar_len) * bar_len, h[1]) for h in x["hits"] if w[0] <= int(h[0] // bar_len) < w[0] + w[1]}
        chosen = []
        if ONLY_COMPLEX:
            full_windows = [w for w in full_windows if score(w) >= COMPLEX_MIN]
        for w in sorted(full_windows, key=lambda w: -score(w)):
            if all(len(items(w) & items(c)) / max(1, len(items(w) | items(c))) < 0.7 for c in chosen):
                chosen.append(w)
            if len(chosen) == (6 if x["seconds"] > 300.0 else MAX_EXCERPTS):
                break
        chosen.sort()
        # an excerpt without a hit: no Baustein (in a complete recording it stays, a pause)
        chosen = [w for w in chosen if any(w[0] <= int(h[0] // bar_len) < w[0] + w[1] for h in x["hits"])]
    parts = []
    for b0, n in chosen:
        a, b = b0 - first + 1, b0 - first + n
        pid = f"{base}-t{a}"
        p = pattern_json(pid, f"{title} (T{a}-{b})", tags, x["bpm"], x["meter"], bar_len, b0, n, x["hits"], x["grids"],
                         ["groove-midi-import", CREDIT, r["midi_filename"], f"Takte {a}-{b} der Einspielung, Timing wie gespielt"], x["swing"])
        with open(os.path.join(OUT, "patterns", pid + ".json"), "w", encoding="utf-8") as f:
            json.dump(p, f, ensure_ascii=False, indent=2)
        parts.append({"pattern": pid, "label": f"T{a}-{b}", "repeats": 1})
        stats["Bausteine"] += 1
    if id(x) in arranged:
        # in Teile, damit kein Arrangement über 480 Takte oder 7800 Schläge kommt
        groups, cur, bars, hits_in = [], [], 0, 0
        for (b0, n), part in zip(chosen, parts):
            h = sum(1 for h in x["hits"] if b0 <= int(h[0] // bar_len) < b0 + n)
            if cur and (bars + n > 480 or hits_in + h > 7800):
                groups.append(cur); cur, bars, hits_in = [], 0, 0
            cur.append(part); bars += n; hits_in += h
        groups.append(cur)
        for k, group in enumerate(groups, 1):
            sid = f"gmd-{x['main']}-{x['take']}" + (f"-teil{k}" if len(groups) > 1 else "")
            label = f"komplett, {last - first + 1} Takte" if len(groups) == 1 else f"Teil {k} von {len(groups)}, {group[0]['label']} bis {group[-1]['label'][1:].split('-')[1]}"
            with open(os.path.join(OUT, "songs", sid + ".json"), "w", encoding="utf-8") as f:
                json.dump({"formatVersion": 1, "id": sid, "name": f"{title} ({label})", "tempo": x["bpm"], "parts": group},
                          f, ensure_ascii=False, indent=2)
            song_ids.append("song." + sid)
            stats["Arrangements"] += 1
with open(os.path.join(OUT, "ordner-zusatz.json"), "w", encoding="utf-8") as f:
    json.dump({"songs": {BAND: song_ids}}, f, ensure_ascii=False, indent=2)

print("Einspielungen:", len(recordings), "| komplett als Arrangement:", [f"{x['sub']} {x['bpm']} BPM {x['take']} ({x['last'] - x['first'] + 1} Takte, {x['variety']} verschiedene)" for x in long_ones[:ARRANGE_MAX]])
for k, v in sorted(stats.items()):
    print(f"  {k:36} {v}")
print("Raster je Takt:", dict(grid_stats), "| Swing je Einspielung:", dict(swing_stats))
if lags:
    import statistics
    print(f"Versatz: Median |{statistics.median(abs(l) for l in lags):.1f}| ms, über 50 ms: {sum(1 for l in lags if abs(l) > 50)} von {len(lags)}, begrenzt auf 100: {sum(1 for l in lags if abs(l) >= 100)}")
print("Ordner:", collections.Counter(x["sub"] for x in recordings))
print("Swing-Schätzung (Median, Offbeats, 16tel):", sorted(swing_debug))
