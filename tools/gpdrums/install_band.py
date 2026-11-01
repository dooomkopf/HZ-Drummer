#!/usr/bin/env python3
"""Spielt eine vorbereitete Band (patterns/ und songs/ aus gp2json) in den Datenordner ein.

Aufruf: install_band.py <staging-ordner mit patterns/ und songs/> <Bandname>
Nur bei geschlossener App (bricht sonst ab). Vorher wird der Datenordner nach /var/tmp gesichert.
Datenordner: $HZ_DRUMMER_DATA, sonst ~/.config/HZ-Drummer.
Die Songs kommen unter Arrangements in einen Ordner mit dem Bandnamen (ordner.json); die Bausteine
finden ihren Ordner über die Tags band:/song: von selbst. Liegt im Vorbereitungsordner eine alben.json
(import_band.py --alben), kommen Bands, die weder die mitgelieferte noch die eigene alben.json kennt, in die eigene;
bei einer Band, die eine der Listen schon kennt, nur die Songs dieses Imports, die dort noch kein Album haben, je mit
Album und Nummer. Was schon einen Platz hat, bleibt dort.
"""
import json, os, shutil, subprocess, sys, time
from pathlib import Path

if subprocess.run(["pgrep", "-x", "HZ-Drummer"], capture_output=True).returncode == 0:
    sys.exit("HZ-Drummer läuft: erst beenden, dann einspielen (das Programm schreibt sonst seinen Stand darüber).")
staging, band = Path(sys.argv[1]), sys.argv[2]
data = Path(os.environ.get("HZ_DRUMMER_DATA", Path.home() / ".config/HZ-Drummer"))
stamp = time.strftime("%Y%m%d-%H%M%S")
backup = Path(f"/var/tmp/hz-daten-backup-{stamp}-{band.lower().replace(' ', '-')}")
shutil.copytree(data, backup, ignore=shutil.ignore_patterns(".git"))
print("Sicherung:", backup)

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "import"))
from alben_musicbrainz import norm  # noqa: E402  (the same spelling rule as the app's AlbumIndex::key)

album_file = staging / "alben.json"   # checked now, before anything is copied: a damaged one is skipped, not fatal
new_albums = None
if album_file.exists():
    try:
        loaded = json.loads(album_file.read_text(encoding="utf-8"))
        new_albums = loaded.get("bands") if isinstance(loaded, dict) else None
    except ValueError:
        loaded = None
    if not isinstance(new_albums, dict):
        print(f"HINWEIS: {album_file} hat nicht den Aufbau {{\"bands\": {{…}}}}, Alben übergangen")
        new_albums = None

patterns = sorted((staging / "patterns").glob("*.json"))
songs = sorted((staging / "songs").glob("*.json"))
for f in patterns:
    shutil.copy2(f, data / "patterns" / f.name)
for f in songs:
    shutil.copy2(f, data / "songs" / f.name)
print(f"{len(patterns)} Bausteine, {len(songs)} Songs kopiert")

if new_albums:   # the albums of a band nobody knows yet: into the own alben.json, nothing overwritten
    repo = Path(__file__).resolve().parents[2]
    def bands_of(path):
        try:
            d = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
            return d.get("bands", {}) if isinstance(d, dict) and isinstance(d.get("bands", {}), dict) else None
        except ValueError:
            return None
    own_file = data / "alben.json"
    own_bands = bands_of(own_file)
    shipped = bands_of(repo / "alben" / "alben.json") or {}
    if own_bands is None:
        print(f"HINWEIS: {own_file} hat nicht den Aufbau {{\"bands\": {{…}}}}, Alben nicht ergänzt (Datei bleibt, wie sie ist)")
    else:
        own = json.loads(own_file.read_text(encoding="utf-8")) if own_file.exists() else {"formatVersion": 1, "bands": {}}
        known = {norm(k) for k in list(shipped) + list(own_bands)}
        listed = set(known)   # the bands one of the lists knew before this import
        new = {}
        for b, a in new_albums.items():
            if norm(b) not in known and isinstance(a, list):
                new[b] = a
                known.add(norm(b))   # two spellings in the staging file: the first one
        own_list = own.setdefault("bands", {})
        own_list.update(new)

        # a band the lists know already: the songs of this import without an album there get theirs from the
        # staging file, each with its number under "nummern" (as the app reads it, the own file wins per song);
        # a song that has a place keeps it, a full track list is never copied over
        def placed_songs(bands):
            out = {}
            for b, albums in bands.items():
                for a in albums if isinstance(albums, list) else []:
                    if isinstance(a, dict):
                        names = list(a.get("titel", [])) + list(a["nummern"] if isinstance(a.get("nummern"), dict) else [])
                        out.setdefault(norm(b), set()).update(norm(t) for t in names if isinstance(t, str))
            return out
        placed = placed_songs(shipped)
        for b, songs_of in placed_songs(own_bands).items():
            placed.setdefault(b, set()).update(songs_of)
        imported = {}   # norm(band) -> {norm(song): the song as tagged}
        for f in patterns:
            try:
                tags = json.loads(f.read_text(encoding="utf-8")).get("tags", [])
            except (ValueError, AttributeError):
                continue
            band_tag = next((t[5:].strip() for t in tags if isinstance(t, str) and t.lower().startswith("band:")), "")
            song_tag = next((t[5:].strip() for t in tags if isinstance(t, str) and t.lower().startswith("song:")), "")
            if norm(band_tag) and norm(song_tag):
                imported.setdefault(norm(band_tag), {}).setdefault(norm(song_tag), song_tag)
        filled = []
        for b, albums in new_albums.items():
            if norm(b) not in listed or not isinstance(albums, list):
                continue
            missing = {k: v for k, v in imported.get(norm(b), {}).items() if k not in placed.get(norm(b), set())}
            known_albums = {}   # norm(album) -> (name, year, has a track list), as the lists hold the band's albums
            for listed_bands in (shipped, own_bands):
                for k, albums_of in listed_bands.items():
                    for e in albums_of if norm(k) == norm(b) and isinstance(albums_of, list) else []:
                        if isinstance(e, dict) and isinstance(e.get("album"), str):
                            known_albums.setdefault(norm(e["album"]), (e["album"], e.get("jahr", 0), isinstance(e.get("titel"), list)))
            entries = []
            for a in albums:   # earliest first, as the tool writes them: the first album that has the song
                if not missing or not isinstance(a, dict) or not a.get("album"):
                    continue
                numbers = {}
                for i, t in enumerate(a.get("titel", []) if isinstance(a.get("titel"), list) else []):
                    if isinstance(t, str) and norm(t) in missing:
                        numbers[missing.pop(norm(t))] = i + 1
                for t, n in (a["nummern"] if isinstance(a.get("nummern"), dict) else {}).items():
                    if norm(t) in missing:
                        numbers[missing.pop(norm(t))] = n if isinstance(n, int) else 0
                if numbers:
                    if norm(a["album"]) in known_albums:   # another edition of an album the lists know: its name and year
                        name, year, track_list = known_albums[norm(a["album"])]
                        a = dict(a, album=name, jahr=year)
                        if track_list:   # the song is not on that edition's list, its number there is unknown
                            numbers = {song: 0 for song in numbers}
                    entries.append((a, numbers))
            for song in missing.values():
                print(f"HINWEIS: {b} – {song}: kein Album gefunden, bleibt unter „Sonstige“")
            if not entries:
                continue
            key = next((k for k in own_list if norm(k) == norm(b)), b)   # the band as the own file spells it
            if not isinstance(own_list.setdefault(key, []), list):
                print(f"HINWEIS: {b}: der Eintrag in {own_file} ist keine Liste, Alben nicht ergänzt")
                continue
            for a, numbers in entries:
                same = next((e for e in own_list[key] if isinstance(e, dict) and e.get("album") == a["album"]
                             and e.get("jahr", 0) == a.get("jahr", 0) and isinstance(e.get("nummern", {}), dict)), None)
                if same is None:
                    same = {"album": a["album"], "jahr": a.get("jahr", 0)}
                    if a.get("live"):
                        same["live"] = True
                    own_list[key].append(same)
                same.setdefault("nummern", {}).update(numbers)
                filled += [f"{b} – {song}: {a['album']}" for song in numbers]
        if new or filled:
            tmp = own_file.with_name(".alben.json.tmp")
            tmp.write_text(json.dumps(own, ensure_ascii=False, indent=1), encoding="utf-8")
            tmp.replace(own_file)
        print(f"alben.json: {len(new)} Bands ergänzt" + (f" ({', '.join(new)})" if new else ""))
        if filled:
            print(f"alben.json: {len(filled)} Songs bekannter Bands ergänzt ({'; '.join(filled)})")

ordner = data / "ordner.json"
tree = json.loads(ordner.read_text(encoding="utf-8"))
area = tree.setdefault("songs", {"folders": [], "items": []})
ids = ["song." + json.loads(f.read_text(encoding="utf-8"))["id"] for f in songs]
def leave(node):   # never twice: out of every folder, at any depth, before it goes into the band's
    node["items"] = [i for i in node.get("items", []) if i not in ids]
    for sub in node.get("folders", []):
        leave(sub)
leave(area)
folder = next((f for f in area["folders"] if f["name"].lower() == band.lower()), None)
if folder is None:
    folder = {"name": band, "folders": [], "items": []}
    area["folders"].append(folder)
    area["folders"].sort(key=lambda f: f["name"].lower())
folder["items"] = folder.get("items", []) + ids
tmp = ordner.with_name(".ordner.json.tmp")
tmp.write_text(json.dumps(tree, ensure_ascii=False, indent=2), encoding="utf-8")
tmp.replace(ordner)
print("ordner.json: Arrangements >", band, ids)
