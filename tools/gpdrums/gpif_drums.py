#!/usr/bin/env python3
"""Liest die Schlagzeugspur einer Guitar-Pro-7/8-Datei (.gp = Zip mit Content/score.gpif).

Gegenstück zu tools/gpdrums/GpDrums.java (das nur GP3/4/5 über TuxGuitar liest): gleiche Ausgabe,
damit gp2json.py sie weiterverarbeiten könnte. Nur Standardbibliothek, liest nur.

Aufruf (immer mit python3 -I):
  gpif_drums.py <datei.gp>                  TRACK/BAR/MARKER/HIT-Zeilen wie GpDrums.java (960 Ticks je Viertel)
  gpif_drums.py <datei.gp> --summary        Spuren, Takte, Schläge, Stimmen, Muster-Vielfalt
  gpif_drums.py <datei.gp> --bar N          Takt N der Schlagzeugspur als Positionen (Viertel) je HZ-Stimme
  gpif_drums.py <datei.gp> --track T        andere Spur als die erste Drumkit-Spur
"""
import sys
import zipfile
from collections import Counter, defaultdict
from fractions import Fraction
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


def load(path):
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
        ch = t.find("MidiConnection")
        chan = int(text(ch, "PrimaryChannel", "-1")) if ch is not None else -1
        tracks.append((int(t.get("id")), text(t, "Name"), typ == "drumKit" or chan == 9))
    drum_idx = None
    for k, (tid, name, perc) in enumerate(tracks):
        if want is not None and tid == want:
            drum_idx = k
        elif want is None and perc and drum_idx is None:
            drum_idx = k
    if drum_idx is None:
        return tracks, None
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
                bpm = float(v[0])
                unit = int(v[1]) if len(v) > 1 else 2
                bpm = {1: bpm / 2, 2: bpm, 3: bpm * 1.5, 4: bpm * 2, 5: bpm * 3}.get(unit, bpm)
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
        rep_close = int(rep.get("count", "0")) if rep is not None and rep.get("end") == "true" else 0
        hits = []
        if drum_idx < len(bar_ids) and bar_ids[drum_idx] in bars:
            bar = bars[bar_ids[drum_idx]]
            for vid in ids(text(bar, "Voices")):
                if vid < 0 or vid not in voices:
                    continue
                pos = Fraction(0)
                for bid in ids(text(voices[vid], "Beats")):
                    beat = beats[bid]
                    dur = rhythms[int(beat.find("Rhythm").get("ref"))]
                    gn = beat.find("GraceNotes")
                    if gn is not None and gn.text and gn.text.strip() != "":
                        dur = Fraction(0)         # Vorschlag: zählt nicht zur Taktlänge
                    vel = DYN.get(text(beat, "Dynamic", "MF").upper(), 79)
                    for nid in ids(text(beat, "Notes")):
                        n = notes[nid]
                        midi = None
                        ghost = acc = 0
                        tie_dest = False
                        for p in n.iter("Property"):
                            if p.get("name") == "Midi":
                                midi = int(text(p, "Number", "-1"))
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
                        rep_open=rep_open, rep_close=rep_close, hits=sorted(hits)))
    return tracks, out


def main():
    args = sys.argv[1:]
    path = args[0]
    want = int(args[args.index("--track") + 1]) if "--track" in args else None
    root = load(path)
    tracks, bars = parse(root, want)
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
        print("BAR %d %d %d %d %d %d %d %d" % (b["nr"], start, ln, b["num"], b["den"], round(b["tempo"]),
                                                1 if b["rep_open"] else 0, b["rep_close"]))
        if b["marker"]:
            print("MARKER %d %s" % (b["nr"], b["marker"]))
        for p, m, vel, ghost, acc in b["hits"]:
            print("HIT %d %d %d %d %d %d" % (b["nr"], int(p * TPQ), m, vel, ghost, acc))
        start += ln


if __name__ == "__main__":
    main()
