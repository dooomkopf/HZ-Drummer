# Changelog

## v0.2 (31.10.2026)

- Startpaket beim ersten Start: Community-Sätze (Carcass, Exhorder, Groove MIDI, R.P. Bardet) und drei
  Beispiel-Arrangements; weitere Quellen per Klick unter System › Quellen (auch Metallica bis „…And Justice
  for All“, Death, Black Sabbath), Liste scrollt, Knöpfe öffnen die Ordner der Quellen
- Loop-Bereich per Shift+Ziehen in der Takt-Leiste, Speed-Trainer mit zehn Speicherplätzen
- Verlauf mit Alt+←/→, Spulen im Arranger am Positionsstrich
- Blättern nach Ordnern mit Geister-Menü, Startbild, Tooltips abschaltbar
- Abzeichen mit Material Symbols, Menü-Ordner ein- und ausblendbar (Liste alphabetisch, zehn Konfigurationen
  zum Abrufen: Minimal, Maximal und eigene)
- Ordnerstruktur: Knopf Sortieren (alphabetisch, wahlweise mit Unterordnern)
- Ansicht Band › „jjjj - Album“ › „nn. Titel“ als zweite Ordnung im Baustein-Menü (die Zeile „Ansicht“ oben im
  Menü, Taste O im Menü oder das Plattensymbol in der Baustein-Pille schalten um),
  Alben aus `alben/alben.json` (MusicBrainz) und eigener `alben.json`; `tools/import/alben_musicbrainz.py` holt die
  Alben einer Band, Quellen-Import und `install_band.py` ergänzen neue Bands von selbst
- Band-Import nach Spielweise, MIDI, Guitar Pro 6/7/8 und MuseScore als Quelle, Import-Vorlagen unter `tools/import`
- Player: jede Runde spielt genau ihre Schläge, nichts doppelt oder verloren bei Pause, Sprung und Tempowechsel

## v0.1 (29.–30.10.2026)

- Erste öffentliche Fassung: Hauptfenster, Raster-Editor, Arranger, Suche, Mixer mit Pegeln
- Ordnerstruktur und Papierkorb für eigene Bausteine und Arrangements
- Editor: Swing, Versatz in 10-ms-Stufen, Flams, Besen, „Übertragen auf“
- Datenordner wird beim Start geholt und nach dem Ende hochgeladen, wenn er ein Git-Remote hat
