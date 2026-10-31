#!/usr/bin/env python3
"""Baut das Startpaket, das ein frischer Install beim ersten Start in seinen Datenordner legt.

Aufruf (immer mit python3 -I, bei geschlossenem oder laufendem Programm, es liest nur):
  tools/startpaket.py <datenordner>          z. B. ~/.config/HZ-Drummer oder eine Sicherung davon

- community/groove-midi: die Groove-MIDI-Bausteine (user.gmd-*) und ihre Arrangements (CC BY 4.0)
- community/bardet: die Bardet-Bausteine (user.bardet-*, Hydrogen-Fassung von Albert Phelipot, CC BY-SA)
- startpaket/songs: drei Beispiel-Arrangements nur aus Bibliothek-Bausteinen (eigenes Werk, AGPL)
- startpaket/ordner.json: die Ordnerbäume von Groove MIDI und R.P. Bardet aus dem Datenordner, dazu
  "Beispiele" für die Arrangements; nur Einträge, die im Paket stecken
JSON wird kompakt geschrieben (das Programm liest es genauso, das Repo bleibt kleiner).
Was wovon mitgeliefert werden darf, steht in docs/QUELLEN.md.
"""
import glob
import json
import os
import shutil
import sys
import uuid

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def compact(obj, path):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, separators=(",", ":"))


def export_set(data, name, pattern_prefix, song_prefix=None):
    out = os.path.join(REPO, "community", name)
    for old in glob.glob(os.path.join(out, "*.json")) + glob.glob(os.path.join(out, "songs", "*.json")):
        os.remove(old)
    os.makedirs(out, exist_ok=True)
    ids = set()
    for f in sorted(glob.glob(os.path.join(data, "patterns", pattern_prefix + "*.json"))):
        p = json.load(open(f, encoding="utf-8"))
        ids.add(p["id"])
        compact(p, os.path.join(out, os.path.basename(f)))
    songs = set()
    if song_prefix:
        os.makedirs(os.path.join(out, "songs"), exist_ok=True)
        for f in sorted(glob.glob(os.path.join(data, "songs", song_prefix + "*.json"))):
            s = json.load(open(f, encoding="utf-8"))
            refs = [p.get("source") or p.get("pattern") for p in s.get("parts", []) if p.get("type", "pattern") == "pattern"]
            if all(r in ids for r in refs):   # only arrangements whose blocks all come with the set
                songs.add("song." + s["id"])
                compact(s, os.path.join(out, "songs", os.path.basename(f)))
    print(f"community/{name}: {len(ids)} Bausteine, {len(songs)} Arrangements")
    return ids, songs


def example(song_id, name, tempo, parts):
    blocks = []
    for label, pid, repeats in parts:
        copy = json.load(open(os.path.join(REPO, "library", pid + ".json"), encoding="utf-8"))
        blocks.append({"uid": uuid.uuid4().hex[:12], "type": "pattern", "label": label, "repeats": repeats,
                       "source": pid, "copy": copy})
    return {"formatVersion": 2, "id": song_id, "name": name, "tempo": tempo, "parts": blocks}


EXAMPLES = [
    example("beispiel-thrash", "Beispiel: Thrash-Song", 180.0, [
        ("Intro", "lib.thrash-half-time-groove", 4),
        ("Strophe", "lib.thrash-polka-4-takte-mit-snare-wirbel", 2),
        ("Refrain", "lib.thrash-gallop-4-takte-mit-snare-fill", 2),
        ("Strophe", "lib.thrash-polka-4-takte-mit-snare-wirbel", 2),
        ("Refrain", "lib.thrash-gallop-4-takte-mit-snare-fill", 2),
        ("Breakdown", "lib.thrash-breakdown", 4),
        ("Solo", "lib.thrash-beat-mit-16tel-doppelbass", 8),
        ("Refrain", "lib.thrash-gallop-4-takte-mit-snare-fill", 2),
        ("Schluss", "lib.thrash-polka-crash-viertel", 2),
    ]),
    example("beispiel-prog", "Beispiel: Prog in ungeraden Takten", 140.0, [
        ("Intro 7/8", "lib.prog-7-8-2-2-3", 4),
        ("Thema 7/8", "lib.prog-7-8-4-takte-mit-tom-fill", 2),
        ("Strophe 5/4", "lib.prog-5-4-groove", 4),
        ("Wechsel 5/4 + 6/8", "lib.prog-5-4-6-8-wechsel", 2),
        ("Mittelteil 9/8", "lib.prog-9-8-2-2-2-3", 4),
        ("Thema 7/8", "lib.prog-7-8-4-takte-mit-tom-fill", 2),
        ("Schluss 11/8", "lib.prog-11-8-3-3-3-2", 2),
    ]),
    example("beispiel-doom", "Beispiel: Doom", 70.0, [
        ("Intro", "lib.doom-crash-teppich", 2),
        ("Strophe", "lib.doom-half-time-schwer", 4),
        ("Übergang", "lib.doom-4-takte-mit-schwerem-tom-bergang", 1),
        ("Refrain", "lib.doom-ride-groove", 4),
        ("Mittelteil", "lib.doom-tom-groove", 4),
        ("Übergang", "lib.doom-4-takte-mit-schwerem-tom-bergang", 1),
        ("Refrain", "lib.doom-ride-groove", 4),
        ("Schluss", "lib.doom-crash-teppich", 2),
    ]),
]


def subtree(tree, area, name, keep):
    """Ordner `name` aus dem Baum, nur mit Einträgen aus `keep`, leere Ordner fallen weg."""
    def prune(node):
        items = [i for i in node.get("items", []) if i in keep]
        folders = [f for f in (prune(x) for x in node.get("folders", [])) if f is not None]
        if not items and not folders:
            return None
        out = {"name": node["name"], "folders": folders, "items": items}
        return out
    for f in tree.get(area, {}).get("folders", []):
        if f["name"] == name:
            return prune(f)
    return None


def main():
    data = os.path.expanduser(sys.argv[1])
    gmd_ids, gmd_songs = export_set(data, "groove-midi", "user.gmd-", "gmd-")
    bardet_ids, _ = export_set(data, "bardet", "user.bardet-")

    pack = os.path.join(REPO, "startpaket")
    shutil.rmtree(os.path.join(pack, "songs"), ignore_errors=True)
    os.makedirs(os.path.join(pack, "songs"))
    for s in EXAMPLES:
        compact(s, os.path.join(pack, "songs", s["id"] + ".json"))
    print(f"startpaket/songs: {len(EXAMPLES)} Beispiel-Arrangements")

    tree = json.load(open(os.path.join(data, "ordner.json"), encoding="utf-8"))
    pattern_folders = [t for t in (subtree(tree, "patterns", "Groove MIDI", gmd_ids),
                                   subtree(tree, "patterns", "R.P. Bardet", bardet_ids)) if t]
    song_folders = [t for t in (subtree(tree, "songs", "Groove MIDI", gmd_songs),) if t]
    song_folders.insert(0, {"name": "Beispiele", "folders": [], "items": ["song." + s["id"] for s in EXAMPLES]})
    start = {"formatVersion": 1, "songs": {"folders": song_folders, "items": []},
             "patterns": {"folders": pattern_folders, "items": []}}
    with open(os.path.join(pack, "ordner.json"), "w", encoding="utf-8") as f:
        json.dump(start, f, ensure_ascii=False, indent=1)
    print("startpaket/ordner.json: Bausteine", [t["name"] for t in pattern_folders], "Arrangements", [t["name"] for t in song_folders])


if __name__ == "__main__":
    main()
