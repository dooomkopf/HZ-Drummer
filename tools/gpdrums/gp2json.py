#!/usr/bin/env python3
"""Übersetzt die Ausgabe von GpDrums.java in HZ-Drummer-Patterns (JSON) – mit Dynamik.

Aufruf:
  gp2json.py <gpdrums-ausgabe.txt> <zielordner> <titel> <id-präfix> [abschnitte] [--tags a,b,...] [--spielweise]

  abschnitte: "1:Intro,25:Strophe,41:Strophe 2,..."  (Taktnummern im gespielten Song, d.h. nach dem
              Ausrollen der Wiederholungen und ohne leere Anfangstakte; ohne Angabe: Marker aus der
              Guitar-Pro-Datei, sonst 8-Takt-Teile)
  --tags:     zusätzliche Tags, z.B. "band:Gorguts,song:Obscura" (Ordner im Baustein-Menü); ein Komma vor einem
              Leerzeichen oder zwischen Ziffern gehört zum Tag ("song:Damage, Inc.", "song:1,000 Eyes")
  --spielweise: Bausteine nach Spielweise statt nach ganzen Abschnitten: geschnitten wird, wo Taktart, Tempo,
              Spielart (Blast, Doppelbass, Groove, Fill) oder das Zeitmaß-Becken wechselt, innerhalb der Marker;
              2 bis 8 echte Takte je Baustein (längere Strecken in Vierern, ein Einzeltakt hängt am Vorgänger),
              Namen wie "Riff A – Doppelbass mit Ride"; nur exakt Gleiches wird zusammengelegt
  Versatz:    ein optionales 8. Feld der HIT-Zeile (ms, z.B. von mid2gpdrums.py) wird "lag" (höchstens ±100)

Wiederholungen werden ausgerollt wie beim Abspielen in TuxGuitar: |: ... :| mit Anzahl, alternative
Enden (1., 2. ...) nur im passenden Durchgang. Gleiche Abschnitte werden ein Baustein; folgen sie
direkt aufeinander, stehen sie im Song als Wiederholung.

Fremde Dateien: Was HZ-Drummer nicht halten kann, wird angepasst und am Ende gemeldet. Schläge hinter dem
Ende eines überfüllten Takts und doppelte Schläge (gleiche Trommel, gleiche Stelle) fallen weg, Takte über
64 Schläge werden geteilt, das Tempo bleibt zwischen 30 und 320. Abschnitte ohne Schläge bleiben als Pause
im Song. Ein Song über 512 Takte oder 8192 Schläge wird in Teile geteilt (<id>-teil1, ...).

Erzeugt:
  <zielordner>/<id-präfix>-NN-<name>.json   ein Pattern je verschiedenem Abschnitt
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
import unicodedata
from collections import Counter, OrderedDict
from fractions import Fraction
from math import gcd, isfinite

TPQ = 960   # Ticks pro Viertel
MAX_BARS, MAX_HITS = 512, 8192   # what one Baustein, and one arrangement, may hold in HZ-Drummer
dropped = Counter()              # what was left out or changed, for the report at the end

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
    for line in open(path, encoding="utf-8", errors="replace"):
        p = line.split()
        if not p:
            continue
        try:
            if p[0] == "BAR":
                bars[int(p[1])] = dict(nr=int(p[1]), length=int(p[3]), num=int(p[4]), den=int(p[5]), tempo=int(p[6]), hits=[],
                                       open=len(p) > 7 and p[7] == "1", close=int(p[8]) if len(p) > 8 else 0,
                                       alt=int(p[9]) if len(p) > 9 else 0)
            elif p[0] == "MARKER":
                markers[int(p[1])] = no_controls(" ".join(p[2:]))
            elif p[0] == "HIT":
                nr, off, note, vel, ghost, acc = (int(x) for x in p[1:7])
                lag = float(p[7]) if len(p) > 7 else 0.0
                lag = max(-100.0, min(100.0, lag)) if isfinite(lag) else 0.0
                if note in VOICE:
                    bars[nr]["hits"].append(dict(off=off, voice=VOICE[note], vel=vel, ghost=ghost, acc=acc, lag=lag))
                else:
                    unknown[note] = unknown.get(note, 0) + 1
        except (ValueError, IndexError, KeyError):
            dropped["unlesbare Zeilen übersprungen"] += 1   # a damaged line, or a hit without its bar
    return bars, markers, unknown


def no_controls(s):
    """Text without control characters: a NUL or ESC in a name makes the file unreadable for HZ-Drummer."""
    return "".join(ch for ch in s if unicodedata.category(ch) != "Cc")


def unroll(bars):
    """Die Takte in Spielreihenfolge (Liste von Taktnummern), wie TuxGuitars MidiRepeatController:
    'close' = Anzahl der Wiederholungen, 'alt' = Bitmaske der Durchgänge eines alternativen Endes."""
    order = list(bars)
    out = []
    index, start, last, number, alternative = 0, 0, -1, 0, 0
    done = False   # the repeat is played out: a further close without a new open is ignored, as in TuxGuitar
    guard = 0
    while index < len(order) and guard < 100000:
        guard += 1
        bar = bars[order[index]]
        if index == 0:
            start = 0
        if bar["open"]:
            start = index
            done = False
            if index > last:   # zum ersten Mal hier: Zähler zurück
                number, alternative = 0, 0
        else:
            if alternative == 0:
                alternative = bar["alt"]
            if alternative > 0 and not (alternative & (1 << number)):
                if bar["close"] > 0:
                    alternative = 0
                index += 1     # alternatives Ende eines anderen Durchgangs: überspringen
                continue
        last = max(last, index)
        out.append(order[index])
        if number < bar["close"] and not done:
            number += 1
            index = start
            alternative = 0
        else:
            if bar["close"] > 0:
                number = 0
                done = True
            index += 1
            alternative = 0 if bar["close"] > 0 or not bar["alt"] else alternative
    return out


def content_key(part):
    # the offset to 10 ms, as the editor shows it: what sounds the same is the same
    return tuple((b["num"], b["den"], b["length"], tuple(sorted((h["off"], h["voice"], h["v"], round(h.get("lag", 0.0) / 10))
                                                                for h in b["hits"]))) for b in part)


CYMBAL_NAMES = {"hihat": "Hi-Hat", "hihat.open": "offener Hi-Hat", "ride": "Ride", "china": "China", "crash": "Crash"}


def bar_style(bar):
    """Spielart eines Takts und sein Zeitmaß-Becken (das Becken, das mindestens einmal je Viertel läuft)."""
    quarters = bar["length"] / TPQ
    if quarters <= 0 or not bar["hits"]:
        return ("Pause", None)
    count = {}
    for h in bar["hits"]:
        count[h["voice"]] = count.get(h["voice"], 0) + 1
    kick = count.get("kick", 0) / quarters
    snare = (count.get("snare", 0)) / quarters
    toms = sum(count.get(t, 0) for t in ("tom1", "tom2", "tom3", "tom4"))
    cym = {"hihat": count.get("hihat", 0), "hihat.open": count.get("hihat.open", 0),
           "ride": count.get("ride", 0) + count.get("ride.bell", 0), "china": count.get("china", 0),
           "crash": count.get("crash1", 0) + count.get("crash2", 0) + count.get("splash", 0)}
    main = max(cym, key=cym.get)
    timekeeper = main if cym[main] >= quarters else None
    if toms >= 4 and toms >= sum(cym.values()):
        return ("Fill", None)
    if snare >= 2 and kick >= 2:
        return ("Blast", timekeeper)
    if kick >= 3.5:
        return ("Doppelbass", timekeeper)
    return ("Groove", timekeeper)


def style_name(style, bars=1):
    kind, cymbal = style
    if kind == "Fill" and bars > 1:
        kind = "Tom-Groove"   # several bars of toms: a groove on them, not a fill
    return kind if cymbal is None else f"{kind} mit {CYMBAL_NAMES[cymbal]}"


def clean_marker(name):
    """Markernamen ohne Tempoangaben ("Verse 1 - tempo: 194" -> "Verse 1"): das Tempo steht im Baustein."""
    return re.sub(r"\s*[-–(]?\s*tempo\s*:?\s*\d+\s*\)?", "", name, flags=re.IGNORECASE).strip(" -–")


def style_sections(song, marked):
    """Abschnitte nach Spielweise: marked = [(start, markername)] (1-basiert, mindestens einer bei 1).
    Gibt [(start, name)] zurück; Schnitte bei Wechsel von Taktart, Tempo, Spielart oder Zeitmaß-Becken."""
    out = []   # [start, name]
    for k, (start, marker) in enumerate(marked):
        end = marked[k + 1][0] - 1 if k + 1 < len(marked) else len(song)
        marker = clean_marker(marker)
        sig = lambda n: (song[n - 1]["num"], song[n - 1]["den"], song[n - 1]["tempo"], bar_style(song[n - 1]))
        runs = []   # [start, end, style]
        for n in range(start, end + 1):
            if runs and sig(n) == sig(runs[-1][0]):
                runs[-1][1] = n
            else:
                runs.append([n, n, bar_style(song[n - 1])])
        # a single bar (a fill, a break) belongs to the run before it (a long run is cut in fours below)
        merged = []
        for r in runs:
            if r[1] == r[0] and merged:
                merged[-1][1] = r[1]
            elif merged and merged[-1][1] == merged[-1][0] and merged[-1][1] + 1 == r[0]:
                merged[-1][1] = r[1]   # a single bar at the start goes with the run after it
                merged[-1][2] = r[2]
            else:
                merged.append(list(r))
        # long runs in fours; a remainder of one bar goes with the last four
        for a, b, style in merged:
            length = b - a + 1
            pieces = [4] * (length // 4) if length > 8 else [length]
            if length > 8 and length % 4:
                if length % 4 == 1:
                    pieces[-1] += 1
                else:
                    pieces.append(length % 4)
            pos = a
            for size in pieces:
                name = style_name(style, size)
                out.append([pos, f"{marker} – {name}" if marker else name])
                pos += size
    # a section of a single bar (a marker on one bar) joins its neighbour: the one before, else the one after;
    # a neighbour of 8 bars is halved first, so no Baustein gets more than 8
    k = 0
    while k < len(out) and len(out) > 1:
        end = out[k + 1][0] - 1 if k + 1 < len(out) else len(song)
        if end == out[k][0]:
            if k > 0:
                if out[k][0] - out[k - 1][0] >= 8:
                    out.insert(k, [out[k - 1][0] + 4, out[k - 1][1]])
                    k += 1
                del out[k]
                continue
            if (out[2][0] if len(out) > 2 else len(song) + 1) - out[1][0] >= 8:
                out.insert(2, [out[1][0] + 4, out[1][1]])
            out[1][0] = out[0][0]
            del out[0]
            continue
        k += 1
    return [tuple(x) for x in out]


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
        seen = set()
        for h in sorted(b["hits"], key=lambda h: (h["off"], h["voice"], -h["v"])):
            if not 0 <= h["off"] < b["length"]:
                dropped["Schläge hinter dem Ende eines überfüllten Takts weggelassen"] += 1
                continue
            if (h["off"], h["voice"]) in seen:
                dropped["doppelte Schläge (gleiche Trommel, gleiche Stelle) zusammengelegt"] += 1
                continue   # the louder one stays
            seen.add((h["off"], h["voice"]))
            ev = {"id": eid, "voice": h["voice"], "pos": frac(pos + h["off"]), "vel": h["v"]}
            if abs(h.get("lag", 0.0)) >= 0.5:
                ev["lag"] = round(h["lag"], 1)   # ms later (-: earlier) than its place, as played
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


def fit_meter(bar):
    """The bar as HZ-Drummer can hold it (1 to 64 beats of 1/1 to 1/32): itself, or cut into 4/4 pieces and a rest
    (149/4, 6/64), or nothing when its length cannot be written (3/64, zero)."""
    if 1 <= bar["num"] <= 64 and bar["den"] in (1, 2, 4, 8, 16, 32) and bar["length"] > 0:
        return [bar]
    pieces = []
    for start in range(0, max(0, bar["length"]), TPQ * 4):
        size = min(TPQ * 4, bar["length"] - start)
        den = next((d for d in (4, 8, 16, 32) if size * d % (TPQ * 4) == 0), None)
        if den is None:
            continue
        hits = [dict(h, off=h["off"] - start) for h in bar["hits"] if start <= h["off"] < start + size]
        pieces.append(dict(bar, num=size * den // (TPQ * 4), den=den, length=size, hits=hits))
    dropped[f"Taktart {bar['num']}/{bar['den']} nicht darstellbar, " + ("neu eingeteilt (4/4 und Rest)" if pieces else "Takt weggelassen")] += 1
    return pieces


def split_song(parts, size):
    """The song's parts in arrangements of at most MAX_BARS bars and MAX_HITS hits; size: pattern id -> (bars, hits)."""
    groups, cur, bars, hits = [], [], 0, 0
    for q in parts:
        b, e = size[q["pattern"]]
        left = q["repeats"]
        while left:
            fit = min(left, (MAX_BARS - bars) // b, (MAX_HITS - hits) // e if e else left)   # a pause has no hits
            if fit == 0 and cur:
                groups.append(cur)
                cur, bars, hits = [], 0, 0
                continue
            fit = max(fit, 1)
            cur.append(dict(q, repeats=fit))
            bars, hits, left = bars + fit * b, hits + fit * e, left - fit
    return groups + [cur] if cur else groups


def main():
    args = [a for a in sys.argv[1:]]
    tags = ["song", "abschnitt"]
    if "--tags" in args:
        k = args.index("--tags")
        # a comma before a space or between digits belongs to the tag: "song:Damage, Inc.", "song:1,000 Eyes"
        tags += [t.strip() for t in re.split(r",(?!\s|(?<=\d,)\d)", args[k + 1]) if t.strip()]
        del args[k:k + 2]
    by_style = "--spielweise" in args
    if by_style:
        args.remove("--spielweise")
    src, outdir, title, prefix = args[:4]
    section_arg = args[4] if len(args) > 4 else ""
    # undecodable bytes in a file name (argv) as text
    readable = lambda t: no_controls(t.encode("utf-8", "surrogateescape").decode("utf-8", "replace"))   # noqa: E731
    title, tags = readable(title), [readable(t) for t in tags]

    bars, markers, unknown = parse(src)
    played = unroll(bars)
    with_hits = [k for k, n in enumerate(played) if bars[n]["hits"]]
    played = played[with_hits[0]:with_hits[-1] + 1] if with_hits else []   # without empty bars at start and end

    has_dynamics = len({h["vel"] for b in bars.values() for h in b["hits"] if h["vel"] > 0}) > 1
    fitted = {}   # bar number -> the bar as HZ-Drummer can hold it (a bar too long for it comes in pieces)
    for n, b in bars.items():   # once per written bar: repeated bars share it
        if has_dynamics:
            for h in b["hits"]:
                vel = min(h["vel"], 127) if h["vel"] > 0 else 95   # a broken value (-1 in some files): forte
                h["v"] = round(vel / 127, 3) if not h["ghost"] else 0.3
        fitted[n] = fit_meter(b)
        if not has_dynamics:
            for piece in fitted[n]:
                dynamics(piece)

    song, starts = [], []   # starts: where each played bar begins in song
    for n in played:
        starts.append(len(song))
        song += fitted[n]
    if not song:
        sys.exit(f"{title}: keine Schläge, die HZ-Drummer einer Trommel zuordnen kann, nichts geschrieben"
                 + (f" (nicht zugeordnete MIDI-Noten: {unknown})" if unknown else ""))

    tempi = [b["tempo"] for b in song if b["tempo"] > 0]
    tempo = round(sum(tempi) / len(tempi)) if tempi else 120
    if not 30 <= tempo <= 320:
        dropped[f"Tempo {tempo} auf {min(320, max(30, tempo))} begrenzt"] += 1
        tempo = min(320, max(30, tempo))

    # Abschnitte über die gespielte Reihenfolge (1-basiert): Angabe > Marker der Datei > 8-Takt-Teile
    sections = []
    if section_arg:
        for item in section_arg.split(","):
            n, name = item.split(":", 1)
            k = int(n)   # counts played bars; a bar that came in pieces (149/4) is several bars in song
            if 1 <= k <= len(starts):
                k = starts[k - 1] + 1
            elif k > len(starts):
                k += len(song) - len(starts)   # beyond the end stays beyond it
            sections.append((k, name.strip()))
    elif markers:
        sections = [(starts[i] + 1, markers[n]) for i, n in enumerate(played) if n in markers]
        if not sections or sections[0][0] != 1:
            sections.insert(0, (1, "Anfang"))
    elif not by_style:
        sections = [(i + 1, f"Teil {i // 8 + 1}") for i in range(0, len(song), 8)]
    if by_style:   # within the markers (or the whole song): by playing style
        sections = style_sections(song, sections if sections else [(1, "")])
    capped = []   # a section too big for one Baustein (one marker over a whole long song): in parts of 8 bars or less
    for k, (start, name) in enumerate(sections):
        end = sections[k + 1][0] - 1 if k + 1 < len(sections) else len(song)
        if end - start + 1 > MAX_BARS or sum(len(b["hits"]) for b in song[start - 1:end]) > MAX_HITS:
            while start <= end:
                last = min(start + 7, end)
                while last > start and sum(len(b["hits"]) for b in song[start - 1:last]) > MAX_HITS:
                    last -= 1
                capped.append((start, name))
                start = last + 1
        else:
            capped.append((start, name))
    sections = capped

    os.makedirs(outdir, exist_ok=True)
    seed = abs(hash(prefix)) % 100000 + 1

    written = []
    song_parts = []
    pattern_of = {}   # content -> pattern id: equal sections become one pattern
    size = {}         # pattern id -> (bars, hits); a section without hits is a pause in the song
    pending = []      # written once it is clear that there is something to hear
    for k, (start, name) in enumerate(sections):
        end = sections[k + 1][0] - 1 if k + 1 < len(sections) else len(song)
        part = song[start - 1:end]
        if not part:
            continue
        key = content_key(part)
        pid = pattern_of.get(key)
        if pid is None:
            number = len(pattern_of) + 1
            pid = f"{prefix}-{number:02d}-{slug(name)}"
            if len(pid.encode()) > 250:   # a file name has at most 255 bytes: shorter, the number stays
                pid = f"{prefix[:200].rstrip('-')}-{number:02d}-{slug(name)}"[:250].rstrip("-")
            pattern_of[key] = pid
            pat = make_pattern(pid, f"{title} – {number:02d} {name} (T{start}-{end})", tags, tempo, part, seed + number)
            pending.append((os.path.join(outdir, f"{pid}.json"), pat, len(part)))
            size[pid] = (len(part), sum(len(layer["events"]) for layer in pat["layers"]))
        if song_parts and song_parts[-1]["pattern"] == pid and song_parts[-1]["repeats"] < 64:
            song_parts[-1]["repeats"] += 1   # directly again: a repeat in the song
        else:
            song_parts.append({"pattern": pid, "label": name, "repeats": 1})
    if not any(hits for _, hits in size.values()):
        sys.exit(f"{title}: keine Schläge, die in einen Takt passen, nichts geschrieben")
    for path, pat, n in pending:
        json.dump(pat, open(path, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        written.append((path, n))

    # der ganze Song als Struktur: Abschnitte nacheinander (für den Song-Arranger); zu lang für ein Arrangement: in Teilen
    songs_dir = os.path.join(os.path.dirname(os.path.abspath(outdir)), "songs")
    os.makedirs(songs_dir, exist_ok=True)
    song_id = prefix.split(".", 1)[-1]
    groups = split_song(song_parts, size)
    if len(song_id.encode()) > (250 if len(groups) == 1 else 240):   # a file name has at most 255 bytes
        song_id = song_id[:200].rstrip("-")
    for g, group in enumerate(groups, 1):
        sid, name = (song_id, f"{title} (Song)") if len(groups) == 1 else (f"{song_id}-teil{g}", f"{title} (Song, Teil {g} von {len(groups)})")
        song_json = {"formatVersion": 1, "id": sid, "name": name, "tempo": tempo, "parts": group}
        path = os.path.join(songs_dir, f"{sid}.json")
        json.dump(song_json, open(path, "w", encoding="utf-8"), ensure_ascii=False, indent=1)

    print(f"{len(bars)} Takte notiert, {len(song)} gespielt, {len(sections)} Abschnitte, {len(size)} Bausteine, "
          f"{len(song_parts)} Blöcke im Song; Tempo {tempo}, "
          f"{'Dynamik aus der Vorlage' if has_dynamics else 'Dynamik nach Spielregeln'}")
    for p, n in written:
        print(f"  {os.path.basename(p)}  ({n} Takte)")
    for g, group in enumerate(groups, 1):
        sid = song_id if len(groups) == 1 else f"{song_id}-teil{g}"
        print(f"  songs/{sid}.json: " + ", ".join(f"{q['label']}" + (f" x{q['repeats']}" if q['repeats'] > 1 else "") for q in group))
    if unknown:
        print("Nicht zugeordnete MIDI-Noten (weggelassen):", unknown)
    if len(groups) > 1:
        print(f"Song zu lang für ein Arrangement (höchstens {MAX_BARS} Takte, {MAX_HITS} Schläge): {len(groups)} Teile")
    for what, n in dropped.items():
        print(what if n == 1 else f"{what}: {n}")


if __name__ == "__main__":
    main()
