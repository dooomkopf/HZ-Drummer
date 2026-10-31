# Import-Vorlagen: fremde Schlagzeug-Daten → HZ-Drummer-JSON

Diese Skripte haben die Bausteine aus Guitar Pro, MIDI, Hydrogen und gedruckten Pattern-Büchern
erzeugt, die in HZ-Drummer stecken. Sie sind als **Vorlagen** gedacht: für Menschen, die eine
eigene Quelle umwandeln wollen, und für KI-Assistenten, die dafür ein neues Skript schreiben.
Die Zielformate (Pattern-JSON, Song-JSON) beschreibt [docs/FORMATE.md](../../docs/FORMATE.md),
woher die mitgelieferten Daten stammen, steht in [docs/QUELLEN.md](../../docs/QUELLEN.md).

## Welches Skript für welche Quelle

| Quelle | Skript | Ergebnis |
|---|---|---|
| Guitar Pro 3/4/5 | [`../gpdrums/GpDrums156.java`](../gpdrums/GpDrums156.java) (TuxGuitar 1.5.6) bzw. [`GpDrums.java`](../gpdrums/GpDrums.java) (TuxGuitar aus Ubuntu) | Schlagzeugspur als Textzeilen `TRACK`/`BAR`/`MARKER`/`HIT` |
| | [`../gpdrums/gp2json.py`](../gpdrums/gp2json.py) | ein Song: Wiederholungen und alternative Enden ausgerollt, Abschnitte nach Markern, gleiche Abschnitte = ein Baustein, dazu das Arrangement; mit `--spielweise` Bausteine nach Spielweise (Blast, Doppelbass, Groove, Tom-Groove, je mit Becken), 2 bis 8 Takte |
| | [`../gpdrums/import_band.py`](../gpdrums/import_band.py) | ein ganzer Band-Ordner: je Song genau eine Fassung (die mit den meisten verschiedenen Takten), mehrere Schlagzeugspuren zusammengeführt, `auswahl.tsv` sagt warum |
| | [`../gpdrums/gp_je_datei.py`](../gpdrums/gp_je_datei.py) | vorher prüfen: hat die Datei eine Schlagzeugspur mit echten Figuren? |
| Guitar Pro 7/8 (`.gp`) | [`../gpdrums/gpif_drums.py`](../gpdrums/gpif_drums.py) | dieselben Textzeilen wie `GpDrums.java`, nur Standardbibliothek (Weiterverarbeitung mit `gp2json.py` noch nicht erprobt) |
| Drum-Tabs (eigenes Format) | `patternc` (C++, [`../patternc`](../patternc)) | Pattern-JSON, siehe docs/FORMATE.md |
| MIDI (Kanal 10) | [`midi/midiscan.py`](midi/midiscan.py) | MIDI-Leser ohne Fremdpakete; Statistik, einen Takt als Positionen je Stimme, Belegungen GM und Roland TD-11 |
| | [`midi/midi_je_datei.py`](midi/midi_je_datei.py) | vorher prüfen: echte Figuren oder nur ein Grundbeat? |
| | [`../gpdrums/mid2gpdrums.py`](../gpdrums/mid2gpdrums.py) | eine MIDI-Datei in die Textzeilen von `GpDrums.java`: danach geht sie durch `gp2json.py` wie eine Guitar-Pro-Datei (Raster je Takt, Versatz in ms bleibt) |
| | [`../gpdrums/import_midi_band.py`](../gpdrums/import_midi_band.py) | eine Band aus ausgewählten MIDI-Dateien (je Song eine), wie `import_band.py` |
| | [`midi/gmd_import.py`](midi/gmd_import.py) | Groove MIDI Dataset: echte Einspielungen, Timing bleibt als Versatz in ms erhalten, 8-Takt-Ausschnitte und Arrangements |
| | [`midi/lmd_drums.py`](midi/lmd_drums.py), [`midi/pdmx_drums.py`](midi/pdmx_drums.py) | Schlagzeugspuren aus Lakh MIDI bzw. PDMX ziehen, ohne die Archive zu entpacken |
| | [`midi/dupes.py`](midi/dupes.py) | gleiche Grooves über mehrere Ordner finden |
| Impro-Visor (`.sty`) | [`midi/impro_sty.py`](midi/impro_sty.py) | Schlagzeug-Muster der Stile als Positionen (Leser) |
| Hydrogen (`.h2song`) | [`hydrogen/h2import.py`](hydrogen/h2import.py) | Songs → Bausteine je Takt-Inhalt plus Arrangement; R.P. Bardet je Stil |
| | [`hydrogen/mt_import.py`](hydrogen/mt_import.py), [`hydrogen/mt_import_all.py`](hydrogen/mt_import_all.py) | MT-Power-Grooves (je Groove Original und Fassung mit Fill) |
| | [`hydrogen/h2_dump.py`](hydrogen/h2_dump.py), [`hydrogen/h2scan.py`](hydrogen/h2scan.py), [`hydrogen/mtpower_decode.py`](hydrogen/mtpower_decode.py) | Patterns als JSON ausgeben, Dateien untersuchen, einen Takt dekodieren |
| Gedruckt / PDF | [`papier/po_text_extract.py`](papier/po_text_extract.py) | Pocket Operations aus der Textebene des PDFs (`pdftotext -bbox`) |
| | [`papier/po_to_tab.py`](papier/po_to_tab.py) | → HZ-Drummer-Tabs, danach `patternc` |

## Ablauf am Beispiel einer Band in Guitar Pro

```bash
# 1. umwandeln, nur in einen Vorbereitungsordner (Bausteine nach Spielweise)
python3 -I tools/gpdrums/import_band.py ~/tabs/Atheist Atheist /var/tmp/hz-import/atheist --spielweise
# 2. ansehen: auswahl.tsv, patterns/, songs/
# 3. HZ-Drummer beenden, dann einspielen (Sicherung des Datenordners kommt vorher)
python3 -I tools/gpdrums/install_band.py /var/tmp/hz-import/atheist Atheist
```

Für Bausteine ohne Arrangement gibt es [`install_patterns.py`](install_patterns.py). Beide
Einspiel-Skripte brechen ab, solange HZ-Drummer läuft: Das Programm würde seinen Stand sonst
darüber schreiben. Der Datenordner ist `~/.config/HZ-Drummer`, bei `install_band.py` auch über
`HZ_DRUMMER_DATA` wählbar. TuxGuitar 1.5.6 wird unter `/opt/tuxguitar` erwartet, sonst
`TUXGUITAR=<ordner>` setzen (siehe docs/SETUP.md).

## Regeln, nach denen die Vorlagen gebaut sind

Wer ein neues Skript schreibt, Mensch oder KI, hält sich am besten an dieselben Regeln:

- **Nur Python-Standardbibliothek**, immer mit `python3 -I` aufrufen. Heruntergeladene Daten
  sind fremd: jede Quelle in einen eigenen Ordner, Skripte liegen woanders, nichts aus den
  Downloads wird ausgeführt.
- **Quellen nur lesen, nur in einen Zielordner schreiben.** Eingespielt wird erst nach dem
  Ansehen, bei geschlossenem Programm und nach einer Sicherung.
- **Raster und Versatz:** Jeder Schlag kommt auf den nächsten Rasterplatz (16tel, Triolen oder
  feiner, je Takt das passende), die Abweichung bleibt als Versatz in ms (höchstens ±100).
  Zwei Schläge derselben Trommel kurz hintereinander (unter etwa 15–25 ms) werden ein Flam.
- **Stimmen** heißen wie in docs/FORMATE.md (`kick`, `snare`, `hihat`, `hihat.open`, `ride`,
  `crash1`, `tom1` …); die General-MIDI-Belegung steht in `midiscan.py` und `gp2json.py`.
- **Kennungen und Tags:** `user.<quelle>-<titel>-NN-<abschnitt>`; die Tags `band:<Name>` und
  `song:<Titel>` legen den Ordner im Baustein-Menü fest. Im Feld `origin.operators` steht,
  woher ein Baustein stammt (Datei, Takte, Lizenz), damit es später nachvollziehbar bleibt.
- **Lieber zerlegen als ganze Songs:** Bausteine sind das Wichtige. Gleiches wird nur bei exakt
  gleichem Inhalt zusammengelegt, von mehreren Fassungen eines Songs wird genau eine genommen.

## Lizenz

Die Skripte gehören zu HZ-Drummer und stehen wie das ganze Projekt unter der AGPL-3.0. Die
Daten, die sie lesen, haben ihre eigenen Lizenzen, siehe docs/QUELLEN.md. `GpDrums*.java`
nutzen zur Laufzeit die TuxGuitar-Bibliothek (LGPL), die nicht mitgeliefert wird.
