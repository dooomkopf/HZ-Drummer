#!/usr/bin/env python3
"""Bereitet eine Band aus Guitar-Pro- oder MuseScore-Dateien zum Einspielen vor (nur Vorbereitungsordner, keine App-Daten).

Aufruf: import_band.py <ordner mit .gp3/.gp4/.gp5/.gpx/.gp/.mscz/.mscx> <Bandname> <staging-ordner> [--nur "Titel1;Titel2"] [--ohne "Titel;..."] [--spielweise] [--alben]

- Dekodiert jede Datei mit dem Leser für ihre Version, alle geben dieselben Textzeilen: GP3/4/5 mit
  tools/gpdrums/GpDrums156.java (TuxGuitar 1.5.6), GP6 (.gpx) und GP7/8 (.gp) mit gpif_drums.py, MuseScore
  (.mscz/.mscx) mit mscz2gpdrums.py. Alle Schlagzeugspuren zusammengeführt (z.B. Hände und Füße in getrennten Spuren).
- Fasst Fassungen eines Songs zusammen ("Titel", "Titel (2)", Groß-/Kleinschreibung) und nimmt genau eine:
  die mit Schlagzeug und den meisten verschiedenen Takten, bei Gleichstand die mit Markern, dann die längere.
- Übersetzt sie mit tools/gpdrums/gp2json.py (Wiederholungen ausgerollt, gleiche Abschnitte = ein Baustein)
  nach <staging>/patterns und <staging>/songs, Tags band:<Band>, song:<Titel>.
- Schreibt <staging>/auswahl.tsv: welche Datei je Song genommen wurde und warum.
- Was übersprungen oder vereinfacht wird (Datei defekt oder ohne Schlagzeug, Fassungen nicht genommen, Hinweise der
  Leser und von gp2json.py), steht je Fall in einer Zeile "HINWEIS: <Datei oder Song>: …".
- --alben: holt danach die Alben der Band von MusicBrainz nach <staging>/alben.json (tools/import/alben_musicbrainz.py,
  braucht Internet); install_band.py übernimmt sie in die eigene alben.json, für die Ansicht Band › Album › Song.
"""
import json, os, re, shutil, subprocess, sys, tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TG = os.environ.get("TUXGUITAR", "/opt/tuxguitar")   # TuxGuitar 1.5.6 (Archiv von tuxguitar.app), siehe docs/SETUP.md
CP = f"{TG}/lib/tuxguitar-lib.jar:{TG}/lib/tuxguitar.jar:{TG}/share/plugins/tuxguitar-gtp.jar"
READER = REPO / "tools/gpdrums/GpDrums156.java"
CONVERTER = REPO / "tools/gpdrums/gp2json.py"
PY_READERS = {".gpx": "gpif_drums.py", ".gp": "gpif_drums.py", ".mscz": "mscz2gpdrums.py", ".mscx": "mscz2gpdrums.py"}
FORMATS = (".gp3", ".gp4", ".gp5") + tuple(PY_READERS)   # GP3/4/5: GpDrums156.java
sys.path.insert(0, str(REPO / "tools/gpdrums"))
from gp2json import VOICE  # noqa: E402


class Unreadable(Exception):
    pass


def decode(path, track=None):
    reader = PY_READERS.get(Path(path).suffix.lower())
    if reader:
        cmd = [sys.executable, "-I", str(REPO / "tools/gpdrums" / reader), str(path)] + (["--track", str(track)] if track is not None else [])
    else:
        cmd = ["java", "-cp", CP, str(READER), str(path)] + ([str(track)] if track is not None else [])
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=180)
    except subprocess.TimeoutExpired:
        raise Unreadable("Zeitüberschreitung beim Lesen") from None
    except OSError as e:
        raise Unreadable(f"{'Python' if reader else 'Java'} nicht startbar ({e})") from None
    if r.returncode != 0 and reader:   # its one line: "<datei>: nicht lesbar (…)"
        raise Unreadable((r.stderr.strip().splitlines() or ["nicht lesbar"])[-1].split(": ", 1)[-1][:160])
    if r.returncode != 0:   # a broken file: the reader stops with an exception, its output would be cut off
        why = next((l.strip() for l in r.stderr.splitlines() if "Exception" in l or "Error" in l), "Java-Fehler")
        name, _, message = why.replace('Exception in thread "main" ', "").partition(": ")
        raise Unreadable("nicht lesbar: " + (name.rsplit(".", 1)[-1] + (f": {message}" if message else ""))[:160])
    return r.stdout


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


def known_hits(text):
    """Schläge auf Noten, die gp2json.py einer Trommel zuordnet."""
    return sum(1 for l in text.splitlines() if l.startswith("HIT ") and len(l.split()) > 3
               and l.split()[3].isdigit() and int(l.split()[3]) in VOICE)


def readable(name):
    """Ein Dateiname als Text: Bytes, die kein UTF-8 sind (ein alter Latin-1-Name), als Latin-1 gelesen."""
    raw = name.encode("utf-8", "surrogateescape")
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def title_of(path, band):
    name = readable(path.stem)
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
    with_albums = "--alben" in args
    if with_albums:
        args.remove("--alben")
    src, band, staging = Path(args[0]), args[1], Path(args[2])
    if not src.is_dir():
        sys.exit(f"Ordner fehlt: {src}")
    decoded_dir = staging / "decoded"
    decoded_dir.mkdir(parents=True, exist_ok=True)

    candidates = {}
    skipped = 0
    for f in sorted(p for p in src.iterdir() if p.suffix.lower() in FORMATS):
        title = title_of(f, band)
        if (only and title.lower() not in only) or (without and title.lower() in without):
            continue
        try:   # Java opens a non-ASCII name only in a UTF-8 locale, a Latin-1 name never: then a copy with a plain name
            with tempfile.TemporaryDirectory() as tmp:
                plain = f.suffix.lower() in PY_READERS or str(f).isascii()
                text = decode_all_drums(f if plain else shutil.copy(f, Path(tmp) / ("datei" + f.suffix.lower())))
            why = ("keine Schlagzeugspur" if text is None else "Schlagzeugspur ohne Schläge" if stats(text)[1] == 0
                   else "keine Schläge auf Trommeln, die HZ-Drummer kennt" if known_hits(text) == 0 else None)
        except (Unreadable, OSError) as e:   # OSError: the copy of a file without read permission
            why = f"nicht lesbar: {e.strerror}" if isinstance(e, OSError) else str(e)
        if why:
            print(f"HINWEIS: {readable(f.name)}: übersprungen, {why}")
            skipped += 1
            continue
        distinct, played, markers = stats(text)
        candidates.setdefault(title.lower(), []).append((distinct, markers > 0, played, f, title, text))

    rows = ["song\tdatei\tverschiedene_takte\tmarker\ttakte\tverworfen"]
    used = set()
    converted = 0
    for key, versions in sorted(candidates.items()):
        versions.sort(key=lambda v: (v[0], v[1], v[2]), reverse=True)
        distinct, has_markers, played, f, title, text = versions[0]
        rejected = "; ".join(readable(v[3].name) for v in versions[1:])
        rows.append(f"{title}\t{readable(f.name)}\t{distinct}\t{'ja' if has_markers else 'nein'}\t{played}\t{rejected}")
        txt = decoded_dir / (f.stem + ".txt")
        txt.write_text(text, encoding="utf-8")
        prefix = base = f"user.{slug(band)}-{slug(title)}"
        n = 2
        while prefix in used:   # another title with the same short name ("One (S&M)", "One - S&M"): it would be overwritten
            prefix, n = f"{base}-{n}", n + 1
        used.add(prefix)
        print(f"=== {title}  ({readable(f.name)})")
        if rejected:
            print(f"HINWEIS: {title}: Fassung {readable(f.name)} genommen, nicht genommen: {rejected}")
        if prefix != base:
            print(f"HINWEIS: {title}: Kurzname schon von einem anderen Song belegt, Kennung {prefix}")
        for line in text.splitlines():
            if line.startswith("HINWEIS: "):
                print(line)   # what the reader left out
        result = subprocess.run([sys.executable, "-I", str(CONVERTER), str(txt), str(staging / "patterns"),
                                 f"{band} – {title}", prefix, "--tags", f"band:{band},song:{title}"]
                                + (["--spielweise"] if by_style else []),
                                capture_output=True, text=True, errors="replace")
        if lines := [l for l in result.stdout.splitlines() if not l.startswith("  user.")]:
            print("\n".join(lines))
        if result.returncode != 0 and result.stderr.strip():
            print(result.stderr.strip())
        converted += result.returncode == 0
    (staging / "auswahl.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")
    if with_albums and converted:
        print(f"=== {band}: Alben bei MusicBrainz nachsehen")
        r = subprocess.run([sys.executable, "-I", str(REPO / "tools" / "import" / "alben_musicbrainz.py"), band,
                            "--daten", str(staging), "--schreiben", "--vorbereitung"], capture_output=True, text=True,
                           errors="replace")
        print("\n".join(l for l in r.stderr.splitlines() if l.startswith(("HINWEIS", "===", "  ")) or "nicht gefunden" in l))
        if r.returncode != 0:
            print(f"HINWEIS: {band}: keine Alben von MusicBrainz, die Songs bleiben flach unter der Band")
    print(f"Fertig: {converted} Songs umgewandelt, {skipped} Dateien übersprungen")


if __name__ == "__main__":
    main()
