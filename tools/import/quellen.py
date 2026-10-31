#!/usr/bin/env python3
"""Quellen, die HZ-Drummer nicht mitliefert: Das Programm lädt sie auf Knopfdruck (System › Quellen) von der
Originalquelle und wandelt sie auf diesem Rechner um, nur für den eigenen Gebrauch. Mitgeliefert wird nur
dieser Code, nicht die Inhalte.

Aufruf (immer mit python3 -I):
  quellen.py liste [<datenordner>]          JSON: je Quelle Name, Beschreibung, Herkunft, Hinweis, Größe,
                                            ob sie hier geht ("nutzbar", sonst "grund") und ob sie schon da ist
  quellen.py laden <id> <staging-ordner>    lädt und wandelt um nach <staging>/patterns und <staging>/songs,
                                            <staging>/ordner.json sagt, in welche Band-Ordner die Arrangements
                                            gehören. Fortschritt als Zeilen "SCHRITT …", Ende "FERTIG <bausteine>
                                            <arrangements>" oder "FEHLER …" (Ausgang 1)
Downloads liegen in ~/.cache/HZ-Drummer/quellen/<id>/ und werden beim nächsten Mal wiederverwendet. Jeder Import
schreibt ins Import-Log ~/.cache/HZ-Drummer/import.log: was kam und was wegfiel oder vereinfacht wurde (die
Zeilen "HINWEIS: …" der Umwandler). Nur Standardbibliothek.
"""
import datetime
import glob
import json
import os
import shutil
import subprocess
import sys
import tarfile
import urllib.parse
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
CACHE = Path.home() / ".cache" / "HZ-Drummer" / "quellen"
DATA = Path.home() / ".config" / "HZ-Drummer"
AGENT = {"User-Agent": "HZ-Drummer Quellen-Import"}
TG = Path(os.environ.get("TUXGUITAR", "/opt/tuxguitar"))
LOG = Path.home() / ".cache" / "HZ-Drummer" / "import.log"
log_lines = []   # for the import log: what the converters said, without their per-file lists

MT_FILES = ["8th Closed Hi-Hat", "8th Ride", "8th China", "4th Closed Hi-Hat", "4th Ride", "4th China",
            "16th Closed Hi-Hat", "8th Closed Hi-Hat 6_8", "8th Ride 6_8", "8th China 6_8", "16th Closed Hi-Hat 6_8",
            "8th Closed Hi-Hat SHUFFLE", "8th Ride SHUFFLE", "8th China SHUFFLE", "4th Tom Grooves", "8th Tom Grooves",
            "8th _ 16th Tom Grooves", "8th Tom Grooves 6_8"]
HYDROGEN_SONGS = ["ACDC/Back_in_Black/Back_in_Black.h2song", "Led_Zeppelin/Immigrant_Song/Immigrant_Song.h2song",
                  "Nirvana/Smells_like_teen_spirit/Smells_Like_Teen_Spirit.h2song", "ZZTop/LaGrange/LaGrange.h2song"]
GP_BANDS = [("Atheist", "A/Atheist", ["Atheist - Mineral.gp4", "Atheist - Piece Of Time.gp4", "Atheist - Unholy War.gp4"]),
            ("Gorguts", "G/Gorguts", ["Gorguts - Inverted.gp4", "Gorguts - Nostalgia.gp4", "Gorguts - Obscura.gp4"]),
            ("Necrophagist", "N/Necrophagist", ["Necrophagist - Seven.gp4"])]
# Metallica (bis ...And Justice for All), Death, Black Sabbath aus demselben Archiv: je Song die Fassung, die
# import_band.py unter allen dort gewählt hat, gespeichert unter sauberem Titel (das Archiv hat Tippfehler)
GP_LISTS = {}
for _line in (HERE / "quellen-guitarpro.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    _band, _path, _title = _line.split("\t")
    GP_LISTS.setdefault(_band, []).append((_path, _title))
HENDRIX = ["1983... (A Merman I Should Turn to Be).mid", "All Along The Watchtower.1.mid", "Fire.2.mid", "Foxy Lady.mid",
           "Hey Joe.1.mid", "Little Wing.mid", "Purple Haze.mid", "The Wind Cries Mary.mid",
           "Third Stone From the Sun.mid", "Wild Thing.mid"]
LAKH = "http://hog.ee.columbia.edu/craffel/lmd/clean_midi.tar.gz"

SOURCES = [
    {"id": "mt-power", "name": "MT Power Drum Kit 2: Grooves",
     "beschreibung": "rund 380 Grooves zu 8 Takten mit Hi-Hat, Ride oder China (8tel, 4tel, 16tel, 6/8, Shuffle) "
                     "und Tom-Grooves, je mit einer Fassung mit Fill",
     "herkunft": "github.com/torgeir/drumgizmo-aasimonster-reaper: die Groove-Bibliothek des Gratis-Plugins von "
                 "Manda Audio als Hydrogen-Songs",
     "hinweis": "Ohne Lizenzangabe, deshalb nicht mitgeliefert: nur für den eigenen Gebrauch.",
     "groesse": "rund 40 MB", "praefixe": ["user.mtpower-"]},
    {"id": "hydrogen-bands", "name": "Hydrogen: vier Band-Songs",
     "beschreibung": "AC/DC – Back in Black, Led Zeppelin – Immigrant Song, Nirvana – Smells Like Teen Spirit, "
                     "ZZ Top – La Grange: Bausteine je Songteil und die Arrangements",
     "herkunft": "github.com/hydrogen-music/Song-and-pattern-repository",
     "hinweis": "Die Abschriften stehen unter CC0, die Songs selbst gehören ihren Urhebern: nur für den eigenen Gebrauch.",
     "groesse": "unter 1 MB", "praefixe": ["user.acdc-", "user.led-zeppelin-", "user.nirvana-", "user.zz-top-"]},
    {"id": "gp-bands", "name": "Guitar Pro: Atheist, Gorguts, Necrophagist",
     "beschreibung": "Community-Tabs, nach Spielweise in Bausteine zu 2 bis 8 Takten zerlegt (Blast, Doppelbass, "
                     "Groove …), je Song ein Arrangement",
     "herkunft": "github.com/YetAnotherMorty/Guitar-Pro-Tabs",
     "hinweis": "Ohne Lizenzangabe: nur für den eigenen Gebrauch. Braucht Java und TuxGuitar 1.5.6 (docs/SETUP.md).",
     "groesse": "unter 1 MB", "praefixe": ["user.atheist-", "user.gorguts-", "user.necrophagist-"]},
    {"id": "gp-metallica", "name": "Guitar Pro: Metallica bis …And Justice for All",
     "beschreibung": f"Die {len(GP_LISTS['Metallica'])} Songs von Kill 'Em All, Ride the Lightning, Master of Puppets und "
                     "…And Justice for All, nach Spielweise in Bausteine zu 2 bis 8 Takten zerlegt, je Song ein Arrangement",
     "herkunft": "github.com/YetAnotherMorty/Guitar-Pro-Tabs",
     "hinweis": "Ohne Lizenzangabe: nur für den eigenen Gebrauch. Braucht Java und TuxGuitar 1.5.6 (docs/SETUP.md).",
     "groesse": "rund 2,4 MB", "praefixe": ["user.metallica-"]},
    {"id": "gp-death", "name": "Guitar Pro: Death",
     "beschreibung": f"{len(GP_LISTS['Death'])} Songs von Scream Bloody Gore bis The Sound of Perseverance, nach Spielweise "
                     "zerlegt, je Song ein Arrangement",
     "herkunft": "github.com/YetAnotherMorty/Guitar-Pro-Tabs",
     "hinweis": "Ohne Lizenzangabe: nur für den eigenen Gebrauch. Braucht Java und TuxGuitar 1.5.6 (docs/SETUP.md).",
     "groesse": "rund 2,3 MB", "praefixe": ["user.death-"]},
    {"id": "gp-black-sabbath", "name": "Guitar Pro: Black Sabbath",
     "beschreibung": f"{len(GP_LISTS['Black Sabbath'])} Songs aus allen Phasen (Ozzy, Dio, Tony Martin, Ian Gillan), nach "
                     "Spielweise zerlegt, je Song ein Arrangement",
     "herkunft": "github.com/YetAnotherMorty/Guitar-Pro-Tabs",
     "hinweis": "Ohne Lizenzangabe: nur für den eigenen Gebrauch. Braucht Java und TuxGuitar 1.5.6 (docs/SETUP.md).",
     "groesse": "rund 1,3 MB", "praefixe": ["user.black-sabbath-"]},
    {"id": "hendrix", "name": "Jimi Hendrix: zehn Songs",
     "beschreibung": "Fan-Sequenzen als MIDI, nach Spielweise zerlegt; das Spielgefühl bleibt als Versatz erhalten",
     "herkunft": "Lakh MIDI Dataset, clean_midi (colinraffel.com/projects/lmd)",
     "hinweis": "Datensatz CC BY 4.0, die Sequenzen selbst von Fans: nur für den eigenen Gebrauch. "
                "Lädt das ganze Archiv und behält nur die zehn Dateien.",
     "groesse": "234 MB Download", "praefixe": ["user.jimi-hendrix-"]},
]


def step(text):
    print("SCHRITT " + text, flush=True)


def write_log(name, result):
    LOG.parent.mkdir(parents=True, exist_ok=True)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(f"\n=== {datetime.datetime.now():%d.%m.%Y %H:%M} · {name}\n")
        for line in log_lines:
            f.write(line + "\n")
        f.write(result + "\n")


def fail(text):
    print("FEHLER " + text, flush=True)
    write_log(CURRENT, "FEHLER: " + text)
    sys.exit(1)


def download(url, target, label=None, size_hint=None):
    """url nach target (über eine .part-Datei), vorhandenes wird wiederverwendet."""
    target = Path(target)
    if target.exists() and target.stat().st_size > 0:
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    part = target.with_name(target.name + ".part")
    req = urllib.request.Request(url, headers=AGENT)
    try:
        with urllib.request.urlopen(req, timeout=60) as r, open(part, "wb") as out:
            total = int(r.headers.get("Content-Length") or size_hint or 0)
            done, shown = 0, -1
            while True:
                chunk = r.read(1 << 16)
                if not chunk:
                    break
                out.write(chunk)
                done += len(chunk)
                if label and total > 10_000_000:
                    pct = done * 100 // total
                    if pct // 5 != shown:
                        shown = pct // 5
                        step(f"{label}: {done // 1_000_000} von {total // 1_000_000} MB")
    except Exception as e:  # noqa: BLE001 - every network problem ends the same way
        part.unlink(missing_ok=True)
        fail(f"Download fehlgeschlagen ({url}): {e}")
    part.replace(target)
    return target


def raw_github(repo, path):
    return f"https://raw.githubusercontent.com/{repo}/HEAD/" + urllib.parse.quote(path)


def run(args):
    r = subprocess.run([sys.executable, "-I"] + [str(a) for a in args], capture_output=True, text=True)
    for line in r.stdout.splitlines():   # into the import log, without the per-file lists
        if line.strip() and not line.startswith(("  user.", "  songs/", "  ", "Zuordnung", "Raster:")):
            log_lines.append(line.rstrip())
    if r.returncode != 0:
        fail((r.stderr or r.stdout).strip().splitlines()[-1] if (r.stderr or r.stdout).strip() else "Umwandlung fehlgeschlagen")
    return r.stdout


def collect(parts, staging):
    """Ergebnisse der Umwandler (je patterns/ und songs/) nach staging zusammenführen; Arrangements nach Band."""
    folders = {}
    for src, band in parts:
        for kind in ("patterns", "songs"):
            for f in glob.glob(os.path.join(src, kind, "*.json")):
                (staging / kind).mkdir(parents=True, exist_ok=True)
                shutil.copy2(f, staging / kind / os.path.basename(f))
                if kind == "songs":
                    sid = "song." + json.load(open(f, encoding="utf-8"))["id"]
                    name = band or json.load(open(f, encoding="utf-8")).get("name", "").split(" – ")[0]
                    folders.setdefault(name, []).append(sid)
    (staging / "patterns").mkdir(parents=True, exist_ok=True)
    (staging / "songs").mkdir(parents=True, exist_ok=True)
    with open(staging / "ordner.json", "w", encoding="utf-8") as f:
        json.dump({"arrangements": folders}, f, ensure_ascii=False, indent=1)
    patterns = len(glob.glob(str(staging / "patterns" / "*.json")))
    songs = len(glob.glob(str(staging / "songs" / "*.json")))
    write_log(CURRENT, f"Umgewandelt: {patterns} Bausteine, {songs} Arrangements")
    print(f"FERTIG {patterns} {songs}", flush=True)


def load_mt_power(staging):
    cache = CACHE / "mt-power"
    for k, name in enumerate(MT_FILES, 1):
        step(f"MT Power: Datei {k} von {len(MT_FILES)}")
        download(raw_github("torgeir/drumgizmo-aasimonster-reaper", f"hydrogen-mt-power-drum-kit/songs/{name}.h2song"),
                 cache / "songs" / f"{name}.h2song")
    step("MT Power: Grooves umwandeln")
    out = cache / "out"
    shutil.rmtree(out, ignore_errors=True)
    run([HERE / "hydrogen" / "mt_import_all.py", cache / "songs", out, REPO / "library", DATA / "patterns"])
    collect([(out, None)], staging)


def load_hydrogen_bands(staging):
    cache = CACHE / "hydrogen-bands"
    for k, rel in enumerate(HYDROGEN_SONGS, 1):
        step(f"Hydrogen: Song {k} von {len(HYDROGEN_SONGS)}")
        download(raw_github("hydrogen-music/Song-and-pattern-repository", "Songs/" + rel), cache / "repo" / "Songs" / rel)
    (cache / "repo" / "Songs" / "260_Drum_Machine_Patterns_from_RP_Bardet").mkdir(parents=True, exist_ok=True)  # liefert HZ selbst
    step("Hydrogen: Songs umwandeln")
    out = cache / "out"
    shutil.rmtree(out, ignore_errors=True)
    run([HERE / "hydrogen" / "h2import.py", cache / "repo", out])
    collect([(out, None)], staging)


def tuxguitar_missing():
    if shutil.which("java") is None:
        return "Java fehlt (sudo apt install default-jre)"
    if not (TG / "lib" / "tuxguitar-lib.jar").exists():
        return f"TuxGuitar 1.5.6 fehlt unter {TG} (docs/SETUP.md)"
    return None


def load_gp_bands(staging):
    if reason := tuxguitar_missing():
        fail(reason)
    cache = CACHE / "gp-bands"
    parts = []
    for band, folder, files in GP_BANDS:
        for name in files:
            step(f"Guitar Pro: {band}, {name.split(' - ', 1)[-1].rsplit('.', 1)[0]}")
            download(raw_github("YetAnotherMorty/Guitar-Pro-Tabs", f"Guitar Pro Tabs/{folder}/{name}"), cache / band / name)
        step(f"Guitar Pro: {band} umwandeln")
        out = cache / "out" / band
        shutil.rmtree(out, ignore_errors=True)
        run([REPO / "tools" / "gpdrums" / "import_band.py", cache / band, band, out, "--spielweise"])
        parts.append((out, band))
    collect(parts, staging)


def load_gp_list(band):
    """Eine Band aus quellen-guitarpro.tsv laden und umwandeln; Ordner wie die Kennung der Quelle (gp-<band>)."""
    def load(staging):
        if reason := tuxguitar_missing():
            fail(reason)
        cache = CACHE / ("gp-" + band.lower().replace(" ", "-"))
        for path, title in GP_LISTS[band]:
            step(f"Guitar Pro: {band}, {title}")
            download(raw_github("YetAnotherMorty/Guitar-Pro-Tabs", path), cache / band / f"{band} - {title}{Path(path).suffix}")
        step(f"Guitar Pro: {band} umwandeln")
        out = cache / "out"
        shutil.rmtree(out, ignore_errors=True)
        run([REPO / "tools" / "gpdrums" / "import_band.py", cache / band, band, out, "--spielweise"])
        collect([(out, band)], staging)
    return load


def load_hendrix(staging):
    cache = CACHE / "hendrix"
    midi = cache / "midi"
    missing = [n for n in HENDRIX if not (midi / n).exists()]
    if missing:
        archive = download(LAKH, cache / "clean_midi.tar.gz", "Lakh MIDI laden", 234_283_029)
        step("Lakh MIDI: die zehn Hendrix-Dateien heraussuchen")
        wanted = {f"clean_midi/Jimi Hendrix/{n}": n for n in HENDRIX}
        midi.mkdir(parents=True, exist_ok=True)
        try:
            with tarfile.open(archive, "r:gz") as tar:
                for member in tar:
                    if member.name in wanted and member.isfile():
                        with tar.extractfile(member) as src, open(midi / wanted[member.name], "wb") as out:
                            shutil.copyfileobj(src, out)
        except (tarfile.TarError, OSError) as e:
            archive.unlink(missing_ok=True)
            fail(f"Archiv nicht lesbar: {e}")
        archive.unlink(missing_ok=True)   # 234 MB: nur die zehn Dateien bleiben
        if missing := [n for n in HENDRIX if not (midi / n).exists()]:
            fail("nicht im Archiv: " + ", ".join(missing))
    step("Hendrix: umwandeln")
    out = cache / "out"
    shutil.rmtree(out, ignore_errors=True)
    run([REPO / "tools" / "gpdrums" / "import_midi_band.py", midi, "Jimi Hendrix", out, "--spielweise",
         "--dateien", ";".join(HENDRIX)])
    collect([(out, "Jimi Hendrix")], staging)


CURRENT = ""   # name of the source being loaded, for the import log

LOADERS = {"mt-power": load_mt_power, "hydrogen-bands": load_hydrogen_bands, "gp-bands": load_gp_bands,
           "gp-metallica": load_gp_list("Metallica"), "gp-death": load_gp_list("Death"),
           "gp-black-sabbath": load_gp_list("Black Sabbath"), "hendrix": load_hendrix}


def listing(data):
    have = os.listdir(data / "patterns") if (data / "patterns").is_dir() else []
    out = []
    for s in SOURCES:
        entry = {k: v for k, v in s.items() if k != "praefixe"}
        entry["vorhanden"] = any(n.startswith(p) for n in have for p in s["praefixe"])
        reason = tuxguitar_missing() if s["id"].startswith("gp-") else None   # Guitar Pro 3/4/5 needs TuxGuitar
        entry["nutzbar"] = reason is None
        entry["grund"] = reason or ""
        out.append(entry)
    print(json.dumps(out, ensure_ascii=False))


def main():
    args = sys.argv[1:]
    if args[:1] == ["liste"]:
        listing(Path(args[1]) if len(args) > 1 else DATA)
    elif args[:1] == ["laden"] and len(args) == 3 and args[1] in LOADERS:
        global CURRENT
        CURRENT = next(s["name"] for s in SOURCES if s["id"] == args[1])
        staging = Path(args[2])
        shutil.rmtree(staging, ignore_errors=True)
        staging.mkdir(parents=True)
        LOADERS[args[1]](staging)
    else:
        print(__doc__)
        sys.exit(2)


if __name__ == "__main__":
    main()
