#!/usr/bin/env python3
"""Impro-Visor-Stile (.sty) lesen: Schlagzeug-Muster als Positionen in Vierteln.

Tokens je Instrument: X<dauer> = Schlag, R<dauer> = Pause, V<n> = Lautstärke für die folgenden Schläge.
Dauer: 4 = Viertel, 8 = Achtel, 8/3 = Achteltriole, 4. = punktiert, 4+8 = gebunden.

Aufruf (python3 -I):
  impro_sty.py stats <styles-ordner>          Übersicht je Stil (Muster, Längen, Swing, Kommentar)
  impro_sty.py show  <datei.sty> [nr]         Muster nr (ab 1) als Positionen je Instrument
"""
import os
import re
import sys
from fractions import Fraction

GM_NAME_TO_VOICE = {
    "Acoustic_Bass_Drum": "kick", "Bass_Drum_1": "kick", "Acoustic_Snare": "snare", "Electric_Snare": "snare",
    "Side_Stick": "snare.side", "Closed_Hi-Hat": "hihat", "Open_Hi-Hat": "hihat.open", "Pedal_Hi-Hat": "hihat.pedal",
    "Ride_Cymbal_1": "ride", "Ride_Cymbal_2": "ride", "Ride_Bell": "ride.bell", "Crash_Cymbal_1": "crash1",
    "Crash_Cymbal_2": "crash2", "Chinese_Cymbal": "china", "Splash_Cymbal": "splash", "Cowbell": "cowbell",
    "High_Tom": "tom1", "Hi-Mid_Tom": "tom1", "Low-Mid_Tom": "tom2", "Low_Tom": "tom2",
    "High_Floor_Tom": "tom3", "Low_Floor_Tom": "tom4",
}


def dur(tok):
    """'8/3+4.' -> Fraction in Vierteln."""
    total = Fraction(0)
    for part in tok.split("+"):
        dotted = part.endswith(".")
        part = part.rstrip(".")
        m = re.fullmatch(r"(\d+)(?:/(\d+))?", part)
        if not m:
            raise ValueError("Dauer " + tok)
        d = Fraction(4, int(m.group(1)))
        if m.group(2):                       # n-Tole: n Noten in der Zeit der nächstkleineren Zweierpotenz
            n = int(m.group(2))
            d = d * (1 << (n.bit_length() - 1)) / n      # 8/3: drei Achteltriolen = zwei Achtel
        if dotted:
            d = d * Fraction(3, 2)
        total += d
    return total


def parse_line(tokens):
    pos = Fraction(0)
    vel = 127
    hits = []
    for t in tokens:
        if t[0] == "V":
            vel = int(t[1:])
            continue
        kind, rest = t[0], t[1:]
        rest = rest.rstrip("C")              # ein Ausreißer „X4C“ in den Daten
        d = dur(rest)
        if kind == "X":
            hits.append((pos, vel))
        pos += d
    return hits, pos


def parse_style(path):
    txt = open(path, encoding="utf-8", errors="replace").read()
    name = re.search(r"\(name\s+([^)]+)\)", txt)
    swing = re.search(r"\(swing\s+([\d.]+)\)", txt)
    comment = re.search(r"\(comments\s+([^)]*)\)", txt)
    pats = []
    for blk in re.finditer(r"\(drum-pattern(.*?)\(weight\s+([\d.]+)\)", txt, re.S):
        lines = {}
        length = Fraction(0)
        # zwei Schreibweisen: (drum Inst X4 R8 …)  und  (drum Inst (name …)(rules X4 R8 …))
        for dm in re.finditer(r"\(drum\s+([^\s()]+)\s*(?:\(name[^)]*\)\s*)?(?:\(rules\s+([^)]*)\)|([^()]*))\)", blk.group(1)):
            hits, ln = parse_line((dm.group(2) if dm.group(2) is not None else dm.group(3)).split())
            lines.setdefault(dm.group(1), []).extend(hits)
            length = max(length, ln)
        pats.append({"lines": lines, "length": length, "weight": float(blk.group(2))})
    return {"name": name.group(1).strip() if name else os.path.basename(path),
            "swing": float(swing.group(1)) if swing else None,
            "comment": comment.group(1).strip() if comment else "", "patterns": pats}


def cmd_stats(root):
    tot = 0
    for f in sorted(os.listdir(root)):
        if not f.endswith(".sty"):
            continue
        s = parse_style(os.path.join(root, f))
        lens = sorted({str(p["length"]) for p in s["patterns"]}, key=lambda x: Fraction(x))
        tot += len(s["patterns"])
        print("%-26s Muster %3d  Längen(Viertel) %-14s swing %-5s %s" % (
            f[:-4], len(s["patterns"]), ",".join(lens), s["swing"], s["comment"][:60]))
    print("Muster gesamt:", tot)


def cmd_show(path, nr=1):
    s = parse_style(path)
    p = s["patterns"][nr - 1]
    print("Stil %s, Muster %d von %d, Länge %s Viertel, swing %s, Gewicht %s" % (
        s["name"], nr, len(s["patterns"]), p["length"], s["swing"], p["weight"]))
    for inst, hits in p["lines"].items():
        v = GM_NAME_TO_VOICE.get(inst, "– (" + inst + ")")
        print("  %-12s %s" % (v, "  ".join("%s(v%.2f)" % (pos, vel / 127) for pos, vel in sorted(hits))))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    if sys.argv[1] == "stats":
        cmd_stats(sys.argv[2])
    else:
        cmd_show(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 1)
