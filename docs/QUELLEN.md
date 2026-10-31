# Woher die Bausteine stammen

Die mitgelieferte Bibliothek (`library/`, Kennung `lib.…`) ist von Hand geschrieben und gehört zum
Projekt. Dazu kommen importierte Bausteine im Datenordner. Jeder Baustein nennt seine Herkunft
selbst im Feld `origin.operators` (Quelle, Datei, Takte, Lizenz). Die Umwandler liegen unter
[tools/import](../tools/import/README.md) und [tools/gpdrums](../tools/gpdrums).

Mitgeliefert wird, was die Lizenz erlaubt: Groove MIDI (CC BY 4.0) und die Bardet-Fassung von Albert Phelipot
(CC BY-SA) liegen als Sätze unter [community](../community/README.md) und kommen bei einem frischen Install
beim ersten Start automatisch in den Datenordner, zusammen mit Carcass, Exhorder und drei Beispiel-Arrangements
aus Bibliothek-Bausteinen (`startpaket/`). Alles, was „nur privat“ ist, bleibt im eigenen Datenordner.

## Importiert (Stand 31.10.2026)

| Quelle | Herkunft | Lizenz | Bausteine | Umwandler |
|---|---|---|---|---|
| Groove MIDI Dataset (Google Magenta, Gillick u. a. 2019) | [magenta.tensorflow.org/datasets/groove](https://magenta.tensorflow.org/datasets/groove) | CC BY 4.0, Namensnennung „Groove MIDI Dataset, Google Magenta“ | 892 | `midi/gmd_import.py` |
| R.P. Bardet: 260 Drum Machine Patterns, Hydrogen-Fassung von Albert Phelipot | [Hydrogen Song-and-pattern-repository](https://github.com/hydrogen-music/Song-and-pattern-repository) | CC BY-SA (Hydrogen-Fassung); das Buch selbst von Hal Leonard | 421 | `hydrogen/h2import.py` |
| MT Power Drum Kit 2 (Manda Audio), Groove-Bibliothek als Hydrogen-Songs | [github.com/torgeir/drumgizmo-aasimonster-reaper](https://github.com/torgeir/drumgizmo-aasimonster-reaper) | keine Lizenzangabe, nur privat | 382 | `hydrogen/mt_import_all.py` |
| Band-Songs aus dem Hydrogen-Repository: AC/DC, Nirvana, ZZ Top, Led Zeppelin | [Hydrogen Song-and-pattern-repository](https://github.com/hydrogen-music/Song-and-pattern-repository) | CC0 (in den Song-Dateien) | 56 | `hydrogen/h2import.py` |
| Guitar-Pro-Tabs: Gorguts, Necrophagist | [YetAnotherMorty/Guitar-Pro-Tabs](https://github.com/YetAnotherMorty/Guitar-Pro-Tabs) (Community-Abschriften) | keine Lizenzangabe, nur privat | 35 | `gpdrums/import_band.py` |
| Guitar-Pro-Tabs: Atheist (Mineral, Piece Of Time, Unholy War), nach Spielweise zerlegt | [YetAnotherMorty/Guitar-Pro-Tabs](https://github.com/YetAnotherMorty/Guitar-Pro-Tabs) | keine Lizenzangabe, nur privat | 89 | `gpdrums/import_band.py --spielweise` |
| Guitar-Pro-Tabs per System › Quellen: Metallica bis „…And Justice for All“ (35 Songs), Death (41), Black Sabbath (37), nach Spielweise zerlegt | [YetAnotherMorty/Guitar-Pro-Tabs](https://github.com/YetAnotherMorty/Guitar-Pro-Tabs), Auswahl in `tools/import/quellen-guitarpro.tsv` | keine Lizenzangabe, nur privat | 1473 / 1101 / 781 (Probelauf) | `gpdrums/import_band.py --spielweise` |
| Jimi Hendrix: zehn Songs als MIDI-Fan-Sequenzen, nach Spielweise zerlegt | [Lakh MIDI Dataset](https://colinraffel.com/projects/lmd/), clean_midi | Datensatz CC BY 4.0, die Sequenzen selbst von Fans: nur privat | 182 | `gpdrums/import_midi_band.py --spielweise` |
| Carcass | Guitar-Pro-Dateien und eine Songsterr-Abschrift (Nutzer-Transkriptionen), im Datenordner unter `songs-src/` | keine Lizenzangabe, nur privat | 35 | `gpdrums/gp2json.py`, `patternc` |
| Exhorder | eigene Abschrift nach Bildschirmfoto | eigen | 13 | von Hand |

## Gesichtet, (noch) nicht importiert

Weitere Sammlungen wurden heruntergeladen und geprüft, aber nicht eingespielt, darunter: Lakh MIDI
(CC BY 4.0 für den Datensatz, die Dateien selbst von Dritten), E-GMD (CC BY 4.0), PDMX (MuseScore,
Public Domain/CC0), Pocket Operations (Paul Wenzel, „educational use“), die 200 Bardet-Patterns
als JSON ([gvellut/dmp_midi](https://github.com/gvellut/dmp_midi)), Groove Monkee, OddGrooves,
Impro-Visor-Stile, MMA-Grooves (GPL-2), tidal-drum-patterns (GPL-3.0), DrumBeatRepo (GPL-3.0),
dnb-pattern-library (MIT), composable-drum-patterns (CC0), Handbücher von Roland- und
Yamaha-Drumcomputern sowie Mastodon „Oblivion“ (Internet Archive, CC BY-NC-SA 4.0) und Obscura
„Ten Sepiroth“ (offizielle Abschrift).

Bausteine aus Quellen ohne freie Lizenz („nur privat“) bleiben im eigenen Datenordner und werden
nicht mit dem Programm verteilt.
