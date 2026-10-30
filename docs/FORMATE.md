# Dateiformate

HZ-Drummer kennt drei Formate: **Drum-Tabs** zum Schreiben von Hand, **Pattern-JSON** als Format, das das Programm lädt und speichert, und **Song-JSON** für Abläufe aus dem Arranger. Tabs werden mit `patternc` in Pattern-JSON übersetzt.

## Wo was liegt

| Was | Ort |
|---|---|
| Mitgelieferte Patterns, Quelle | `library-src/*.tab` |
| Mitgelieferte Patterns, übersetzt | `library/*.json`, Kennungen `lib.*`, schreibgeschützt |
| Eigene Patterns | `~/.config/HZ-Drummer/patterns/*.json`, Kennungen `user.*` |
| Eigene Songs | `~/.config/HZ-Drummer/songs/*.json` |
| Papierkorb | `~/.config/HZ-Drummer/trash/<uuid>/` mit `data.json` und `metadata.json` |
| Arbeitsstand und Ablage des Arrangers | `~/.config/HZ-Drummer/arranger.json`, `ablage.json` |
| Ordnerstruktur für Songs und eigene Patterns | `~/.config/HZ-Drummer/ordner.json` |

Bei Patterns werden Unterordner mitgelesen. Jede Kennung darf nur einmal vorkommen, Doppelte werden mit Meldung ignoriert.

## Drum-Tabs (`.tab`)

Eine Datei enthält ein oder mehrere Patterns, getrennt durch eine Zeile `---`. Alles hinter `#` ist Kommentar.

```text
# D-Beat: Kick auf 1, 2+, 3, 4+ / Snare auf 2 und 4
name:  D-Beat klassisch
tags:  d-beat, punk, crust
tempo: 180 (150-220)
grid:  1/8
cr | X x x x X x x x |
sn | . . X . . . X . |
bd | x . . x x . . x |
---
name:  Prog 7/8, zwei Takte
tags:  prog, odd
tempo: 150
meter: 7/8 (2+2+3)
grid:  1/16
hh | X . x . X . x . X . x . x . | X . x . X . x . X . x . x . |
sn | . . . . X . . . . . . . X . | . . . . X . . . X . . . . . |
bd | x . . . . . x . x . . x . . | x . . . . . x . x . . . x . |
```

### Kopfzeilen

| Feld | Pflicht | Bedeutung |
|---|---|---|
| `name:` | ja | Anzeigename. Mit „Band – Song – Teil“ gliedert das Programm eigene Patterns nach Band und Song. |
| `id:` | nein | Kennung. Ohne Angabe `lib.` plus Name in Kleinbuchstaben. Eigene und Community-Patterns: `user.…` |
| `tags:` | nein | Komma-Liste. Der erste Tag ist der Stil, unter dem die Bibliothek das Pattern einordnet. `band:Name` und `song:Name` ordnen nach Band und Song. |
| `tempo:` | nein | Empfohlenes Tempo in BPM, Viertel. Optional mit Bereich: `180 (150-220)` |
| `meter:` | nein | Taktart, Standard `4/4`. Gruppierung in Klammern: `7/8 (2+2+3)`. Eine Komma-Liste gilt je Takt, der letzte Eintrag für alle übrigen: `4/4, 4/4, 7/8` |
| `grid:` | nein | Rasterweite einer Zelle, Standard `1/16`. Ebenso als Komma-Liste je Takt. Triolen zum Beispiel `1/12`. |
| `swing:` | nein | Notenwert und Verhältnis: `1/8 0.62`. `0.5` ist gerade, `0.667` triolisch. |
| `humanize:` | nein | Menschliche Schwankung von Timing und Lautstärke, `0` bis `1`. |

### Spuren

Eine Spur ist eine Zeile `kürzel | Zellen | Zellen |`, ein Abschnitt zwischen `|` ist ein Takt. Alle Spuren eines Patterns haben gleich viele Takte, die Zellenzahl pro Takt ergibt sich aus Taktart und Raster.

| Kürzel | Instrument | Kürzel | Instrument |
|---|---|---|---|
| `bd` | Bassdrum, `br` / `bl` rechter / linker Fuß | `cr`, `c1` | Crash 1 |
| `sn` | Snare | `c2` | Crash 2 |
| `sr` | Snare Rimshot | `ch` | China |
| `ss` | Sidestick | `sp` | Splash |
| `hh` | HiHat | `t1` … `t4` | Toms, hoch bis tief |
| `hp` | HiHat-Pedal | `cb` | Kuhglocke |
| `rd` | Ride | `rb` | Ride-Glocke |

### Zellen

| Zeichen | Bedeutung |
|---|---|
| `.` oder `-` | Pause |
| `x` | Schlag, normal |
| `X` | Akzent |
| `g` | Ghost-Note, leise |
| `f` / `F` | Flam, normal / akzentuiert |
| `o` / `O` | offene HiHat, normal / akzentuiert, nur in der `hh`-Spur |
| `h` | halboffene HiHat, nur in der `hh`-Spur |

### Übersetzen

`patternc` übersetzt einen Ordner mit Tabs in JSON. **Achtung:** Es ersetzt den gesamten Inhalt des Zielordners. Deshalb immer in einen leeren Zwischenordner übersetzen und erst dann kopieren, nie direkt in den Patterns-Ordner.

```bash
./build/patternc_artefacts/Release/patternc mein-ordner /tmp/hz-out
cp /tmp/hz-out/*.json ~/.config/HZ-Drummer/patterns/
```

Fehler meldet `patternc` mit Datei und Zeile.

## Pattern-JSON

So speichert das Programm Patterns. Positionen und Längen sind exakte Brüche in Vierteln, das Raster ist nur eine Ansicht.

```json
{
  "formatVersion": 1,
  "id": "user.community-exhorder-my-time-02-hauptriff",
  "name": "Exhorder – My Time – 02 Hauptriff – Offbeat-Snare",
  "revision": 1,
  "tags": ["thrash", "groove", "band:Exhorder", "song:My Time"],
  "tempo": { "default": 204, "min": 0, "max": 0 },
  "bars": [ { "meter": "4/4", "grid": "1/32" } ],
  "swing": { "subdivision": "1/8", "ratio": 0.5 },
  "humanize": { "amount": 0.08, "seed": 430739695 },
  "layers": [
    { "id": 1, "role": "hands", "events": [
        { "id": 1, "voice": "hihat.open", "pos": "0", "vel": 0.75 },
        { "id": 2, "voice": "snare", "pos": "1/2", "vel": 1.0 } ] },
    { "id": 2, "role": "feet", "events": [
        { "id": 3, "voice": "kick", "pos": "0", "vel": 0.75, "limb": "right" } ] }
  ],
  "origin": { "parentId": "", "parentRevision": 0, "operators": [], "seed": 0 }
}
```

| Feld | Bedeutung |
|---|---|
| `bars` | Ein Eintrag je Takt mit Taktart und Rasterweite. Die Länge des Patterns ergibt sich daraus. |
| `layers` | Ebenen für Hände (`hands`) und Füße (`feet`). |
| `events[].voice` | Instrument, optional mit Spielweise nach einem Punkt: `kick`, `snare`, `snare.rim`, `snare.side`, `hihat`, `hihat.open`, `hihat.half`, `hihat.pedal`, `ride`, `ride.bell`, `crash1`, `crash2`, `china`, `splash`, `tom1` … `tom4`, `cowbell` |
| `events[].pos` | Position in Vierteln ab Pattern-Anfang als Bruch, zum Beispiel `"0"`, `"1/2"`, `"7/4"` |
| `events[].vel` | Lautstärke `0` bis `1`: Ghost etwa `0.3`, normal `0.75`, Akzent `1.0` |
| `events[].limb` | optional `left` oder `right`, zum Beispiel für Doppelbass |
| `events[].flam` | optional `true` |
| `events[].id` | eindeutig innerhalb des Patterns |
| `origin` | Herkunft einer Variante: Eltern-Kennung, angewandte Bausteine, Zufallswert |
| `revision` | wird bei jedem Speichern erhöht |

## Song-JSON

Abläufe aus dem Arranger. Jeder Block trägt eine eigene **Kopie** seines Patterns, ein gespeicherter Song klingt deshalb immer gleich, auch wenn sich die Quelle später ändert.

```json
{
  "formatVersion": 2,
  "id": "mein-song-1a2b3c4d",
  "name": "Mein Song",
  "tempo": 0.0,
  "parts": [
    { "uid": "3ac0a7846c58", "type": "pattern", "label": "Intro", "repeats": 2,
      "source": "user.mein-intro", "bars": [1, 4], "copy": { "…": "Pattern-JSON wie oben" } },
    { "uid": "8d7af42dddb9", "type": "rest", "meter": "4/4", "count": 1, "repeats": 1 },
    { "uid": "18b2bd7c0e68", "type": "click", "meter": "7/8", "count": 2, "repeats": 1 }
  ]
}
```

| Feld | Bedeutung |
|---|---|
| `tempo` | Song-Tempo in BPM, `0` heißt: Tempo des ersten Patterns |
| `parts[].type` | `pattern`, `rest` (Pause) oder `click` (nur Metronom) |
| `parts[].uid` | Kennung des Blocks im Song |
| `parts[].label` | Anzeigename im Ablauf, ohne Angabe der Pattern-Name |
| `parts[].repeats` | Wiederholungen, 1 bis 64 |
| `parts[].source` | Kennung des Quell-Patterns |
| `parts[].copy` | die eigene Kopie des Patterns |
| `parts[].bars` | optional ein Taktbereich `[von, bis]`, ab 1 gezählt, inklusive |
| `parts[].edited` | `true`, wenn die Kopie im Song verändert wurde |
| `parts[].meter`, `count` | bei Pause und Klick: Taktart und Zahl der Takte |

Ältere Songs im Format 1 verweisen nur auf Patterns, ohne Kopie: `{ "pattern": "user.mein-intro", "label": "Intro", "repeats": 2 }`. Sie werden weiter gelesen und beim nächsten Speichern auf Format 2 gebracht.

## Ordner-JSON

`ordner.json` hält die Ordner aus **System › Ordnerstruktur**, je ein Baum für Songs, für eigene Patterns und für die mitgelieferte Bibliothek. Einträge stehen über ihre Kennung darin, die Pattern- und Song-Dateien selbst bleiben unverändert.

```json
{
  "formatVersion": 1,
  "songs": {
    "folders": [ { "name": "Thrash", "folders": [], "items": [ "song.beispiel-1a2b3c4d" ] } ],
    "items": []
  },
  "patterns": { "folders": [], "items": [] },
  "library": { "folders": [], "items": [] }
}
```

- Die Reihenfolge in `folders` und `items` ist die Reihenfolge im Menü; Ordner stehen vor den Einträgen.
- Ordnernamen gelten ohne Rücksicht auf Groß- und Kleinschreibung; gleichnamige Geschwister werden beim Lesen zusammengelegt.
- Ein Eintrag, der nirgends steht, kommt an seinen Standardplatz: Songs auf die oberste Ebene, eigene Patterns nach `band:` und `song:`, mitgelieferte Patterns nach ihrem Stil. Kennungen gelöschter Einträge fallen beim nächsten Speichern weg.
- Ist die Datei nicht lesbar, meldet die App das und überschreibt sie nicht.
