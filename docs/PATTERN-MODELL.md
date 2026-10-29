# Pattern-Modell (Entwurf)

Stand: 2026-10-28 · Status: **entschieden** (nach Review, vom User freigegeben)

## 1. Pattern

```
Pattern
├── id            UUID (stabil, auch nach Umbenennen)
├── name          frei vergeben ("Thrash Skank schnell")
├── formatVersion 1
├── tags          Genre/Stil/Feel: ["thrash", "skank", "fast"]
├── tempo         empfohlener Bereich + Standard (z.B. 180–240, 210)
├── bars[]        ein oder mehrere Takte, jeder mit eigener Taktart
│     ├── meter       Zähler/Nenner/Gruppierung (7/8 = 2+2+3)
│     └── grid        Rasterauflösung des Taktes (nur Bearbeitungsansicht)
├── swing         0..1 (nur auf Achtel- bzw. 16tel-Offbeats)
├── layers
│     ├── hands[]     Events: Becken, Snare, Toms, Kuhglocke …
│     └── feet[]      Events: Kick rechts/links, HiHat-Pedal
├── humanize      0..1 (Standard für dieses Pattern; global überstimmbar)
└── origin        Herkunft (Entscheidung E3): parentId, Operatoren, Zufalls-Seed
```

**Event:** `{ id, voice, pos, velocity, flags }`
- `id`: stabile Event-ID (für Variationen/Undo/Vergleich)
- `voice`: abstrakte Kit-Stimme (siehe 2.)
- `pos`: exakte Position als **Bruch in Vierteln ab Pattern-Anfang** (z.B. 7/3 = Triole), Takt-Grenzen ergeben sich aus den Taktarten
- `velocity`: 0..1 (Ghost ≈ 0,2, normal ≈ 0,7, Akzent = 1,0)
- `flags`: z.B. Flam, Choke

Die Hände/Füße-Trennung (E4) ist im Modell fest. Dadurch lässt sich die Kick-Schicht austauschen, ohne die Hände anzufassen. Im Raster werden beide gemeinsam angezeigt.

## 2. Kit-Stimmen (unabhängig vom konkreten Kit)

`kick`, `kick_l`, `snare`, `snare_ghost`*, `snare_rim`, `snare_side`, `hh_closed`, `hh_open`, `hh_half`, `hh_pedal`, `ride`, `ride_bell`, `crash1`, `crash2`, `china`, `splash`, `tom1`–`tom4`, `cowbell`

\* `snare_ghost` ist nur eine Schreibhilfe: Snare mit niedriger Velocity.

Jedes Kit bildet diese Stimmen auf seine Instrumente ab (Abbildung in `kit.json`). Fehlt eine Stimme, greift eine Ersatzregel (z.B. `hh_half` → `hh_closed`, `cowbell` → `ride_bell`).

## 3. Abspielen

- Ein Pattern wird **außerhalb des Audio-Threads** in einen **Abspielplan** übersetzt: eine sortierte Liste `(Position in Vierteln, Stimme, Velocity, Timing-Versatz)`, die Loop-Länge und die Taktgrenzen.
- Humanisierung (Velocity-Streuung, Mikro-Timing) wird beim Erstellen des Plans mit festem Seed berechnet. Damit ist sie reproduzierbar, und negative Verschiebungen am Loop-Anfang werden korrekt geplant.
- Der Audio-Thread arbeitet den Plan nur noch ab: Vierteln → Samples über das aktuelle Tempo, sample-genau wie der heutige Sequencer.
- Ein Pattern-Wechsel wird am Ende des laufenden Patterns oder Taktes übernommen. Der Plan wird per Zeiger übergeben, alte Pläne gibt das UI frei (gleiches Verfahren wie bei Kits).

## 4. Speichern

- **Eigene Patterns:** JSON, eine Datei pro Pattern, unter selbst vergebenem Namen.
- **Mitgelieferte Bibliothek:** im lesbaren **Drum-Tab-Format** (siehe 5.), wird beim Laden in das Modell übersetzt. Mitgelieferte Patterns sind schreibgeschützt; Änderungen landen als eigene Kopie mit Herkunftsverweis.

## 5. Drum-Tab-Format (Bibliothek und Import)

So, wie Gitarristen Drum-Tabs kennen. Ein Zeichen pro Rasterschritt:

```
name:   Thrash Skank
tags:   thrash, skank
tempo:  220 (180-250)
meter:  4/4
grid:   8

cr | X . . . . . . . |
hh | . . . . . . . . |
sn | . x . x . x . x |
bd | x . x . x . x . |
```

| Zeichen | Bedeutung |
|---|---|
| `.` | Pause |
| `x` | Schlag normal (0,7) |
| `X` | Akzent (1,0) |
| `g` | Ghost (0,2) |
| `o` | HiHat offen (in der HiHat-Zeile) |
| `f` | Flam |

- Mehrere Takte: weitere `| … |`-Blöcke; Taktart pro Takt über `meter: 5/4, 6/8 (3+3)`.
- Triolen/Sextolen über `grid: 12` (Achteltriolen in 4/4) bzw. `grid: 24`.

## 6. Grundbibliothek (erste Ausbaustufe, ca. 45 Patterns)

| Stil | Inhalte |
|---|---|
| D-Beat (4) | klassisch, schnell/Crust, mit Doppelbass, Halbzeit-Variante |
| Thrash (7) | Skank, Polka, Gallop, Thrash-Beat mit 16tel-Doppelbass, Half-Time-Groove, Mid-Tempo-Stampf, Breakdown |
| Death (6) | Double-Time, Kick-Bursts, Triolen-Kick, Mid-Tempo mit Doppelbass, gestoppte Becken |
| Grind / Blast (5) | Hammer-, Traditional-, Bomb-Blast, Blast/D-Beat-Wechsel, Gravity-artiger Einzelhand-Blast |
| Tech-Death (5) | Kick-Ostinato 5er über 4/4, 7er über 4/4, Sextolen-Kick, verschobene Snare, Off-Beat-Becken |
| Doom (7) | Half-Time schwer, Crash-Teppich, Tom-Groove, Triolen-Feel 12/8, Funeral (sehr langsam), Sludge-Shuffle, Ride-Groove |
| Prog / ungerade (8) | 5/4, 7/8 (2+2+3), 7/8 (3+2+2), 9/8, 11/8, 13/16, 5/4+6/8 Wechsel, linearer Groove |
| Jazz / Fusion (3) | Swing-Ride, Shuffle, Fusion-Groove |

Jedes Pattern bekommt Tags, ein empfohlenes Tempo und später Variationen und Fills.

## 7. Entscheidungen nach Review

1. **Positionen:** exakte Brüche in Vierteln (normalisiert, Nenner > 0, Größenlimits beim Laden), `0 ≤ pos < Patternlänge`. Keine künstliche PPQ-Grenze; Quantisierung erst beim MIDI-Export.
2. **Instrument + Artikulation getrennt** (z.B. HiHat/offen, Ride/Glocke, Snare/Rim). Ghost-Note = Velocity, keine eigene Stimme. Fuß (rechts/links/auto) ist eine Spielangabe, keine Stimme; „auto“ wechselt bei schnellen Kick-Folgen die Füße.
3. **Ersatzregeln** zentral und zyklusfrei (z.B. HiHat halboffen → geschlossen, Kick links → Kick). Kein Charakterwechsel durch Ersatz (Kuhglocke wird nicht stillschweigend Ride-Glocke); fehlende Stimmen werden angezeigt.
4. **Layer mit Rolle** (Hände/Füße/Sonstiges) und eigener ID; „Kick-Figur tauschen“ betrifft nur Kick-Events, nicht das HiHat-Pedal.
5. **Event-IDs** eindeutig je Pattern-Revision, bleiben beim Bearbeiten erhalten. Pattern hat eine **Revisionsnummer**; die Herkunft (E3) speichert Eltern-ID **und** Eltern-Revision.
6. **Abspielplan** (unveränderlich) wird im UI-Thread gebaut: Swing und Flam-Ausbau werden dort angewandt. Reihenfolge: Originalposition → Swing → Humanisierung.
7. **Humanisierung** im Audio-Thread deterministisch aus Seed, Event-ID und Durchlaufnummer, also von Durchlauf zu Durchlauf verschieden, aber reproduzierbar. Zeitversatz in Millisekunden mit Obergrenze; **kein Schlag wird über die Pattern-Grenze verschoben** (wird begrenzt). Damit bleiben Wechsel und Einzähler eindeutig.
8. **Transport** als durchgehende musikalische Position (Viertel); Tempo wird pro Block integriert. Live-Tempoänderungen greifen sofort, nichts ist in Samples vorausgeplant.
9. **Planübergabe:** begrenzte Warteschlange UI → Audio; Audio gibt abgelöste Pläne über eine Rückwarteschlange zurück; erst dann löscht das UI sie. Wechsel am Pattern-Ende; bei mehreren wartenden Plänen gilt der neueste.
10. **Swing:** gespeichert als Unterteilung (Achtel/16tel) und Verhältnis (0,5 gerade … 0,75); betrifft nur exakte Offbeats dieser Unterteilung.
11. **Formate:** JSON ist das verbindliche Format (Bibliothek und eigene Patterns). Tabs sind Autoren- und Importformat; ein Werkzeug übersetzt die Bibliotheks-Tabs nach JSON.
12. **Tab-Raster** als Notenwert: `grid: 1/16`, `1/8`, `1/12` (Achteltriolen), `1/24` (16tel-Triolen). Schrittzahl je Takt wird gegen die Taktart geprüft.

Später (Editor/Arranger): stabile Takt-IDs, versionierte Operatoren, Offgrid-Regeln im Editor, Song-Instanzen mit Revision, Wiederholungen, Overrides und Fill-Ersatz.
