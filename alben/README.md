# Alben: welches Album ein Song hat

`alben.json` ordnet die Songs der Bands aus System › Quellen ihren Alben zu, mit Jahr und Titelnummer.
HZ-Drummer liest sie für die Ansicht Band › Album › Song (Zeile „Ansicht“ oben im Baustein-Menü). Eigene Zuordnungen
gehören nicht hierher, sondern in `~/.config/HZ-Drummer/alben.json` (gleicher Aufbau, siehe
[docs/FORMATE.md](../docs/FORMATE.md)); je Song gewinnt die eigene Datei.

- Quelle: [MusicBrainz](https://musicbrainz.org) (Daten CC0): Studioalben nach Erstveröffentlichung mit der
  Titelliste der Erstausgabe (die häufigste Fassung des ersten Jahres; Bonus-Titel späterer Ausgaben fehlen); was
  auf keinem Studioalbum steht, ist einzeln mit Nummer eingetragen (`nummern`), Live-Alben mit `"live": true`.
  Erzeugt am 01.11.2026 mit `tools/import/alben_musicbrainz.py`, Hand-Korrekturen sind in `KORREKTUREN.md`
  vermerkt.
- Nur Fakten (Titel, Jahr, Nummer), keine Noten oder Texte.
- Bands ohne Alben (Sammlungen wie Groove MIDI, R.P. Bardet, MT Power) stehen nicht darin; ihre Einträge bleiben
  in der Ansicht flach unter der Band. Eine Band ohne Eintrag ist also nie ein Fehler, nur unsortiert.
- Kommt eine Quelle unter System › Quellen dazu, gehört ihre Band hier hinein: `python3 -I tools/import/alben_musicbrainz.py
  "<Band>" --daten <Datenordner mit der eingespielten Band>` liefert den Block, Lücken werden von Hand ergänzt und in
  `KORREKTUREN.md` vermerkt.
