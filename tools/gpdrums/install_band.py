#!/usr/bin/env python3
"""Spielt eine vorbereitete Band (patterns/ und songs/ aus gp2json) in den Datenordner ein.

Aufruf: install_band.py <staging-ordner mit patterns/ und songs/> <Bandname>
Nur bei geschlossener App (bricht sonst ab). Vorher wird der Datenordner nach /var/tmp gesichert.
Datenordner: $HZ_DRUMMER_DATA, sonst ~/.config/HZ-Drummer.
Die Songs kommen unter Arrangements in einen Ordner mit dem Bandnamen (ordner.json); die Bausteine
finden ihren Ordner über die Tags band:/song: von selbst.
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

patterns = sorted((staging / "patterns").glob("*.json"))
songs = sorted((staging / "songs").glob("*.json"))
for f in patterns:
    shutil.copy2(f, data / "patterns" / f.name)
for f in songs:
    shutil.copy2(f, data / "songs" / f.name)
print(f"{len(patterns)} Bausteine, {len(songs)} Songs kopiert")

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
