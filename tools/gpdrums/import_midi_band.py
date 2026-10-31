#!/usr/bin/env python3
"""Bereitet eine Band aus MIDI-Dateien zum Einspielen vor (nur Vorbereitungsordner, keine App-Daten).

Aufruf (immer mit python3 -I):
  import_midi_band.py <ordner mit .mid> <Bandname> <staging-ordner> --dateien "Titel.mid;Titel 2.1.mid;..." [--spielweise]

- Genau die genannten Dateien, je Song eine (die Wahl trifft man vorher, z.B. mit
  tools/import/midi/midi_je_datei.py: echte Figuren statt Grundbeat, keine doppelten Fassungen).
- Jede Datei geht durch mid2gpdrums.py (Kanal 10 → Takte, Raster je Takt, Versatz in ms) und gp2json.py
  (Bausteine nach Abschnitten bzw. mit --spielweise nach Spielweise, dazu das Arrangement) nach
  <staging>/patterns und <staging>/songs, Tags band:<Band>, song:<Titel>.
- Der Titel ist der Dateiname ohne Fassungsnummer ("Fire.2.mid" → "Fire").
- Schreibt <staging>/auswahl.tsv: welche Datei je Song genommen wurde.
Einspielen danach wie bei Guitar Pro mit install_band.py (bei geschlossenem Programm).
"""
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TO_TEXT = HERE / "mid2gpdrums.py"
CONVERTER = HERE / "gp2json.py"


def readable(name):
    """Ein Dateiname als Text: Bytes, die kein UTF-8 sind (ein alter Latin-1-Name), als Latin-1 gelesen."""
    raw = name.encode("utf-8", "surrogateescape")
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def title_of(path):
    return re.sub(r"\.\d+$", "", readable(path.stem)).strip()


def slug(s):
    return re.sub(r"[^a-z0-9]+", "-", s.lower().replace("ä", "ae").replace("ö", "oe").replace("ü", "ue").replace("ß", "ss")).strip("-")


def main():
    args = sys.argv[1:]
    by_style = "--spielweise" in args
    if by_style:
        args.remove("--spielweise")
    if "--dateien" not in args:
        sys.exit(__doc__)
    k = args.index("--dateien")
    names = [n.strip() for n in args[k + 1].split(";") if n.strip()]
    del args[k:k + 2]
    src, band, staging = Path(args[0]), args[1], Path(args[2])
    decoded = staging / "decoded"
    decoded.mkdir(parents=True, exist_ok=True)

    rows = ["song\tdatei"]
    used = set()
    for name in names:
        f = src / name
        title = title_of(f)
        try:
            text = subprocess.run([sys.executable, "-I", str(TO_TEXT), str(f)], capture_output=True, text=True, errors="replace",
                                  timeout=300)
        except subprocess.TimeoutExpired:
            text = None
        if text is None or text.returncode != 0 or text.stdout.startswith("NODRUMS"):
            why = ("Zeitüberschreitung beim Lesen" if text is None else "keine Schlagzeugspur (Kanal 10)" if text.returncode == 0
                   else (text.stderr.strip().splitlines() or ["nicht lesbar"])[-1].removeprefix(f.name + ": "))
            print(f"=== {title}: übersprungen, {why} ({readable(f.name)})")
            continue
        txt = decoded / (f.stem + ".txt")
        txt.write_text(text.stdout, encoding="utf-8")
        rows.append(f"{title}\t{readable(f.name)}")
        prefix = base = f"user.{slug(band)}-{slug(title)}"
        n = 2
        while prefix in used:   # a second file with the same short name: its Bausteine would overwrite the first
            prefix, n = f"{base}-{n}", n + 1
        used.add(prefix)
        print(f"=== {title}  ({readable(f.name)})" + (f", Kennung {prefix} (Kurzname schon vergeben)" if prefix != base else ""))
        for line in text.stdout.splitlines():
            if line.startswith("HINWEIS: "):
                print(line)   # what the reader left out, for the import log
        result = subprocess.run([sys.executable, "-I", str(CONVERTER), str(txt), str(staging / "patterns"),
                                 f"{band} – {title}", prefix,
                                 "--tags", f"band:{band},song:{title}"] + (["--spielweise"] if by_style else []),
                                capture_output=True, text=True, errors="replace")
        if lines := [l for l in result.stdout.splitlines() if not l.startswith("  user.")]:
            print("\n".join(lines))
        if result.returncode != 0:
            print(result.stderr.strip())
    (staging / "auswahl.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
