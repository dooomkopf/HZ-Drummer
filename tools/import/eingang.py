#!/usr/bin/env python3
"""Eingangsordner (~/HZ-Drummer-Import): eine Datei zum Einspielen vorbereiten, für die App (ImportInbox).

Aufruf (immer mit python3 -I):
  eingang.py <datei> <vorbereitungsordner> --daten <datenordner>
  eingang.py --alben <Band> <vorbereitungsordner> --daten <datenordner>   nur die Alben nachholen (nach Offline)

Die Datei heißt "Band - Titel.endung" (Guitar Pro .gp3 .gp4 .gp5 .gpx .gp, MuseScore .mscz .mscx, MIDI .mid);
ein " (2)" am Ende des Titels zählt nicht mit. Umgewandelt wird mit den Werkzeugen des Band-Imports
(tools/gpdrums/import_band.py bzw. import_midi_band.py, Bausteine nach Spielweise), die Alben kommen von MusicBrainz
(alben_musicbrainz.py). Gibt es den Song im Datenordner schon, wird es eine zweite Fassung: Kennungen und Namen
mit "-v2" (dann "-v3" …), der Tag song: bleibt, so steht die Fassung beim selben Song im Album.
Der Datenordner wird nur gelesen; einspielen tut die App (MainComponent::installFromSource).
Ergebnis im Vorbereitungsordner: patterns/, songs/, ordner.json (Arrangement in den Ordner der Band), alben.json.
Ergebnis: 0 fertig; 2 fertig, MusicBrainz aber nicht erreichbar (Alben später); 3 Datei nicht brauchbar (Format,
kein Schlagzeug, nicht lesbar); 4 weder Name noch Datei nennen Band und Titel (die Datei bleibt zum Umbenennen
liegen); 1 sonstiger Fehler. Die letzte Zeile der Ausgabe ist immer "ERGEBNIS <n>" (die App liest
das, nicht den Rückgabewert: ein Abbruch durch ein Signal oder ein fehlendes Skript wäre sonst 0 oder 2). Was geschah,
steht in der Ausgabe davor (die App schreibt sie ins Import-Log).
Band und Titel: aus dem Namen; passt er nicht, aus der Datei selbst (Guitar Pro 3–5 im Kopf, 6–8 in score.gpif,
MuseScore in den Metadaten oder Textrahmen Titel und Komponist; MIDI nur über den Namen). Bei Guitar Pro zählt nur
das Feld Interpret, nicht Musik oder Text: dort steht oft der Komponist (Bob Dylan bei Hendrix' Watchtower).
Band: steht die Band schon im Datenordner (Tags band:) oder in einer alben.json in anderer Schreibweise ("AC-DC",
"black sabbath"), gilt deren Schreibweise. Trenner im Namen: " - ", auch " – " oder " — ".
"""
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from alben_musicbrainz import norm  # noqa: E402  (the same spelling rule as the app's AlbumIndex::key)

GPDRUMS = HERE.parent / "gpdrums"
ALBEN = HERE / "alben_musicbrainz.py"
REPO = HERE.parents[1]
SCORES = {".gp3", ".gp4", ".gp5", ".gpx", ".gp", ".mscz", ".mscx"}
MIDI = {".mid", ".midi"}
NOT_USABLE, NO_NETWORK, NO_NAME = 3, 2, 4
sys.path.insert(0, str(GPDRUMS))
import gpif_drums  # noqa: E402  (Guitar Pro 6-8: score.gpif)
import mscz2gpdrums  # noqa: E402  (MuseScore)


def readable(name):
    """Ein Dateiname als Text: Bytes, die kein UTF-8 sind (ein alter Latin-1-Name), als Latin-1 gelesen."""
    raw = name.encode("utf-8", "surrogateescape")
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def band_and_title(path):
    stem = re.sub(r" [–—] ", " - ", readable(path.stem), count=1)   # a dash typed as en or em dash
    if " - " not in stem:
        return None, None
    band, title = (part.strip() for part in stem.split(" - ", 1))
    return band, re.sub(r"\s*\(\d+\)\s*$", "", title).strip()


def from_file(path):
    """Band und Titel, wie die Datei sie selbst nennt; (None, None), wenn sie es nicht tut oder nicht lesbar ist."""
    suffix = path.suffix.lower()
    try:
        if suffix in (".gp3", ".gp4", ".gp5"):   # the head: version (31 bytes), then title, subtitle, artist
            data, pos, fields = path.read_bytes()[:8192], 31, []
            for _ in range(3):
                size, length = int.from_bytes(data[pos:pos + 4], "little"), data[pos + 4]
                fields.append(data[pos + 5:pos + 5 + length].decode("cp1252", "replace").strip())
                pos += 4 + size
            return fields[2], fields[0]
        if suffix in (".gpx", ".gp"):
            root = gpif_drums.load(path)
            return gpif_drums.text(root, "Score/Artist"), gpif_drums.text(root, "Score/Title")
        if suffix in (".mscz", ".mscx"):   # the metadata, else the text frames (MuseScore 2 keeps them only there)
            root = mscz2gpdrums.load(path)
            tags = {m.get("name"): " ".join((m.text or "").split()) for m in root.iter("metaTag")}
            frames = {}
            for t in root.iter("Text"):
                style = (t.findtext("style") or t.get("style") or "").strip().lower()
                body = t.find("text") if t.find("text") is not None else t
                frames.setdefault(style, " ".join("".join(body.itertext()).split()))
            return (tags.get("composer") or frames.get("composer", ""),
                    tags.get("workTitle") or tags.get("movementTitle") or frames.get("title", ""))
    except Exception:   # noqa: BLE001 - a damaged head says nothing; the conversion reports the damage
        pass
    return None, None


def tags_of(data):
    """(band, song) aus den Tags aller Bausteine im Datenordner (nur gelesen)."""
    for f in (data / "patterns").glob("*.json"):
        try:
            tags = json.loads(f.read_text(encoding="utf-8")).get("tags", [])
        except (OSError, ValueError, AttributeError):
            continue
        b = next((t[5:].strip() for t in tags if isinstance(t, str) and t.lower().startswith("band:")), "")
        s = next((t[5:].strip() for t in tags if isinstance(t, str) and t.lower().startswith("song:")), "")
        yield b, s


def album_lists(data):
    """Die Bands der mitgelieferten und der eigenen alben.json: norm(Band) -> (Schreibweise, Songs mit Album)."""
    out = {}
    for f in (REPO / "alben" / "alben.json", data / "alben.json"):
        try:
            bands = json.loads(f.read_text(encoding="utf-8")).get("bands", {})
        except (OSError, ValueError, AttributeError):
            continue
        for band, albums in (bands.items() if isinstance(bands, dict) else []):
            spelling, songs = out.setdefault(norm(band), (band, set()))
            for a in albums if isinstance(albums, list) else []:
                if isinstance(a, dict):
                    songs.update(norm(t) for t in a.get("titel", []) if isinstance(t, str))
                    songs.update(norm(t) for t in (a["nummern"] if isinstance(a.get("nummern"), dict) else {}))
    return out


def known_spelling(band, data, lists):
    """Die Schreibweise, unter der die Band schon im Datenordner oder einer alben.json steht, sonst die gegebene."""
    for b, _ in tags_of(data):
        if b and norm(b) == norm(band):
            return b
    return lists[norm(band)][0] if norm(band) in lists else band


def run(command, timeout):
    """Ein Werkzeug laufen lassen, seine Ausgabe weiterreichen (fürs Log); Rückgabe sein Rückgabewert."""
    try:
        r = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        print(f"HINWEIS: {Path(command[2]).name} brauchte länger als {timeout} s, abgebrochen")
        return None
    for line in (r.stdout + r.stderr).splitlines():
        if not line.strip() or line.startswith(("  user.", "  songs/")):   # not the list of every Baustein
            continue
        if re.match(r"^\s+\d{4} .*: \d+ Titel, 0 Songs$", line):   # an album of the band without this song: no news
            continue
        print(line)
    return r.returncode


def convert(source, band, title, staging):
    """Umwandeln in staging/patterns und staging/songs; Rückgabe die Kennungen der Songs (leer: nichts brauchbar)."""
    with tempfile.TemporaryDirectory() as tmp:
        if source.suffix.lower() in MIDI:   # the MIDI import takes the file name as the title
            copy = Path(tmp) / f"{title}.mid"
            shutil.copyfile(source, copy)
            command = [sys.executable, "-I", str(GPDRUMS / "import_midi_band.py"), tmp, band, str(staging),
                       "--dateien", copy.name, "--spielweise"]
        else:   # the band import reads the title from the name: a plain one ("AC/DC" or "–" would spoil it)
            shutil.copyfile(source, Path(tmp) / f"x - {title.replace('/', '∕')}{source.suffix.lower()}")
            command = [sys.executable, "-I", str(GPDRUMS / "import_band.py"), tmp, band, str(staging), "--spielweise"]
        run(command, 900)
    return [json.loads(f.read_text(encoding="utf-8"))["id"] for f in sorted((staging / "songs").glob("*.json"))]


def as_new_version(staging, data, songs, band, title):
    """Gibt es einen Song oder Baustein mit derselben Kennung schon, alles mit "-v2" (oder der nächsten freien
    Nummer) umbenennen: Dateien, Kennungen, Verweise der Teile, Namen. Rückgabe die Endung ("" ohne Umbenennen)."""
    patterns = {f.stem: f for f in (staging / "patterns").glob("*.json")}
    trashed = set()   # what lies in the trash keeps its id: it can be restored
    for meta in (data / "trash").glob("*/metadata.json"):
        try:
            trashed.add(Path(json.loads(meta.read_text(encoding="utf-8")).get("originalPath", "")).as_posix())
        except (OSError, ValueError, AttributeError):
            continue
    def used(folder, ident):
        return (data / folder / f"{ident}.json").exists() or f"{folder}/{ident}.json" in trashed
    def taken(suffix):
        return (any(used("songs", f"{s}{suffix}") for s in songs)
                or any(used("patterns", renamed(p, suffix)) for p in patterns))
    def renamed(pattern_id, suffix):
        for s in songs:
            if pattern_id.startswith(f"user.{s}"):
                return f"user.{s}{suffix}{pattern_id[len(f'user.{s}'):]}"
        return pattern_id + suffix
    if not taken(""):
        return ""
    n = 2
    while taken(f"-v{n}"):
        n += 1
    suffix = f"-v{n}"
    shown, shown_new = f"{band} – {title}", f"{band} – {title}{suffix}"
    for pid, f in patterns.items():
        d = json.loads(f.read_text(encoding="utf-8"))
        d["id"], d["name"] = renamed(pid, suffix), d.get("name", "").replace(shown, shown_new, 1)
        f.unlink()
        (staging / "patterns" / f"{d['id']}.json").write_text(json.dumps(d, ensure_ascii=False, indent=1), encoding="utf-8")
    for s in songs:
        f = staging / "songs" / f"{s}.json"
        d = json.loads(f.read_text(encoding="utf-8"))
        d["id"], d["name"] = s + suffix, d.get("name", "").replace(shown, shown_new, 1)
        for part in d.get("parts", []):
            for field in ("pattern", "source"):
                if isinstance(part.get(field), str) and part[field] in patterns:
                    part[field] = renamed(part[field], suffix)
        f.unlink()
        (staging / "songs" / f"{s}{suffix}.json").write_text(json.dumps(d, ensure_ascii=False, indent=1), encoding="utf-8")
    return suffix


def albums(band, staging, titles=None):
    """Die Alben der Band nach staging/alben.json; Rückgabe 0, oder NO_NETWORK, wenn MusicBrainz nicht antwortet."""
    command = [sys.executable, "-I", str(ALBEN), band, "--daten", str(staging), "--schreiben", "--vorbereitung"]
    code = run(command + (["--titel", ";".join(titles)] if titles else []), 900)
    return NO_NETWORK if code in (None, 2) else 0


def songs_of_band(data, band):
    """Die Songs einer Band im Datenordner, nach den Tags der Bausteine (nur gelesen)."""
    found = {}
    for b, s in tags_of(data):
        if s and norm(b) == norm(band):
            found.setdefault(norm(s), s)
    return list(found.values())


def main():
    args = sys.argv[1:]
    if "--daten" not in args or len(args) < 4:
        sys.exit(__doc__)
    k = args.index("--daten")
    data = Path(args[k + 1]).expanduser()
    del args[k:k + 2]
    if args[0] == "--alben":   # after an import without network: only the albums of the band
        band, staging = args[1], Path(args[2])
        staging.mkdir(parents=True, exist_ok=True)
        titles = songs_of_band(data, band)
        print(f"Alben nachholen: {band}, {len(titles)} Songs")
        return albums(band, staging, titles) if titles else 0
    source, staging = Path(args[0]), Path(args[1])
    if source.suffix.lower() not in SCORES | MIDI:
        print(f"HINWEIS: {readable(source.name)}: Format nicht unterstützt (siehe HOWTOIMPORT.txt)")
        return NOT_USABLE
    band, title = band_and_title(source)
    if (not band or not title) and source.suffix.lower() in SCORES:
        band, title = from_file(source)
        if band and title:
            print(f"{readable(source.name)}: Band und Titel aus der Datei: {band} – {title}")
    if not band or not title:
        print(f"HINWEIS: {readable(source.name)}: weder der Name (\"Band - Titel.endung\") noch die Datei nennen Band "
              f"und Titel, bleibt liegen: im Ordner umbenennen")
        return NO_NAME
    lists = album_lists(data)
    band = known_spelling(band, data, lists)
    staging.mkdir(parents=True, exist_ok=True)
    songs = convert(source, band, title, staging)
    if not songs:
        print(f"HINWEIS: {readable(source.name)}: nichts umgewandelt (kein Schlagzeug oder nicht lesbar)")
        return NOT_USABLE
    suffix = as_new_version(staging, data, songs, band, title)
    if suffix:
        print(f"{band} – {title}: gibt es schon, wird zweite Fassung {title}{suffix}")
    (staging / "ordner.json").write_text(json.dumps({"arrangements": {band: [f"song.{s}{suffix}" for s in songs]}},
                                                    ensure_ascii=False), encoding="utf-8")
    if norm(band) in lists and norm(title) in lists[norm(band)][1]:
        print(f"{band} – {title}: Album schon bekannt")   # no need to ask MusicBrainz
        code = 0
    else:
        code = albums(band, staging)
    print(f"Fertig: {band} – {title}{suffix}" + (", Alben später (MusicBrainz nicht erreichbar)" if code else ""))
    return code


if __name__ == "__main__":
    try:
        result = main()
    except Exception as e:   # noqa: BLE001 - whatever happens goes to the log, the file to "Fehler"
        print(f"HINWEIS: Fehler beim Vorbereiten: {e!r}")
        result = 1
    print(f"ERGEBNIS {result}", flush=True)
    sys.exit(result)
