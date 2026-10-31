#!/usr/bin/env python3
"""Doppelte Grooves über mehrere Ordner finden, unabhängig von der Noten-Belegung (Mapping).

Fingerabdruck = Folge der Anschlagzeitpunkte in Vierteln (auf 1/96 gerundet) und Anzahl
gleichzeitiger Schläge. Zwei Dateien mit gleichem Fingerabdruck gelten als gleicher Groove.

Aufruf: python3 -I dupes.py <ordner1> [<ordner2> ...]
"""
import os
import sys
from collections import defaultdict, Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import midiscan as m  # noqa: E402


def fingerprint(a):
    ppq = a["ppq"]
    c = Counter(round(t * 96 / ppq) for t, ch, n, v in a["notes"])
    return tuple(sorted(c.items()))


def main(dirs):
    seen = defaultdict(list)
    per_dir = Counter()
    for d in dirs:
        for p in m.iter_midis(d):
            try:
                a = m.analyse(p)
            except Exception as e:  # noqa: BLE001
                print("Fehler", p, e)
                continue
            if not a["notes"]:
                continue
            seen[fingerprint(a)].append((d, p))
            per_dir[d] += 1
    print("Dateien je Ordner:")
    for d in dirs:
        uniq_here = sum(1 for v in seen.values() if any(x[0] == d for x in v))
        only_here = sum(1 for v in seen.values() if all(x[0] == d for x in v))
        print("  %5d Dateien, %5d verschiedene, %5d nur hier: %s" % (per_dir[d], uniq_here, only_here, d))
    print("Verschiedene Grooves gesamt:", len(seen), "von", sum(per_dir.values()), "Dateien")


if __name__ == "__main__":
    main(sys.argv[1:])
