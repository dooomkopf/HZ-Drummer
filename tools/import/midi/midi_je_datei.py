#!/usr/bin/env python3
"""Prüft MIDI-Dateien einzeln: hat Kanal 10 echte Schlagzeugfiguren oder nur einen Grundbeat?

Aufruf (immer mit python3 -I):
  midi_je_datei.py <datei.mid|ordner> [...]

Je Datei eine Zeile:
  name | Takte mit Schlagzeug / Takte gesamt | Taktarten | Tempo | Schläge Kanal 10 | Stimmen (HZ) |
  exakt 1/16|1/12 | verschiedene Taktmuster / Takte | Takte mit Toms | Kick je Takt (Mittel/Max) | Urteil
Nutzt den Leser aus midiscan.py (daneben) (nur Standardbibliothek). Liest nur, schreibt nichts.
"""
import os
import sys
from collections import Counter
from fractions import Fraction

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # midiscan.py
import midiscan as ms  # noqa: E402


def summarize(path):
    a = ms.analyse(path)
    notes = [(t, n, v) for t, ch, n, v in a["notes"] if ch == 9]
    if not notes:
        return "%s | kein Kanal 10" % os.path.basename(path)
    ppq = a["ppq"]
    last = a["notes"][-1][0]
    bars = ms.bars_of(a, last)
    # Schläge je Takt einsortieren
    per_bar = [[] for _ in bars]
    starts = [b[0] for b in bars]
    k = 0
    for t, n, v in notes:
        while k + 1 < len(bars) and starts[k + 1] <= t:
            k += 1
        # Takt zurücksuchen, falls Noten nicht sortiert (sind sortiert, Sicherheit)
        j = k
        while j > 0 and starts[j] > t:
            j -= 1
        per_bar[j].append((t, n, v))
    voices = Counter(ms.MAP_GM.get(n, "?%d" % n) for _, n, _ in notes)
    exact = sum(1 for t, _, _ in notes if (t * 4) % ppq == 0 or (t * 3) % ppq == 0)
    sigs = Counter()
    patterns = Counter()
    drum_bars = 0
    tom_bars = 0
    kicks = []
    for (t0, blen, num, den), hits in zip(bars, per_bar):
        if not hits:
            continue
        drum_bars += 1
        sigs["%d/%d" % (num, den)] += 1
        sig = []
        nk = 0
        toms = False
        for t, n, v in hits:
            voice = ms.MAP_GM.get(n)
            if voice is None:
                continue
            q = ms.snap(Fraction(t - t0, 1) / ppq)
            sig.append((voice, q))
            if voice == "kick":
                nk += 1
            if voice.startswith("tom"):
                toms = True
        patterns[tuple(sorted(set(sig)))] += 1
        kicks.append(nk)
        tom_bars += toms
    distinct = len(patterns)
    top_share = patterns.most_common(1)[0][1] / drum_bars if drum_bars else 0
    tempo = round(60e6 / a["tempos"][0][1], 1)
    if drum_bars < 8:
        verdict = "zu kurz"
    elif distinct / drum_bars < 0.12 and top_share > 0.5:
        verdict = "Grundbeat (kaum Varianten)"
    elif distinct / drum_bars < 0.3:
        verdict = "Beat mit Fills"
    else:
        verdict = "echte Figuren"
    vs = " ".join("%s:%d" % (v, c) for v, c in voices.most_common(10))
    return "%s | %d/%d Takte | %s | %s BPM%s | %d Schläge | %s | exakt %.0f%% | %d Muster/%d Takte, häufigstes %.0f%% | Toms in %d Takten | Kick %.1f/%d | %s" % (
        os.path.basename(path), drum_bars, len(bars), " ".join(s for s, _ in sigs.most_common()), tempo,
        " (+Wechsel)" if len(set(v for _, v in a["tempos"])) > 1 else "", len(notes), vs,
        100.0 * exact / len(notes), distinct, drum_bars, 100 * top_share, tom_bars,
        sum(kicks) / len(kicks), max(kicks), verdict)


def main():
    paths = []
    for p in sys.argv[1:]:
        if os.path.isdir(p):
            paths.extend(ms.iter_midis(p))
        else:
            paths.append(p)
    for p in paths:
        try:
            print(summarize(p))
        except Exception as e:  # defekte Datei: melden, weitermachen
            print("%s | FEHLER %s" % (os.path.basename(p), e))


if __name__ == "__main__":
    main()
