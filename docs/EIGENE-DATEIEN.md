# Eigene Dateien einspielen: Guitar Pro, MuseScore, MIDI

Eigene Abschriften und Tabs werden zu Bausteinen und Arrangements im eigenen Datenordner. Der Weg hat
vier Schritte: Dateien einer Band in einen Ordner legen, in einen Vorbereitungsordner umwandeln,
ansehen, bei geschlossenem HZ-Drummer einspielen. Fertige Quellen per Klick bietet dagegen
**System › Quellen** (siehe [Bedienung](BEDIENUNG.md)).

## Was geht

| Format | Endungen | Braucht |
|---|---|---|
| Guitar Pro 3, 4, 5 | `.gp3`, `.gp4`, `.gp5` | Python 3, Java und TuxGuitar 1.5.6 ([SETUP.md](SETUP.md)) |
| Guitar Pro 6, 7, 8 | `.gpx`, `.gp` | nur Python 3 |
| MuseScore 1 bis 4 | `.mscz`, `.mscx` | nur Python 3 |
| MIDI | `.mid` | nur Python 3; Schlagzeug auf Kanal 10 |

Gelesen wird nur das Schlagzeug: die Schlagzeugspur bzw. -notenzeile, bei mehreren werden sie
zusammengeführt (z. B. Hände und Füße getrennt notiert). Dateien ohne Schlagzeug werden mit Grund
übersprungen.

## 1. Dateien ablegen

- Ein Ordner je Band, z. B. `~/tabs/Metallica`.
- Guitar Pro und MuseScore: Dateiname `Band - Titel.endung`; der Teil nach ` - ` wird der Songtitel.
  Mehrere Fassungen eines Songs dürfen nebeneinanderliegen, auch in verschiedenen Formaten
  (`Metallica - One.gp5`, `Metallica - One (2).gpx`): Genommen wird die mit Schlagzeug und den meisten
  verschiedenen Takten, bei Gleichstand die mit Markern, dann die längere.
- MIDI: Der Titel ist der Dateiname ohne Fassungsnummer (`Fire.2.mid` → „Fire“). Welche Dateien
  genommen werden, sagst du beim Umwandeln.

## 2. Umwandeln

Im Code-Ordner von HZ-Drummer, nur in einen Vorbereitungsordner (der Datenordner bleibt unberührt):

```bash
# Guitar Pro und MuseScore, auch gemischt in einem Ordner
python3 -I tools/gpdrums/import_band.py ~/tabs/Metallica Metallica /var/tmp/hz-import/metallica --spielweise

# MIDI: die Dateien nennen, je Song eine
python3 -I tools/gpdrums/import_midi_band.py ~/midi/Hendrix "Jimi Hendrix" /var/tmp/hz-import/hendrix \
        --spielweise --dateien "Fire.2.mid;Little Wing.mid"
```

- `--spielweise`: Bausteine nach Spielweise (Blast, Doppelbass, Groove, Tom-Groove, je mit Becken)
  zu 2 bis 8 Takten. Ohne: Abschnitte nach den Markern der Datei, sonst Teile zu 8 Takten.
- `--nur "Titel1;Titel2"`, `--ohne "Titel;…"` (nur `import_band.py`): bestimmte Songs.
- Wiederholungen und alternative Enden werden ausgerollt wie beim Abspielen; gleiche Abschnitte werden
  ein Baustein, im Arrangement als Wiederholung.

Im Vorbereitungsordner liegen danach `patterns/` (Bausteine), `songs/` (je Song ein Arrangement),
`auswahl.tsv` (welche Fassung je Song genommen wurde und warum) und `decoded/` (das Zwischenformat).

## 3. Ansehen

Die Ausgabe nennt je Song Takte, Abschnitte und Bausteine. Zeilen `HINWEIS: …` sagen, was übersprungen,
angepasst oder weggelassen wurde: Dateien ohne Schlagzeug, nicht genommene Fassungen, doppelte Schläge,
Vorschlagsnoten, Klänge ohne General-MIDI-Note, Taktarten, die HZ-Drummer nicht darstellt. Abschnitte
ohne Schläge bleiben als Pause im Song. Ob die App alles lädt, prüft:

```bash
./build/app_tests_artefacts/Release/app_tests library-src /var/tmp/hz-import/metallica/patterns /var/tmp/hz-import/metallica/songs
```

## 4. Einspielen

HZ-Drummer beenden (das Skript bricht sonst ab), dann:

```bash
python3 -I tools/gpdrums/install_band.py /var/tmp/hz-import/metallica Metallica
```

- Vorher wird der Datenordner nach `/var/tmp/hz-daten-backup-<Zeit>-<band>` gesichert.
- Bausteine und Arrangements mit derselben Kennung werden ersetzt; so bringt ein zweiter Import derselben
  Band sie auf den neuen Stand. Alles andere bleibt.
- Die Arrangements liegen danach unter **Arrangements** in einem Ordner mit dem Bandnamen, die Bausteine
  finden ihren Ordner über die Tags `band:` und `song:`.

## Rechte

Songs und Fan-Abschriften gehören ihren Urhebern. Was du so einspielst, bleibt in deinem Datenordner
(`~/.config/HZ-Drummer`) und gehört nicht ins Code-Repository.

## Eine Quelle für System › Quellen

Wer eine frei herunterladbare Sammlung allen per Klick anbieten will, trägt sie in
[`tools/import/quellen.py`](../tools/import/quellen.py) ein; mitgeliefert wird nur dieser Code, nie die Inhalte.

1. Ein Eintrag in `SOURCES`: `id`, `name`, `beschreibung`, `herkunft`, `hinweis` (Lizenz), `groesse` und
   `praefixe` (Anfänge der Kennungen, an denen die Seite „schon da“ erkennt).
2. Eine Ladefunktion `load_…(staging)`: herunterladen mit `download(url, ziel)` nach
   `~/.cache/HZ-Drummer/quellen/<id>/` (wird beim nächsten Mal wiederverwendet; der Knopf **Ordner** der
   Quellen-Seite öffnet genau diesen Ordner), umwandeln mit den Skripten oben über `run([...])`, dann
   `collect([(ausgabe, band)], staging)`. Fortschritt mit `step("…")`, Abbruch mit `fail("…")`.
3. Die Funktion in `LOADERS` unter der `id` eintragen.

Für Guitar Pro aus dem Archiv [YetAnotherMorty/Guitar-Pro-Tabs](https://github.com/YetAnotherMorty/Guitar-Pro-Tabs)
reicht eine Zeile je Song in [`tools/import/quellen-guitarpro.tsv`](../tools/import/quellen-guitarpro.tsv)
(Band, Pfad im Archiv, Titel), dazu der Eintrag in `SOURCES` und in `LOADERS` mit `load_gp_list("<Band>")`.
Welche Fassung je Song die beste ist, zeigt vorher ein Probelauf von `import_band.py` (`auswahl.tsv`).
