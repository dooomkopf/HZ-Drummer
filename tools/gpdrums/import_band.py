#!/usr/bin/env python3
"""Bereitet eine Band aus Guitar-Pro-Dateien zum Einspielen vor (nur Vorbereitungsordner, keine App-Daten).

Aufruf: import_band.py <ordner mit .gp3/.gp4/.gp5> <Bandname> <staging-ordner> [--nur "Titel1;Titel2"] [--ohne "Titel;..."] [--spielweise]

- Dekodiert jede Datei mit tools/gpdrums/GpDrums156.java (TuxGuitar 1.5.6), alle Schlagzeugspuren zusammengeführt
  (z.B. Hände und Füße in getrennten Spuren).
- Fasst Fassungen eines Songs zusammen ("Titel", "Titel (2)", Groß-/Kleinschreibung) und nimmt genau eine:
  die mit Schlagzeug und den meisten verschiedenen Takten, bei Gleichstand die mit Markern, dann die längere.
- Übersetzt sie mit tools/gpdrums/gp2json.py (Wiederholungen ausgerollt, gleiche Abschnitte = ein Baustein)
  nach <staging>/patterns und <staging>/songs, Tags band:<Band>, song:<Titel>.
- Schreibt <staging>/auswahl.tsv: welche Datei je Song genommen wurde und warum.
"""
import json, os, re, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TG = os.environ.get("TUXGUITAR", "/opt/tuxguitar")   # TuxGuitar 1.5.6 (Archiv von tuxguitar.app), siehe docs/SETUP.md
CP = f"{TG}/lib/tuxguitar-lib.jar:{TG}/lib/tuxguitar.jar:{TG}/share/plugins/tuxguitar-gtp.jar"
READER = REPO / "tools/gpdrums/GpDrums156.java"
CONVERTER = REPO / "tools/gpdrums/gp2json.py"


def decode(path, track=None):
    cmd = ["java", "-cp", CP, str(READER), str(path)] + ([str(track)] if track is not None else [])
    return subprocess.run(cmd, capture_output=True, text=True, timeout=180).stdout


def decode_all_drums(path):
    """Ausgabe mit den Schlägen aller Schlagzeugspuren (BAR/MARKER der ersten)."""
    first = decode(path)
    if first.startswith("NODRUMS") or "\nNODRUMS" in first:
        return None
    tracks = [int(l.split()[1]) for l in first.splitlines() if l.startswith("TRACK ") and l.split()[2] == "1"]
    if len(tracks) <= 1:
        return first
    lines = first.splitlines()
    hits = {l for l in lines if l.startswith("HIT ")}
    for t in tracks[1:]:
        hits |= {l for l in decode(path, t).splitlines() if l.startswith("HIT ")}
    head = [l for l in lines if not l.startswith("HIT ")]
    # HIT lines belong after their BAR: rebuild per bar
    by_bar = {}
    for h in hits:
        by_bar.setdefault(int(h.split()[1]), []).append(h)
    out = []
    for l in head:
        out.append(l)
        if l.startswith("BAR "):
            out += sorted(by_bar.get(int(l.split()[1]), []), key=lambda h: (int(h.split()[2]), int(h.split()[3])))
    return "\n".join(out) + "\n"


def stats(text):
    bars, cur, markers = {}, None, 0
    for l in text.splitlines():
        p = l.split()
        if not p:
            continue
        if p[0] == "BAR":
            cur = int(p[1]); bars[cur] = []
        elif p[0] == "MARKER":
            markers += 1
        elif p[0] == "HIT" and cur is not None:
            bars[int(p[1])].append((int(p[2]), int(p[3])))
    played = [tuple(sorted(v)) for v in bars.values() if v]
    return len(set(played)), len(played), markers


def title_of(path, band):
    name = path.stem
    if " - " in name:
        name = name.split(" - ", 1)[1]
    return re.sub(r"\s*\(\d+\)\s*$", "", name).strip()


def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower().replace("ä", "ae").replace("ö", "oe").replace("ü", "ue").replace("ß", "ss")).strip("-")


def main():
    args = sys.argv[1:]
    only = without = None
    if "--nur" in args:
        k = args.index("--nur"); only = {t.strip().lower() for t in args[k + 1].split(";")}; del args[k:k + 2]
    if "--ohne" in args:
        k = args.index("--ohne"); without = {t.strip().lower() for t in args[k + 1].split(";")}; del args[k:k + 2]
    by_style = "--spielweise" in args   # Bausteine nach Spielweise (gp2json.py --spielweise)
    if by_style:
        args.remove("--spielweise")
    src, band, staging = Path(args[0]), args[1], Path(args[2])
    decoded_dir = staging / "decoded"
    decoded_dir.mkdir(parents=True, exist_ok=True)

    candidates = {}
    for f in sorted(list(src.glob("*.gp3")) + list(src.glob("*.gp4")) + list(src.glob("*.gp5"))):
        title = title_of(f, band)
        if (only and title.lower() not in only) or (without and title.lower() in without):
            continue
        text = decode_all_drums(f)
        if text is None:
            continue
        distinct, played, markers = stats(text)
        if played == 0:
            continue
        candidates.setdefault(title.lower(), []).append((distinct, markers > 0, played, f, title, text))

    rows = ["song\tdatei\tverschiedene_takte\tmarker\ttakte\tverworfen"]
    for key, versions in sorted(candidates.items()):
        versions.sort(key=lambda v: (v[0], v[1], v[2]), reverse=True)
        distinct, has_markers, played, f, title, text = versions[0]
        rejected = "; ".join(v[3].name for v in versions[1:])
        rows.append(f"{title}\t{f.name}\t{distinct}\t{'ja' if has_markers else 'nein'}\t{played}\t{rejected}")
        txt = decoded_dir / (f.stem + ".txt")
        txt.write_text(text, encoding="utf-8")
        prefix = f"user.{slug(band)}-{slug(title)}"
        print(f"=== {title}  ({f.name})")
        result = subprocess.run([sys.executable, "-I", str(CONVERTER), str(txt), str(staging / "patterns"),
                                 f"{band} – {title}", prefix, "--tags", f"band:{band},song:{title}"]
                                + (["--spielweise"] if by_style else []),
                                capture_output=True, text=True)
        print("\n".join(l for l in result.stdout.splitlines() if not l.startswith("  user.")))
        if result.returncode != 0:
            print(result.stderr)
    (staging / "auswahl.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
