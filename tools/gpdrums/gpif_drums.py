#!/usr/bin/env python3
"""Liest die Schlagzeugspur einer Guitar-Pro-6-Datei (.gpx = BCFZ-Container) oder 7/8-Datei (.gp = Zip), beide mit
score.gpif.

Gegenstück zu tools/gpdrums/GpDrums.java (das nur GP3/4/5 über TuxGuitar liest): gleiche Ausgabe,
damit gp2json.py sie weiterverarbeitet. TuxGuitar 1.5.6 liest .gpx zwar auch, übergeht dabei aber die
alternativen Enden und zählt einen Durchgang zu viel. Schlagzeugklänge, für die Guitar Pro keine
General-MIDI-Note hat (Rimshot, halb offene Hi-Hat, abgestoppte Becken …), werden zur nächsten GM-Note;
das und was wegfällt, steht als Zeile "HINWEIS: …" am Ende. Nur Standardbibliothek, liest nur.

Aufruf (immer mit python3 -I):
  gpif_drums.py <datei.gp|.gpx>                  TRACK/BAR/MARKER/HIT-Zeilen wie GpDrums.java (960 Ticks je Viertel)
  gpif_drums.py <datei.gp|.gpx> --summary        Spuren, Takte, Schläge, Stimmen, Muster-Vielfalt
  gpif_drums.py <datei.gp|.gpx> --bar N          Takt N der Schlagzeugspur als Positionen (Viertel) je HZ-Stimme
  gpif_drums.py <datei.gp|.gpx> --track T        andere Spur als die erste Drumkit-Spur
"""
import os
import sys
import zipfile
from collections import Counter, defaultdict
from fractions import Fraction
from math import isfinite
import xml.etree.ElementTree as ET

TPQ = 960
NOTE_VALUE = {"Whole": Fraction(4), "Half": Fraction(2), "Quarter": Fraction(1), "Eighth": Fraction(1, 2),
              "16th": Fraction(1, 4), "32nd": Fraction(1, 8), "64th": Fraction(1, 16), "128th": Fraction(1, 32)}
DYN = {"PPP": 15, "PP": 31, "P": 47, "MP": 63, "MF": 79, "F": 95, "FF": 111, "FFF": 127}
VOICE = {   # wie tools/gpdrums/gp2json.py
    35: "kick", 36: "kick", 38: "snare", 40: "snare", 39: "snare", 37: "snare.side",
    42: "hihat", 46: "hihat.open", 44: "hihat.pedal",
    49: "crash1", 57: "crash2", 52: "china", 55: "splash",
    51: "ride", 59: "ride", 53: "ride.bell",
    48: "tom1", 50: "tom1", 45: "tom2", 47: "tom2", 43: "tom3", 41: "tom4", 56: "cowbell",
}


# Guitar Pro 6 drum kit: element -> its sound by variation (MIDI, above 87 as Guitar Pro 7 numbers them); checked
# against the same song saved as GP5 (alphaTab's conversion test): GP6 has no extra element for the low floor tom 41/43
GP6_KIT = {0: (36, 36, 36), 1: (38, 91, 37), 2: (99, 100, 99), 3: (56, 100, 56), 4: (102, 103, 102), 5: (41, 41, 41),
           6: (45, 45, 45), 7: (47, 47, 47), 8: (48, 48, 48), 9: (50, 50, 50), 10: (42, 92, 46), 11: (44, 44, 44),
           12: (57, 98, 57), 13: (49, 97, 49), 14: (55, 95, 55), 15: (51, 93, 53), 16: (52, 96, 52)}
# Guitar Pro 6/7/8 drum sounds above the General-MIDI range: (GM note, what it is, what it becomes); None = exact
GP_SOUND = {91: (38, "Snare-Rimshot", "Snare"), 92: (46, "halb offene Hi-Hat", "offene Hi-Hat"), 93: (51, "Ride am Rand", "Ride"),
            94: (51, "abgestoppte Ride", "Ride"), 95: (55, "abgestopptes Splash", "Splash"), 96: (52, "abgestopptes China", "China"),
            97: (49, "abgestopptes Crash", "Crash"), 98: (57, "abgestopptes Crash", "Crash"), 99: (56, "tiefe Cowbell", "Cowbell"),
            100: (56, "Cowbell an der Spitze", "Cowbell"), 101: (56, "Cowbell an der Spitze", "Cowbell"),
            102: (56, "hohe Cowbell", "Cowbell"), 103: (56, "Cowbell an der Spitze", "Cowbell"),
            121: (38, "Orchester-Snare", "Snare"), 126: (59, None, None), 127: (53, None, None)}


def gpx_files(data, limit=64 << 20):
    """Die Dateien eines Guitar-Pro-6-Containers: BCFZ = komprimiert (Verfahren wie in alphaTabs GpxFileSystem),
    BCFS = unkomprimiert; darin Sektoren zu 0x1000 Bytes, ein Dateieintrag hat Typ 2, Name, Größe, Sektorliste."""
    if data[:4] == b"BCFZ":
        src, expected = data[8:], int.from_bytes(data[4:8], "little")
        if expected > limit:
            raise ValueError("GP6-Container beschädigt")   # Guitar Pro 6 files are small: a size like this is damage
        out, pos, end = bytearray(), 0, (len(data) - 8) * 8

        def take(n, reverse=False):   # n bits, the first one highest; reverse: the first one lowest
            nonlocal pos
            chunk = int.from_bytes(src[pos >> 3:(pos >> 3) + 8].ljust(8, b"\0"), "big")
            v = (chunk >> (64 - (pos & 7) - n)) & ((1 << n) - 1)
            pos += n
            return int(format(v, f"0{n}b")[::-1], 2) if reverse and n else v

        while len(out) < expected and pos < end:
            if take(1):   # a copy of what was already unpacked
                word = take(4)
                offset, size = take(word, True), take(word, True)
                out += out[len(out) - offset:len(out) - offset + min(offset, size)]
            else:
                for _ in range(take(2, True)):
                    out.append(take(8))
        block = bytes(out[4:])   # without its own "BCFS"
    elif data[:4] == b"BCFS":
        block = data[4:]
    else:
        raise ValueError("kein Guitar-Pro-6-Container")
    files, sector, off, steps = {}, 0x1000, 0x1000, 0
    while off + 3 < len(block) and steps < len(block) // 0x1000 + 1:
        steps += 1
        if int.from_bytes(block[off:off + 4], "little") == 2:
            name = block[off + 4:off + 131].split(b"\0", 1)[0].decode("utf-8", "replace")
            size = int.from_bytes(block[off + 0x8C:off + 0x90], "little")
            ptr, parts = off + 0x94, []
            while len(parts) < len(block) // sector:
                idx = int.from_bytes(block[ptr + 4 * len(parts):ptr + 4 * len(parts) + 4], "little")
                if idx == 0:
                    break
                off = idx * sector   # the next entry comes after the last sector of this file
                parts.append(block[off:off + sector])
            files[name] = b"".join(parts)[:size]
        off += sector
    return files


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] in (b"BCFZ", b"BCFS"):   # Guitar Pro 6
        files = gpx_files(data)
        if "score.gpif" not in files:
            raise KeyError("score.gpif")
        return ET.fromstring(files["score.gpif"])
    with zipfile.ZipFile(path) as z:
        return ET.fromstring(z.read("Content/score.gpif"))


def text(el, tag, default=""):
    e = el.find(tag)
    return e.text.strip() if e is not None and e.text else default


def ids(s):
    return [int(x) for x in s.split()] if s else []


def rhythm_len(rh):
    q = NOTE_VALUE[text(rh, "NoteValue", "Quarter")]
    dot = rh.find("AugmentationDot")
    if dot is not None:
        n = int(dot.get("count", "1"))
        q = q * (2 - Fraction(1, 2 ** n))
    tup = rh.find("PrimaryTuplet")
    if tup is not None:
        q = q * Fraction(int(tup.get("den")), int(tup.get("num")))
    return q


def parse(root, want=None):
    tracks = []
    for t in root.find("Tracks").findall("Track"):
        iset = t.find("InstrumentSet")
        typ = text(iset, "Type") if iset is not None else ""
        ch = t.find("MidiConnection") if t.find("MidiConnection") is not None else t.find("GeneralMidi")   # GP7 / GP6
        chan = int(text(ch, "PrimaryChannel", "-1")) if ch is not None else -1
        gp6_perc = t.find("GeneralMidi") is not None and t.find("GeneralMidi").get("table") == "Percussion"
        tracks.append((int(t.get("id")), text(t, "Name"), typ == "drumKit" or chan == 9 or gp6_perc))
    drum_idx = None
    for k, (tid, name, perc) in enumerate(tracks):
        if want is not None and tid == want:
            drum_idx = k
        elif want is None and perc and drum_idx is None:
            drum_idx = k
    if drum_idx is None:
        return tracks, None
    # a master bar lists one bar per staff of every track, in track order (a piano has two): the drum track's first
    staves = [len(t.findall("Staves/Staff")) or 1 for t in root.find("Tracks").findall("Track")]
    drum_bar = sum(staves[:drum_idx])
    bars = {int(b.get("id")): b for b in root.find("Bars").findall("Bar")}
    voices = {int(v.get("id")): v for v in root.find("Voices").findall("Voice")}
    beats = {int(b.get("id")): b for b in root.find("Beats").findall("Beat")}
    notes = {int(n.get("id")): n for n in root.find("Notes").findall("Note")}
    rhythms = {int(r.get("id")): rhythm_len(r) for r in root.find("Rhythms").findall("Rhythm")}
    tempos = {}
    mt = root.find("MasterTrack")
    if mt is not None and mt.find("Automations") is not None:
        for a in mt.find("Automations").findall("Automation"):
            if text(a, "Type") == "Tempo":
                v = text(a, "Value").split()
                bpm = float(v[0]) if v else 0.0
                unit = int(v[1]) if len(v) > 1 else 2
                bpm = {1: bpm / 2, 2: bpm, 3: bpm * 1.5, 4: bpm * 2, 5: bpm * 3}.get(unit, bpm)
                if not isfinite(bpm):
                    bpm = 0.0   # no usable tempo: gp2json.py takes the others
                tempos[int(text(a, "Bar", "0"))] = bpm
    out = []
    tempo = tempos.get(0, 120.0)
    for nr, mb in enumerate(root.find("MasterBars").findall("MasterBar"), 1):
        tempo = tempos.get(nr - 1, tempo)
        num, den = (int(x) for x in text(mb, "Time", "4/4").split("/"))
        length = Fraction(4 * num, den)
        bar_ids = ids(text(mb, "Bars"))
        sec = mb.find("Section")
        marker = text(sec, "Text") if sec is not None else ""
        rep = mb.find("Repeat")
        rep_open = rep is not None and rep.get("start") == "true"
        # count: how often the part is played (as in GP5); GpDrums.java gives the repeats after the first
        rep_close = max(0, int(rep.get("count", "0")) - 1) if rep is not None and rep.get("end") == "true" else 0
        alt = sum(1 << (int(n) - 1) for n in text(mb, "AlternateEndings").split() if n.isdigit() and 0 < int(n) <= 32)
        hits = []
        if drum_bar < len(bar_ids) and bar_ids[drum_bar] in bars:
            bar = bars[bar_ids[drum_bar]]
            for vid in ids(text(bar, "Voices")):
                if vid < 0 or vid not in voices:
                    continue
                pos = Fraction(0)
                for bid in ids(text(voices[vid], "Beats")):
                    beat = beats[bid]
                    dur = rhythms[int(beat.find("Rhythm").get("ref"))]
                    gn = beat.find("GraceNotes")
                    if gn is not None and gn.text and gn.text.strip() != "":
                        skipped["Vorschlagsnoten weggelassen"] += len(ids(text(beat, "Notes")))
                        continue                  # Vorschlag: as in GpDrums.java no hit of its own, no length
                    vel = DYN.get(text(beat, "Dynamic", "MF").upper(), 79)
                    for nid in ids(text(beat, "Notes")):
                        n = notes[nid]
                        midi = None
                        ghost = acc = 0
                        tie_dest = False
                        element = variation = None
                        for p in n.iter("Property"):
                            if p.get("name") == "Midi":
                                midi = int(text(p, "Number", "-1"))
                            elif p.get("name") == "Element":
                                element = int(text(p, "Element", "-1"))
                            elif p.get("name") == "Variation":
                                variation = int(text(p, "Variation", "0"))
                        if not midi and element in GP6_KIT:   # Guitar Pro 6 drum kit: no MIDI number, a kit element
                            midi = GP6_KIT[element][variation if variation in (0, 1, 2) else 0]
                        if midi in GP_SOUND:
                            gm, was, now = GP_SOUND[midi]
                            if was:
                                skipped[f"{was} als {now} gelesen"] += 1
                            midi = gm
                        if not midi or midi < 0:
                            skipped["Noten ohne Schlagzeugklang weggelassen (Guitar Pro hat keinen dafür)"] += 1
                            continue
                        if n.find("AntiAccent") is not None:
                            ghost = 1
                        a = n.find("Accent")
                        if a is not None and a.text and int(a.text) >= 4:
                            acc = 1
                        tie = n.find("Tie")
                        if tie is not None and tie.get("destination") == "true":
                            tie_dest = True
                        if midi is not None and not tie_dest:
                            hits.append((pos, midi, vel, ghost, acc))
                    pos += dur
        out.append(dict(nr=nr, length=length, num=num, den=den, tempo=tempo, marker=marker,
                        rep_open=rep_open, rep_close=rep_close, alt=alt, hits=sorted(hits)))
    return tracks, out


skipped = Counter()   # what parse() left out or read as another sound, for the HINWEIS lines


def main():
    args = sys.argv[1:]
    path = args[0]
    want = int(args[args.index("--track") + 1]) if "--track" in args else None
    try:
        root = load(path)
    except zipfile.BadZipFile:   # neither a GP6 container nor a zip: Guitar Pro 5 and older go through GpDrums156.java
        sys.exit(f"{path}: nicht lesbar, keine Guitar-Pro-6/7/8-Datei")
    except KeyError:
        sys.exit(f"{path}: nicht lesbar, score.gpif fehlt")
    except (OSError, ET.ParseError, ValueError) as e:
        sys.exit(f"{path}: nicht lesbar ({e})")
    try:
        tracks, bars = parse(root, want)
    except (AttributeError, IndexError, KeyError, TypeError, ValueError, ZeroDivisionError) as e:   # an element missing or odd
        sys.exit(f"{path}: Aufbau nicht lesbar ({type(e).__name__}: {e})")
    if "--summary" in args:
        for tid, name, perc in tracks:
            print("TRACK %d %d %s" % (tid, 1 if perc else 0, name))
        if bars is None:
            print("NODRUMS")
            return
        nhits = sum(len(b["hits"]) for b in bars)
        withhits = [b for b in bars if b["hits"]]
        voices = Counter(VOICE.get(m, "?%d" % m) for b in bars for _, m, _, _, _ in b["hits"])
        patterns = Counter(tuple(sorted({(VOICE.get(m, "?%d" % m), p) for p, m, _, _, _ in b["hits"]})) for b in withhits)
        sigs = Counter("%d/%d" % (b["num"], b["den"]) for b in bars)
        tempi = sorted({b["tempo"] for b in bars})
        markers = [(b["nr"], b["marker"]) for b in bars if b["marker"]]
        print("Takte %d, davon mit Schlägen %d; Taktarten %s; Tempi %s" % (len(bars), len(withhits), dict(sigs), tempi))
        print("Schläge %d; Stimmen %s" % (nhits, dict(voices.most_common())))
        print("verschiedene Taktmuster %d von %d Takten; häufigstes %d-mal" % (
            len(patterns), len(withhits), patterns.most_common(1)[0][1] if patterns else 0))
        print("Marker:", markers)
        return
    if "--bar" in args:
        n = int(args[args.index("--bar") + 1])
        if bars is None or n > len(bars):
            print("NODRUMS oder Takt fehlt")
            return
        b = bars[n - 1]
        rows = defaultdict(list)
        for p, m, vel, ghost, acc in b["hits"]:
            rows[VOICE.get(m, "?%d" % m)].append("%s(v%.2f%s%s)" % (p, vel / 127.0, " ghost" if ghost else "", " akz" if acc else ""))
        print("Takt %d: %d/%d, Tempo %.1f, Marker %r" % (n, b["num"], b["den"], b["tempo"], b["marker"]))
        for v in sorted(rows):
            print("  %-12s %s" % (v, "  ".join(rows[v])))
        return
    for tid, name, perc in tracks:
        print("TRACK %d %d %s" % (tid, 1 if perc else 0, name))
    if bars is None:
        print("NODRUMS")
        return
    start = 0
    for b in bars:
        ln = int(b["length"] * TPQ)
        print("BAR %d %d %d %d %d %d %d %d %d" % (b["nr"], start, ln, b["num"], b["den"], round(b["tempo"]),
                                                   1 if b["rep_open"] else 0, b["rep_close"], b["alt"]))
        if b["marker"]:
            print("MARKER %d %s" % (b["nr"], b["marker"]))
        for p, m, vel, ghost, acc in b["hits"]:
            print("HIT %d %d %d %d %d %d" % (b["nr"], int(p * TPQ), m, vel, ghost, acc))
        start += ln
    for what, n in skipped.items():
        print(f"HINWEIS: {os.path.basename(path)}: {what} ({n} {'Schlag' if n == 1 else 'Schläge'})")


if __name__ == "__main__":
    main()
