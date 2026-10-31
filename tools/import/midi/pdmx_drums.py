#!/usr/bin/env python3
"""PDMX (MuseScore, Public Domain/CC0): Schlagzeugspuren (Kanal 10) herausziehen und mit Metadaten verbinden.

Liest PDMX.csv (Genre, Tags, Lizenz) und mid.tar.gz als Datenstrom, nichts wird entpackt. Schreibt
  <ziel>/pdmx_drums.zip   je Partitur eine MIDI-Datei nur mit Kanal 10 + Tempo/Taktart
  <ziel>/index.csv        Pfad, Titel, Genre, Tags, Lizenz, Takte, Taktarten, Tempo, Jazz-Kennzeichen

Aufruf: python3 -I pdmx_drums.py <PDMX.csv> <mid.tar.gz> <zielordner> [min_schlaege=32]
"""
import csv
import os
import re
import sys
import tarfile
import zipfile
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import midiscan as m  # noqa: E402
import lmd_drums as L  # noqa: E402

JAZZ = re.compile(r"jazz|swing|bebop|be-bop|bossa|big ?band|samba|latin|mambo|salsa|fusion|dixie|ragtime|"
                  r"charleston|cha.?cha|rumba|rhumba|songo|ballad", re.I)
KEEP = ["title", "artist_name", "composer_name", "genres", "tags", "license", "license_conflict",
        "subset:no_license_conflict", "subset:deduplicated", "rating", "n_tracks"]


def norm(p):
    return p[2:] if p.startswith("./") else p


def main(csv_path, tar_path, dst, min_hits=32):
    csv.field_size_limit(1 << 30)
    meta = {}
    with open(csv_path, encoding="utf-8", errors="replace", newline="") as f:
        for r in csv.DictReader(f):
            meta[norm(r["mid"])] = {k: r.get(k, "") for k in KEEP}
    print("Metadaten:", len(meta))
    os.makedirs(dst, exist_ok=True)
    st = Counter()
    sigs = Counter()
    with tarfile.open(tar_path, "r|gz") as tf, \
            zipfile.ZipFile(os.path.join(dst, "pdmx_drums.zip"), "w", zipfile.ZIP_DEFLATED) as zf, \
            open(os.path.join(dst, "index.csv"), "w", newline="", encoding="utf-8") as cf:
        w = csv.writer(cf)
        w.writerow(["pfad"] + KEEP + ["jazz_kennung", "schlaege", "takte_mit_schlagzeug", "taktarten",
                                      "tempo_start", "anteil_exakt_1_16_1_12", "anteil_gm_zugeordnet"])
        for mem in tf:
            if not mem.isfile() or not mem.name.lower().endswith((".mid", ".midi")):
                continue
            st["dateien"] += 1
            data = tf.extractfile(mem).read()
            if b"\x99" not in data:            # kein Note-on auf Kanal 10 möglich
                continue
            try:
                dd = L.drums_only(data)
                a = m.analyse(dd)
            except Exception:  # noqa: BLE001
                st["fehler"] += 1
                continue
            hits = [n for n in a["notes"] if n[1] == 9]
            if len(hits) < min_hits:
                continue
            ppq = a["ppq"]
            bars = m.bars_of(a, hits[-1][0])
            starts = [b[0] for b in bars]
            used, k = set(), 0
            for t, *_ in hits:
                while k + 1 < len(starts) and starts[k + 1] <= t:
                    k += 1
                used.add(k)
            rel = norm(mem.name)
            md = meta.get(rel, {k: "" for k in KEEP})
            jazz = bool(JAZZ.search(" ".join([md["genres"], md["tags"], md["title"]])))
            s = sorted({"%d/%d" % x for _, x in a["sigs"]})
            for x in s:
                sigs[x] += 1
            exact = sum(1 for t, *_ in hits if (t * 4) % ppq == 0 or (t * 3) % ppq == 0)
            mapped = sum(1 for n in hits if n[2] in m.MAP_GM)
            zf.writestr(rel, dd)
            w.writerow([rel] + [md[k] for k in KEEP] + [jazz, len(hits), len(used), " ".join(s),
                                                        round(60e6 / a["tempos"][0][1], 1),
                                                        round(exact / len(hits), 3), round(mapped / len(hits), 3)])
            st["mit_schlagzeug"] += 1
            st["takte_mit_schlagzeug"] += len(used)
            st["jazz"] += jazz
            st["jazz_takte"] += len(used) if jazz else 0
            st["ohne_lizenzkonflikt"] += md["subset:no_license_conflict"] == "True"
    print(dict(st))
    print("Taktarten (Dateien):", dict(sigs.most_common(15)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 32)
