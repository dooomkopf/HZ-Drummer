#!/usr/bin/env python3
"""Übersetzt die Schlagzeugspur (Kanal 10) einer MIDI-Datei in die Textzeilen von GpDrums.java
(TRACK/BAR/MARKER/HIT), damit gp2json.py sie wie eine Guitar-Pro-Datei weiterverarbeitet: Zerlegung nach
Abschnitten oder Spielweise, Dynamik aus den Anschlagstärken, Arrangement.

Aufruf (immer mit python3 -I):
  mid2gpdrums.py <datei.mid> > ausgabe.txt

- Takte aus den Taktart-Wechseln der Datei, Tempo je Takt aus der Tempokarte.
- Jeder Schlag kommt auf das Raster, das im Takt besser passt (16tel oder 16tel-Triolen); die Abweichung bleibt
  als Versatz in ms im 8. Feld der HIT-Zeile (gp2json.py übernimmt ihn als "lag", höchstens ±100 ms). Ein Schlag
  kurz vor dem Taktstrich gehört zum nächsten Takt.
- Marker der Datei werden MARKER-Zeilen. Gibt es keine Schläge auf Kanal 10: nur "NODRUMS".
- Was dabei wegfällt oder vereinfacht wird, steht als Zeile "HINWEIS: …" am Ende (Versatz über 100 ms, doppelte
  Schläge, unbrauchbare Tempo- und Taktangaben, Format 2). Eine defekte Datei: eine Zeile auf stderr, Ausgang 1.
Nur Standardbibliothek; der MIDI-Leser ist tools/import/midi/midiscan.py.
"""
import os
import struct
import sys
from fractions import Fraction

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "import", "midi"))
import midiscan as ms  # noqa: E402

TPQ = 960   # Ticks je Viertel in der Ausgabe, wie GpDrums.java


def main():
    path = sys.argv[1]
    try:
        a = ms.analyse(path)
    except (ms.MidiError, struct.error, OSError) as e:
        sys.exit(f"{os.path.basename(path)}: " + ("Datei fehlt" if isinstance(e, FileNotFoundError) else
                                                   f"nicht lesbar ({e.strerror if isinstance(e, OSError) else e})"))
    ppq = a["ppq"]
    drums = [(Fraction(t), note, vel) for (t, ch, note, vel) in a["notes"] if ch == 9]
    if not drums:
        print("NODRUMS")
        return
    bars = ms.bars_of(a, max(t for t, _, _ in drums) + ppq)
    markers = [(tick, " ".join(text.split())) for tick, text in a["markers"] if text.strip()]   # one line each

    # each hit to its bar (by where it lies), then onto the grid that suits that bar better
    starts = [b[0] for b in bars]

    def bar_of(t):
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= t:
                lo = mid
            else:
                hi = mid - 1
        return lo

    per_bar = {}
    for t, note, vel in drums:
        per_bar.setdefault(bar_of(t), []).append((t, note, vel))

    hits = {}   # bar index -> [(offset in TPQ, note, vel, lag ms)]
    far = 0     # hits further than 100 ms from their place: the lag keeps only 100
    for k, notes in per_bar.items():
        start, length, num, den = bars[k]
        us = ms.tempo_at(a, int(start))
        ms_per_quarter = us / 1000.0

        def snapped(t, steps):
            q = (t - start) / ppq
            return Fraction(round(q * steps), steps)

        def error(steps):
            return sum(abs(float((t - start) / ppq - snapped(t, steps))) for t, _, _ in notes)

        steps = 6 if error(6) < 0.6 * error(4) else 4   # triplets only when they clearly fit better
        for t, note, vel in notes:
            q = snapped(t, steps)
            lag = float((t - start) / ppq - q) * ms_per_quarter
            target = k
            if q * ppq >= length and k + 1 < len(bars):   # just before the bar line: the next bar's first beat
                target, q = k + 1, Fraction(0)
            far += abs(lag) > 100.0
            hits.setdefault(target, []).append((int(q * TPQ), note, vel, max(-100.0, min(100.0, lag))))

    doubled = 0
    print("TRACK 0 1 Drums")
    for k, (start, length, num, den) in enumerate(bars):
        bpm = round(60_000_000 / ms.tempo_at(a, int(start)))
        print(f"BAR {k + 1} {int(start * TPQ / ppq)} {int(length * TPQ / ppq)} {num} {den} {bpm} 0 0 0")
        for tick, text in markers:
            if start <= tick < start + length:
                print(f"MARKER {k + 1} {text}")
        seen = set()
        for off, note, vel, lag in sorted(hits.get(k, []), key=lambda h: (h[0], h[1], -h[2])):
            if (off, note) in seen:
                doubled += 1
                continue   # the same drum twice on one place: the louder stays
            seen.add((off, note))
            print(f"HIT {k + 1} {off} {note} {vel} 0 0 {lag:.1f}")
    name = os.path.basename(path)
    if far:
        print(f"HINWEIS: {name}: {far} Schläge mehr als 100 ms neben dem Raster, ihr Versatz auf 100 ms begrenzt")
    if doubled:
        print(f"HINWEIS: {name}: {doubled} Schläge derselben Trommel auf einem Rasterplatz zusammengelegt (der lautere bleibt)")
    for what, n in a["ignored"].items():
        print(f"HINWEIS: {name}: {what} unbrauchbar, ignoriert" + (f" ({n}×)" if n > 1 else ""))
    if a["format"] == 2:
        print(f"HINWEIS: {name}: MIDI-Format 2, die Spuren als Teile hintereinander gelesen")


if __name__ == "__main__":
    main()
