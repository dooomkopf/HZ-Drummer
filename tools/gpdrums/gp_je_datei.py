#!/usr/bin/env python3
"""Prüft Guitar-Pro-Dateien (GP3/4/5) einzeln: gibt es eine Schlagzeugspur, und enthält sie echte Figuren?

Aufruf (immer mit python3 -I):
  gp_je_datei.py <datei.gp[345]|ordner> [...]          eine Prüfzeile je Datei
  gp_je_datei.py --bar N <datei.gp[345]>               Takt N der Schlagzeugspur als Positionen (Viertel) je HZ-Stimme

Ruft je Datei den Leser GpDrums156.java (TuxGuitar 1.5.6, $TUXGUITAR, sonst /opt/tuxguitar) auf
und wertet dessen TRACK/BAR/MARKER/HIT-Zeilen aus. Liest nur, schreibt nichts in die Quellen.
"""
import os
import subprocess
import sys
from collections import Counter, defaultdict
from fractions import Fraction

HERE = os.path.dirname(os.path.abspath(__file__))
TG = os.environ.get("TUXGUITAR", "/opt/tuxguitar")   # TuxGuitar 1.5.6, siehe docs/SETUP.md
CP = ":".join([f"{TG}/lib/tuxguitar-lib.jar", f"{TG}/lib/tuxguitar.jar", f"{TG}/share/plugins/tuxguitar-gtp.jar"])
READER = os.path.join(HERE, "GpDrums156.java")
TPQ = 960
VOICE = {   # wie tools/gpdrums/gp2json.py
    35: "kick", 36: "kick", 38: "snare", 40: "snare", 39: "snare", 37: "snare.side",
    42: "hihat", 46: "hihat.open", 44: "hihat.pedal",
    49: "crash1", 57: "crash2", 52: "china", 55: "splash",
    51: "ride", 59: "ride", 53: "ride.bell",
    48: "tom1", 50: "tom1", 45: "tom2", 47: "tom2", 43: "tom3", 41: "tom4", 56: "cowbell",
}


def run(path):
    p = subprocess.run(["java", "-cp", CP, READER, path], capture_output=True, text=True, timeout=180)
    return p.stdout, p.stderr


def parse(out):
    tracks, bars, markers = [], {}, {}
    for line in out.splitlines():
        p = line.split()
        if not p:
            continue
        if p[0] == "TRACK":
            tracks.append((int(p[1]), int(p[2]), " ".join(p[3:])))
        elif p[0] == "NODRUMS":
            return tracks, None, markers
        elif p[0] == "BAR":
            bars[int(p[1])] = dict(length=int(p[3]), num=int(p[4]), den=int(p[5]), tempo=int(p[6]), hits=[])
        elif p[0] == "MARKER":
            markers[int(p[1])] = " ".join(p[2:])
        elif p[0] == "HIT":
            nr, off, note, vel, ghost, acc = (int(x) for x in p[1:7])
            bars[nr]["hits"].append((off, note, vel, ghost, acc))
    return tracks, bars, markers


def summarize(path):
    out, err = run(path)
    tracks, bars, markers = parse(out)
    name = os.path.basename(path)
    if not tracks:
        return "%s | FEHLER %s" % (name, (err.strip().splitlines() or ["keine Ausgabe"])[-1][:80])
    perc = [t for t in tracks if t[1]]
    if bars is None:
        return "%s | %d Spuren, KEINE Schlagzeugspur" % (name, len(tracks))
    withhits = [b for b in bars.values() if b["hits"]]
    nhits = sum(len(b["hits"]) for b in withhits)
    if not withhits:
        return "%s | %d Spuren, Schlagzeugspur '%s' LEER (%d Takte)" % (name, len(tracks), perc[0][2], len(bars))
    voices = Counter(VOICE.get(n, "?%d" % n) for b in withhits for _, n, _, _, _ in b["hits"])
    patterns = Counter(tuple(sorted({(VOICE.get(n, "?%d" % n), Fraction(o, TPQ)) for o, n, _, _, _ in b["hits"]})) for b in withhits)
    sigs = Counter("%d/%d" % (b["num"], b["den"]) for b in bars.values())
    tempi = sorted({b["tempo"] for b in bars.values()})
    kicks = [sum(1 for _, n, _, _, _ in b["hits"] if VOICE.get(n) == "kick") for b in withhits]
    toms = sum(1 for b in withhits if any(VOICE.get(n, "").startswith("tom") for _, n, _, _, _ in b["hits"]))
    distinct = len(patterns)
    top = patterns.most_common(1)[0][1] / len(withhits)
    if len(withhits) < 8:
        verdict = "zu kurz"
    elif distinct / len(withhits) < 0.12 and top > 0.5:
        verdict = "Grundbeat (kaum Varianten)"
    elif distinct / len(withhits) < 0.3:
        verdict = "Beat mit Fills"
    else:
        verdict = "echte Figuren"
    vs = " ".join("%s:%d" % (v, c) for v, c in voices.most_common(9))
    return "%s | %d Spuren, Drums '%s' | %d/%d Takte | %s | %s BPM | %d Schläge | %s | %d Muster/%d, häufigstes %.0f%% | Toms in %d | Kick %.1f/%d | Marker %d | %s" % (
        name, len(tracks), perc[0][2] if perc else "?", len(withhits), len(bars), " ".join(s for s, _ in sigs.most_common()),
        "-".join(str(t) for t in (tempi[0], tempi[-1])) if len(tempi) > 1 else str(tempi[0]), nhits, vs, distinct, len(withhits),
        100 * top, toms, sum(kicks) / len(kicks), max(kicks), len(markers), verdict)


def show_bar(path, n):
    out, err = run(path)
    tracks, bars, markers = parse(out)
    if bars is None or n not in bars:
        print("NODRUMS oder Takt fehlt")
        return
    b = bars[n]
    rows = defaultdict(list)
    for off, note, vel, ghost, acc in sorted(b["hits"]):
        rows[VOICE.get(note, "?%d" % note)].append("%s(v%.2f%s%s)" % (Fraction(off, TPQ), vel / 127.0, " ghost" if ghost else "", " akz" if acc else ""))
    print("Datei: %s" % os.path.basename(path))
    print("Takt %d: %d/%d, Tempo %d, Marker %r" % (n, b["num"], b["den"], b["tempo"], markers.get(n, "")))
    for v in sorted(rows):
        print("  %-12s %s" % (v, "  ".join(rows[v])))


def main():
    args = sys.argv[1:]
    if args and args[0] == "--bar":
        show_bar(args[2], int(args[1]))
        return
    paths = []
    for p in args:
        if os.path.isdir(p):
            for dp, dn, fn in os.walk(p):
                dn.sort()
                paths.extend(os.path.join(dp, f) for f in sorted(fn) if f.lower().endswith((".gp3", ".gp4", ".gp5")))
        else:
            paths.append(p)
    for p in paths:
        try:
            print(summarize(p), flush=True)
        except Exception as e:  # defekte Datei: melden, weitermachen
            print("%s | FEHLER %s" % (os.path.basename(p), e), flush=True)


if __name__ == "__main__":
    main()
