# Material Symbols als Abzeichen-Formen

Google Material Symbols, Copyright Google LLC, Apache-2.0 (siehe LICENSE).
Variante: Rounded, Stärke 700, gefüllt, optische Größe 20. Die App füllt sie im selben Grau
wie die 17 eigenen geometrischen Marken.

- `auswahl.txt`: die verwendeten Symbole, in dieser Reihenfolge ab Abzeichen-Form 17.
  Die Reihenfolge ist die Bedeutung, denn `~/.config/HZ-Drummer/abzeichen.json` speichert
  Formnummern. Nach der endgültigen Auswahl deshalb nur noch anhängen.
- `tools/material_badges.py` erzeugt daraus `src/app/views/MaterialBadgeCount.h` und
  `src/app/views/MaterialBadgePaths.h`.

Die Umrisse kommen aus den variablen Schriften im Google-Repo
https://github.com/google/material-design-icons (Ordner `variablefont/`). Lokal liegen Schriften
und exportierte SVGs unter `assets-src/material-symbols/` (nicht im Git, rund 136 MB).
Neu erzeugen mit `tools/material_symbols_export.py`, Befehl in `assets-src/material-symbols/QUELLE.md`.
