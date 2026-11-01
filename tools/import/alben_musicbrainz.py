#!/usr/bin/env python3
"""Album, Jahr und Titelnummer je Song aus MusicBrainz, für die Ansicht Band › Album › Song (alben.json).

Aufruf (immer mit python3 -I):
  alben_musicbrainz.py <Band> [<Band> …]                 zeigt die Alben der Band mit den Songs aus dem Datenordner
  alben_musicbrainz.py <Band> … --schreiben              trägt sie in <daten>/alben.json ein (vorhandene Bands bleiben)
  alben_musicbrainz.py <Band> … --schreiben --ersetzen   … auch wenn die Band dort oder in alben/alben.json steht
  --vorbereitung        für einen Vorbereitungsordner (import_band.py): jede Band, install_band.py entscheidet je Song
  --daten <ordner>      Datenordner, sonst ~/.config/HZ-Drummer (die Songs kommen aus den Tags band:/song: der Bausteine)
  --titel "A;B;C"       Songtitel von Hand statt aus dem Datenordner
  --kuenstler <Name>    Name bei MusicBrainz, wenn er vom Bandnamen abweicht ("The Jimi Hendrix Experience")

Je Band: die Studioalben nach Erstveröffentlichung mit der Titelliste der Erstausgabe (die Stelle ist die Nummer;
Bonus-Titel späterer Ausgaben fehlen), ein Song gehört zum frühesten Album, das ihn enthält. Was auf keinem
Studioalbum steht, sucht das Skript als Aufnahme (auch Live-Alben, keine Compilations, keine Bootlegs) und trägt es
einzeln mit Nummer ein (0, wenn sie nicht feststeht). Was es nicht findet, meldet es: das landet in der
Ansicht unter „Sonstige“, bis es jemand von Hand einträgt (Aufbau in docs/FORMATE.md). Eine neue Quelle ohne Einträge
zerschießt nichts: ihre Songs bleiben flach unter der Band.
MusicBrainz: eine Anfrage je Sekunde, die Daten stehen unter CC0. Nur Standardbibliothek, braucht Internet.
Rückgabe: 0 geschrieben bzw. ausgegeben; 1 nichts gefunden; 2 MusicBrainz nicht erreichbar oder eine Antwort fehlt,
dann wird nichts geschrieben (ein halbes Ergebnis würde Songs falsch einsortieren).
"""
import collections
import glob
import json
import os
import sys
import time
import urllib.parse
import urllib.request

UA = "HZ-Drummer alben_musicbrainz/1.0 (https://github.com/dooomkopf/HZ-Drummer)"
MB = "https://musicbrainz.org/ws/2/"
SHIPPED = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "alben", "alben.json")
NOT_STUDIO = ("Compilation", "Remix", "Soundtrack", "Demo", "DJ-mix", "Mixtape/Street")
last_request = 0.0
failed = 0   # requests without an answer after all attempts: the result would be incomplete


# the same table as AlbumIndex::key in the app (src/app/AlbumIndex.cpp): a letter with signs as its base letter
FOLDED = {**dict.fromkeys("äÄáÁàÀâÂåÅãÃ", "a"), **dict.fromkeys("öÖóÓòÒôÔøØõÕ", "o"), **dict.fromkeys("üÜúÚùÙûÛ", "u"),
          **dict.fromkeys("éÉèÈêÊëË", "e"), **dict.fromkeys("íÍìÌîÎïÏ", "i"), **dict.fromkeys("ýÝÿ", "y"),
          "ß": "ss", "æ": "ae", "Æ": "ae", "ñ": "n", "Ñ": "n", "ç": "c", "Ç": "c"}


def norm(s):
    """Wie AlbumIndex::key in der App, Zeichen für Zeichen: Kleinbuchstaben, & als and, Buchstaben mit Zeichen als
    Grundbuchstabe (Tabelle FOLDED), sonst nur a–z und Ziffern; alles andere fällt weg.
    Beispiele: "Damage, Inc." → damageinc, "1,000 Eyes" → 1000eyes, "Übung ß" → ubungss, "Queensrÿche" → queensryche."""
    out = []
    for c in s:
        if c == "&":
            out.append("and")
        elif "A" <= c <= "Z" or "a" <= c <= "z" or "0" <= c <= "9":
            out.append(c.lower())
        elif c in FOLDED:
            out.append(FOLDED[c])
    return "".join(out)


def get(path, **params):
    global last_request, failed
    wait = 1.1 - (time.time() - last_request)
    if wait > 0:
        time.sleep(wait)
    url = MB + path + "?" + urllib.parse.urlencode(dict(params, fmt="json"))
    for attempt in range(5):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": UA}), timeout=60) as r:
                last_request = time.time()
                return json.load(r)
        except Exception:   # noqa: BLE001 - 503 and timeouts are normal there: wait and try again, silently
            last_request = time.time()
            time.sleep(3 * (attempt + 1))
    failed += 1
    return None


def songs_of(data, band):
    songs = {}
    for f in glob.glob(os.path.join(data, "patterns", "**", "*.json"), recursive=True):
        try:
            d = json.load(open(f, encoding="utf-8"))
        except (OSError, ValueError):
            continue
        tags = d.get("tags", [])
        b = next((t[5:].strip() for t in tags if t.lower().startswith("band:")), "")
        s = next((t[5:].strip() for t in tags if t.lower().startswith("song:")), "")
        if s and norm(b) == norm(band):
            songs.setdefault(norm(s), s)
    return songs


def artist(name):
    """Id und Name bei MusicBrainz; es zählen auch die Aliasnamen dort und ein „The“ davor oder nicht
    ("Black Dahlia Murder" findet "The Black Dahlia Murder")."""
    d = get("artist/", query=f'artist:"{name}"', limit=5)
    want = norm(name)
    artists = (d or {}).get("artists", [])
    for a in artists:   # the exact name first: "Death" is Death, not a band that once was called so
        if norm(a.get("name", "")) == want:
            return a["id"], a["name"]
    for a in artists:
        names = [a.get("name", "")] + [al.get("name", "") for al in a.get("aliases", []) or [] if isinstance(al, dict)]
        if any(norm(n) and (norm(n) == want or norm(n) == "the" + want or "the" + norm(n) == want) for n in names):
            return a["id"], a["name"]
    return None, None


def studio_albums(aid):
    albums, offset = [], 0
    while True:
        d = get("release-group", artist=aid, type="album", limit=100, offset=offset)
        if not d:
            break
        albums += [rg for rg in d.get("release-groups", []) if rg.get("primary-type") == "Album" and not rg.get("secondary-types")]
        offset += 100
        if offset >= d.get("release-group-count", 0):
            break
    return sorted(albums, key=lambda rg: rg.get("first-release-date") or "9999")


def tracklist(rgid):
    """Die Titel der Erstausgabe: unter den offiziellen Ausgaben aus dem ersten Jahr die häufigste Titelzahl (bei
    Gleichstand die kleinere, denn Bonus-Titel und einzeln gezählte Teilstücke kommen nur dazu), davon die früheste
    Platte, CD oder Datei (Ausgaben ohne Format danach; Kassetten und 8-Spur-Bänder zuletzt, sie ordnen die Titel für
    ihre Seiten neu; ein bloßes Jahr zählt als dessen Ende). Führt das erste Jahr keine Titel, gilt das früheste Jahr
    mit Titeln (mit Hinweis). Alle Datenträger in Reihenfolge, die Nummern laufen durch (Doppelalbum: Seite C beginnt
    nicht wieder bei 1). Beispiel Master of Reality 1971: Ausgaben mit 8 Titeln, eine US-Pressung mit 11 (Teilstücke
    einzeln) → die britische Platte vom 21.07.1971 mit 8. Nur Bootlegs oder Promos: keine Titel."""
    rels, offset = [], 0
    while offset < 500:   # at most five pages of a hundred
        d = get("release", **{"release-group": rgid}, inc="media", limit=100, offset=offset)
        if not d:
            break
        rels += d.get("releases", [])
        offset += 100
        if offset >= d.get("release-count", 0):
            break
    count = lambda r: sum(m.get("track-count", 0) for m in r.get("media", []))
    year_of = lambda r: (r.get("date") or "9999")[:4]
    official = sorted((r for r in rels if r.get("status") == "Official"), key=lambda r: r.get("date") or "9999")
    if not official:
        return []
    year = year_of(official[0])   # the album's first year, also when its editions list no tracks
    with_tracks = [r for r in official if count(r) > 0]
    if not with_tracks:
        return []
    first_year = [r for r in with_tracks if year_of(r) == year]
    if not first_year:
        later = year_of(with_tracks[0])
        print(f"  HINWEIS: keine Titelliste aus {year}, nehme die von {later}", file=sys.stderr, flush=True)
        first_year = [r for r in with_tracks if year_of(r) == later]
    tally = collections.Counter(count(r) for r in first_year)
    wanted = min(tally, key=lambda n: (-tally[n], n))

    def pressing(r):   # record, CD or file keep the order, a cassette or 8-track orders anew; a bare year stands for its end
        formats = [m.get("format") or "" for m in r.get("media", [])]
        rank = 2 if any(t in f for f in formats for t in ("Cassette", "8-Track")) else 0 if any(formats) else 1
        parts = (r.get("date") or "9999").split("-")
        return rank, "-".join(parts + ["99"] * (3 - len(parts)))
    best = []
    for release in sorted((r for r in first_year if count(r) == wanted), key=pressing)[:3]:   # a failed lookup: the next one
        rd = get(f"release/{release['id']}", inc="recordings")
        tracks = []
        for m in sorted((rd or {}).get("media", []), key=lambda m: m.get("position", 0)):
            tracks += [t.get("title", "") for t in sorted(m.get("tracks", []), key=lambda t: int(t.get("position", 0)))]
        if len(tracks) == wanted:
            return tracks
        if len(tracks) > len(best):
            best = tracks
    return best


def recording(artist_name, title, aid=None):
    """Das früheste offizielle Album (auch live, keine Compilation, kein Bootleg), auf dem eine Aufnahme dieses Titels erschien."""
    d = get("recording/", query=f'recording:"{title}" AND artist:"{artist_name}"', limit=25)
    found = []
    for rec in (d or {}).get("recordings", []):
        if norm(rec.get("title", "")) != norm(title):
            continue
        credited = {c.get("artist", {}).get("id") for c in rec.get("artist-credit", []) if isinstance(c, dict)}
        if aid and aid not in credited:   # the search also finds "Christian Death" for Death: only the band itself
            continue
        for rel in rec.get("releases", []):
            if rel.get("status") != "Official":   # a bootleg, a promo or an edition without status is no album of the band
                continue
            rg = rel.get("release-group", {})
            sec = rg.get("secondary-types") or []
            if rg.get("primary-type") != "Album" or any(s in sec for s in NOT_STUDIO):
                continue
            nr = 0   # the place on the first disc ("A4" on a record is 4); further discs: unknown, their count is missing
            for m in rel.get("media", []):
                for t in m.get("track", []) or []:
                    if norm(t.get("title", "")) == norm(title) and m.get("position", 1) == 1 and isinstance(m.get("track-offset"), int):
                        nr = m["track-offset"] + 1
            found.append((rel.get("date") or "9999", rg.get("title", ""), nr, "Live" in sec))
    return sorted(found)[0] if found else None


def albums_for(band, songs, artist_name):
    """Die Alben der Band als Einträge für alben.json, und die Songs, die kein Album fanden."""
    aid, aname = artist(artist_name or band)
    if not aid:
        return None, list(songs.values())
    out, assigned = [], set()
    for rg in studio_albums(aid):
        titles = tracklist(rg["id"])
        hits = [k for k in songs if k not in assigned and any(norm(t) == k for t in titles)]
        year = (rg.get("first-release-date") or "")[:4]
        print(f"  {year} {rg['title']}: {len(titles)} Titel, {len(hits)} Songs", file=sys.stderr, flush=True)
        if hits:
            out.append({"album": rg["title"], "jahr": int(year) if year.isdigit() else 0, "titel": titles})
            assigned.update(hits)
        if len(assigned) == len(songs):
            break
    missing = []
    for k, title in songs.items():
        if k in assigned:
            continue
        hit = recording(aname, title, aid)
        if not hit:
            missing.append(title)
            continue
        date, album, nr, live = hit
        entry = next((a for a in out if a["album"] == album), None)
        if entry is None:
            entry = {"album": album, "jahr": int(date[:4]) if date[:4].isdigit() else 0}
            if live:
                entry["live"] = True
            out.append(entry)
        entry.setdefault("nummern", {})[title] = nr
        print(f"  Aufnahme: {title} → {date[:4]} {album}" + (" (live)" if live else ""), file=sys.stderr, flush=True)
    out.sort(key=lambda a: (a["jahr"] or 9999, a["album"]))
    return out, missing


def write(path, bands, replace, staging=False):
    """In <daten>/alben.json: neue Bands dazu; eine Band, die schon drinsteht oder die alben/alben.json mitbringt
    (Schreibweise egal), bleibt, außer mit --ersetzen: die eigene Datei gewinnt je Song, eine ganze Titelliste aus einer
    anderen Ausgabe würde die mitgelieferten Nummern überschreiben. staging (--vorbereitung): jede Band, das Einspielen
    entscheidet je Song. Eine Datei, die nicht den Aufbau {"bands": {…}} hat, wird nicht angerührt."""
    out = {"formatVersion": 1, "bands": {}}
    if os.path.exists(path):
        try:
            out = json.load(open(path, encoding="utf-8"))
        except ValueError:
            sys.exit(f"{path} ist kein gültiges JSON, nichts geschrieben")
        if not isinstance(out, dict) or not isinstance(out.get("bands", {}), dict):
            sys.exit(f"{path} hat nicht den Aufbau {{\"bands\": {{…}}}}, nichts geschrieben")
        out.setdefault("formatVersion", 1)
    have = out.setdefault("bands", {})
    shipped = {}
    if not staging:
        try:
            shipped = json.load(open(SHIPPED, encoding="utf-8")).get("bands", {})
        except (OSError, ValueError, AttributeError):
            shipped = {}
    for band, albums in bands.items():
        same = [k for k in have if norm(k) == norm(band)]
        listed = [k for k in shipped if norm(k) == norm(band)]
        if (same or listed) and not replace:
            where = f"{path} (als {same[0]})" if same else f"alben/alben.json (als {listed[0]})"
            print(f"{band} steht schon in {where}, bleibt (--ersetzen überschreibt; neue Songs einer Band trägt "
                  f"install_band.py einzeln nach)", file=sys.stderr)
            continue
        for k in same:
            del have[k]
        have[band] = albums
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    os.replace(tmp, path)
    print(f"{path}: {len(have)} Bands", file=sys.stderr)


def main():
    args = sys.argv[1:]
    def option(name, default=None):
        if name in args:
            k = args.index(name)
            value = args[k + 1]
            del args[k:k + 2]
            return value
        return default
    data = os.path.expanduser(option("--daten", "~/.config/HZ-Drummer"))
    titles = option("--titel")
    artist_name = option("--kuenstler")
    do_write = "--schreiben" in args
    replace = "--ersetzen" in args
    staging = "--vorbereitung" in args
    bands = [a for a in args if not a.startswith("--")]
    if not bands:
        sys.exit(__doc__)
    listed = {}   # bands that stay as they are: checked before asking MusicBrainz anything
    if do_write and not replace and not staging:
        for f in (os.path.join(data, "alben.json"), SHIPPED):
            try:
                listed.update({norm(k): f for k in json.load(open(f, encoding="utf-8")).get("bands", {})})
            except (OSError, ValueError, AttributeError):
                pass
    result = {}
    for band in bands:
        if norm(band) in listed:
            print(f"{band} steht schon in {listed[norm(band)]}, bleibt (--ersetzen überschreibt; neue Songs einer Band "
                  f"trägt install_band.py einzeln nach)", file=sys.stderr)
            continue
        songs = {norm(t): t.strip() for t in titles.split(";") if t.strip()} if titles else songs_of(data, band)
        if not songs:
            print(f"{band}: keine Songs mit Tag band:{band} im Datenordner {data}", file=sys.stderr)
            continue
        print(f"=== {band}: {len(songs)} Songs", file=sys.stderr, flush=True)
        albums, missing = albums_for(band, songs, artist_name)
        if albums is None:
            print(f"{band}: MusicBrainz nicht erreichbar" if failed else
                  f"{band}: bei MusicBrainz nicht gefunden (--kuenstler <Name> hilft bei anderer Schreibweise)", file=sys.stderr)
            continue
        if not albums:
            print(f"HINWEIS: {band}: kein Album mit diesen Songs gefunden, die Band bleibt flach (von Hand eintragen)", file=sys.stderr)
            continue
        for title in missing:
            print(f"HINWEIS: {band} – {title}: kein Album gefunden, bleibt unter „Sonstige“ (von Hand eintragen)", file=sys.stderr)
        result[band] = albums
    if failed:
        print(f"HINWEIS: MusicBrainz nicht erreichbar oder unvollständig ({failed} Anfragen ohne Antwort), nichts "
              f"geschrieben", file=sys.stderr)
        sys.exit(2)
    if not result:
        sys.exit(1)
    if do_write:
        write(os.path.join(data, "alben.json"), result, replace, staging)
    else:
        json.dump({"formatVersion": 1, "bands": result}, sys.stdout, ensure_ascii=False, indent=1)
        print()


if __name__ == "__main__":
    main()
