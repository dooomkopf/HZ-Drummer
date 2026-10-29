# HZ-Drummer – Konzept

Stand: 2026-10-28 · Status: **Planung** (noch kein Code)

---

## 1. Ziel & Einsatz

Ein **Drum-Begleitprogramm zum Gitarre-Üben**, das realistisch klingt und sich einfacher bedienen lässt als die üblichen Drum-Pedale.

- **Für mich selbst.** Ein späterer Verkauf ist denkbar, steht aber nicht im Vordergrund. Die Architektur hält ihn trotzdem offen (siehe Kap. 9).
- **Setup:** Laptop → Audioausgang → Stereoanlage / Aktivbox / Mischpult im Proberaum.
- **Leitidee:** *Gute Patterns ergeben gute Riffs.* Erst Patterns durchstöbern und dazu jammen, später daraus Songs bauen.

### Zwei Nutzungsarten

1. **Pattern üben:** Ein Pattern läuft im Loop. Tempo, Taktart, Feel und Variation lassen sich live ändern.
2. **Song:** Frei gewählte Patterns werden zu einer festen Folge zusammengestellt (z.B. A B A C A), mit Wiederholungen pro Part.

**Ausdrücklich nicht:** automatisches Wechseln von Variationen. Variationen wechseln nur bewusst per Taste oder Klick.

---

## 2. Bedienung

- **Kein Fußschalter.** Bedienung nur über **Tastatur und Maus**.
- Weil die Hände an der Gitarre sind: Vor dem Spielen wird alles eingestellt, dann Start mit **Einzähler** (optional 1–2 Takte Vorlauf).

### Tastatur (Entwurf, später frei belegbar)

| Taste | Funktion |
|---|---|
| Leertaste | Start / Stop |
| ← / → | voriges / nächstes Pattern |
| ↑ / ↓ | Tempo ±1 (Shift: ±5) |
| T | Tap-Tempo |
| V | nächste Variation der Familie |
| W | Würfeln (Zufallsvariation) |
| F | Fill am Taktende |
| S | aktuelle Variation speichern / einfrieren |
| 1–9 | Favoriten |

### Fenster (Entscheidung des Users, 2026-10-28)

- **Hauptfenster** = Spielansicht (große Anzeige, Transport, Taktart, Klang).
- **Raster-Editor in einem eigenen Fenster** zum Erstellen und Anpassen von Patterns. Er lässt sich vom Hauptfenster aus öffnen und kann parallel offen bleiben.
- Die Transport-Tasten (Leertaste, Tempo, Tap, Hilfe) wirken in **beiden** Fenstern gleich, egal welches gerade den Fokus hat. Der Editor zeigt selbst Lauflicht und Takt.
- Fensterposition und -größe werden gespeichert.

### Taktart-Auswahl

- **Menü mit Untermenüs** (Viertel / Achtel / Sechzehntel → Zähler → alle Gruppierungen aus 2ern und 3ern), dazu „Zuletzt benutzt“ und „Eigene…“ für freie Eingabe.
- **Favoriten:** frei festlegbar; **← →** blättert durch alle Taktarten, **Shift+← →** nur durch die Favoriten.

### Ordnung und Suche (Entscheidung des Users, 2026-10-29)

- Inhalt getrennt nach **Art** (Patterns, Songs, Taktarten) und **Herkunft** (mitgeliefert, eigene, importiert); Favoriten quer durch alles.
- **Schnellsuche** (Strg+K oder /): unscharfe Suche über Name, Stil/Tags, Künstler, Taktart, Tempo („arc“ findet Carcass); ↑↓ wählen, Enter übernehmen, Esc schließen. → zuerst
- **Bibliotheks-Ansicht** zum Stöbern (Baum + sortierbare Liste + Filter), voraussichtlich als Seitenleiste im Editor-Fenster („Werkstatt“). → danach
- **Sammlungen** (eigene Listen wie Playlists). → später
- Die Menüs P/M bleiben schlank (Favoriten, zuletzt benutzt, Bibliothek öffnen).

### Hilfe

- **F1** oder **?** blendet die Tastenübersicht ein, außerdem gibt es einen „?“-Knopf. Die Übersicht wird aus der Tastenbelegung erzeugt.

### Maus

Raster-Editor, Regler, Bibliothek, Familienbaum, Song-Arranger.

### Anzeige (aus 1–2 m lesbar)

- Groß: Pattern-Name, Taktart, BPM
- Lauflicht der Zählzeiten **mit Gruppierung** (z.B. 7/8 = 2+2+3, farbig abgesetzt)
- Im Song-Modus: aktueller Part, Takt x/y im Part, nächster Part

---

## 3. Klang

- **Realistisch:** Multi-Velocity-Samples mit mehreren Round-Robin-Varianten pro Stufe, damit schnelle Kick-Folgen nicht nach Maschinengewehr klingen.
- **Kit-Artikulationen:** Kick, Snare (Center, Rimshot, Ghost, Sidestick), HiHat (geschlossen, halboffen, offen, Pedal), Ride (Bow, Bell), Crash ×2, China, Splash, Toms, Kuhglocke.
- **Choke-Gruppen:** HiHat offen → geschlossen, Becken-Stopps.
- **Humanisierung als Regler** von 0 % (maschinell exakt) bis 100 %:
  - Velocity-Streuung
  - Mikro-Timing (auch gezielt: Snare „laid back“ für Doom)
  - einstellbar pro Pattern, plus ein globaler Regler, der alles überstimmt
- **Performance:** Multi-Mikrofon-Kits werden **vorab offline** zu Stereo-Samples gemischt. Zur Laufzeit wird nur abgespielt. So reicht auch die alte Laptop-CPU.

### Sample-Quellen (Kandidaten, Lizenzen vor Nutzung prüfen)

| Quelle | Lizenz | Zweck |
|---|---|---|
| DrumGizmo: *Aasimonster*, *CrocellKit*, *DRSKit* | CC-BY (zu prüfen) | Metal / Rock, Hauptkit |
| AVL-Drumkits: *Black Pearl*, *Blonde Bop* | CC-BY-SA (zu prüfen) | Rock, Jazz / Fusion |
| Salamander Drumkit | CC (zu prüfen) | Allround |

CC-BY-SA erlaubt auch kommerzielle Nutzung, verlangt aber Namensnennung und Weitergabe bearbeiteter Samples unter gleicher Lizenz. Lizenzen werden **vor** der Sample-Aufbereitung geprüft.

---

## 4. Patterns, Taktarten & Raster

### Taktarten

- Pro Pattern: **Zähler / Nenner / Gruppierung** (z.B. 5/4, 6/8 = 3+3, 7/8 = 2+2+3 oder 3+2+2, 13/16 …).
- Patterns können **mehrere Takte mit unterschiedlichen Taktarten** enthalten (z.B. 5/4 + 6/8).
- Taktwechsel im Song immer sauber am Taktstrich.
- Beim Wechsel zwischen Nennern (4/4 → 6/8) ist festzulegen, was konstant bleibt: Achtel oder Schlag (→ **E2**).
- Polymetrik (Meshuggah/Tool-Stil) ist **nicht** Teil von Version 1, das Datenmodell schließt sie aber nicht aus.

### Raster-Editor (Step-Sequencer)

```
           | 1 . + . | 2 . + . | 3 . + . + . |
Crash      | X . . . | . . . . | . . . . . . |
Ride       | . . X . | X . X . | X . X . X . |
Snare      | . . . . | X . . . | . . . . X . |
Kick       | X . X X | . . X X | X . . X . . |
```

- Klick: Schlag setzen oder löschen. Ziehen oder Rechtsklick: Velocity (Ghost bis Akzent).
- Auflösung pro Pattern: 8tel, 16tel, 32tel, Triolen, Sextolen.
- Swing/Shuffle als Feel-Parameter.
- Die Gruppierung ist im Raster sichtbar.

### Schichten

```
Pattern = Hände (Becken / Snare / Toms)  +  Füße (Kick-Figur)
```

Die **Kick ist eine eigene, austauschbare Schicht**. Ein Thrash-Beat lässt sich so mit Gallop-, 16tel-Doppelbass- oder Triolen-Kick kombinieren. Gute Bassdrum-Figuren sind ein ausdrücklicher Schwerpunkt („nicht 0815“).

---

### Tempo – flexibel regelbar (Anforderung)

- **Jederzeit live änderbar**, auch während des Spielens: Tasten (±1 / ±5), Regler, direkte Zahleneingabe, Tap-Tempo. Die Änderung greift weich ohne Aussetzer.
- **Bereich** ca. 30–320 BPM, Anzeige mit Bezugseinheit (♩ / ♩. / ♪).
- **Im Song:** Part-Tempo **relativ zum Songtempo** (z.B. „Songtempo“, „+10 BPM“, „×0,5 Half-Time-Feel“) oder **absolut** (fixe BPM). Relativ ist Standard: Wird der ganze Song schneller, bleiben die Verhältnisse erhalten.
- **Übergänge:** pro Übergang wählbar: sofort, allmählich über N Takte (Accelerando/Ritardando), Pulsübernahme (E2).
- **Übungshilfen:** Tempo-Trainer (+N BPM alle X Durchläufe), Tempo-Faktor (z.B. 80 % zum Üben, ohne die gespeicherten Werte zu ändern).

## 5. Familien & Variationen („intelligenter Modus“)

**Version 1 (Entscheidung E3):** Eine Variation ist ein eigenständiges Pattern mit **Herkunftsverweis** (Elternpattern, angewandte Operatoren, Zufalls-Seed). Änderungen am Grundpattern wirken nicht automatisch auf bestehende Variationen, so bleiben sie stabil. Die Familie bleibt als Baum sichtbar. Echte Vererbung mit Update-Vorschau ist für später vorgesehen.

```
Thrash-Gallop 4/4  (Grundpattern)
 ├── + Ride statt HiHat
 │    └── + Ride-Bell auf 2 und 4
 ├── + Ghost-Notes Snare
 ├── + Kick: 16tel-Doppelbass
 └── + Kuhglocke auf den Offbeats
```

### Drei Wege zur Variation (alle gleichwertig)

| Weg | Beispiel |
|---|---|
| **Bewusst, grob:** Knöpfe | Ride statt HiHat, Kick verdoppeln, Ghost-Notes +, Half-Time |
| **Bewusst, fein:** Raster | einzelne Schläge setzen oder ändern |
| **Zufall:** Würfeln | Regler für die Stärke + Auswahl, *was* sich ändern darf (nur Kick, nur Becken …), mit genre-passenden Regeln |

Was gefällt, wird **eingefroren** und als Ast im Baum gespeichert.

### Änderungsbausteine (Operatoren)

- **Becken tauschen:** HiHat ↔ Ride ↔ China ↔ Kuhglocke ↔ Crash-Ride
- **Verzieren:** Ghost-Notes, HiHat-Öffnungen, Flams, Bell-Akzente
- **Dichte:** Kick verdoppeln oder ausdünnen, 8tel → 16tel
- **Feel:** Half-Time, Double-Time, Swing
- **Akzente:** verschieben, Crash auf die Eins
- **Fill-Ende:** letzter Takt mit Fill
- **Kick-Schicht tauschen**

---

## 6. Bibliothek

Patterns, Familien und Song-Vorlagen, durchsuchbar nach **Genre, Taktart, Tempo und Tags**, dazu **Favoriten** und **Notizen** (z.B. „hier kam das Riff in E-Moll“).

### Grundbibliothek (selbst gebaut, Stilmerkmale statt Song-Kopien)

| Stil | Inhalte |
|---|---|
| **D-Beat** | klassisch, Crust, mit Doppelbass |
| **Thrash** | Thrash-/Skank-Beat, Polka-Beat, Gallop, Doppelbass unter Thrash-Beat, Half-Time |
| **Death** | Blasts, Double-Time, Kick-Bursts, Kick-Triolen gegen geraden Beat |
| **Grind / Goregrind** | Hammer-, Traditional- und Bomb-Blast, Gravity-Blast, Blast/D-Beat-Wechsel |
| **Tech-Death** | Kick-Ostinati (5er/7er über 4/4), Sextolen, verschobene Snares, Off-Beat-Becken |
| **Doom** | Half-Time, Crash-Teppiche, Tom-Grooves, Triolen-Feel, Funeral ab ca. 40 BPM |
| **Prog** | 5/4, 7/8, 9/8, 11/8, 13/16, lineare Grooves, verschiedene Gruppierungen |
| **Fusion / Jazz** (Würze) | Swing-Ride, Shuffle, Fusion-Grooves |

Geplant sind ca. 10–20 Grundpatterns pro Stil, jeweils mit Variationen und Fills. Im Programm stehen **keine Bandnamen**, sondern Stilbezeichnungen.

### Song-Vorlagen

Strophe/Refrain-Standard, Doom (lange Parts), Thrash (Intro, Riffs, Solo, Breakdown), Prog (viele Taktwechsel), Jazz (Head, Soli, Head).

### MIDI-Import

- Freie Quellen, Kandidat: **Groove MIDI Dataset** (Google Magenta, CC-BY, echte Drummer).
- Importierte Grooves werden zu eigenen Familien, die menschliches Timing und Dynamik behalten.
- Drum-Mapping (GM und gängige Plugin-Mappings) muss einstellbar sein.

---

## 7. Song-Arranger (Phase 2)

- Songstrukturen werden **unter einem selbst vergebenen Namen gespeichert** (ebenso Patterns und Variationen), mit Umbenennen, Duplizieren („Speichern unter…“) und Löschen. Die Namen sind in der Bibliothek durchsuchbar.
- Ein Song ist eine **Folge von Parts**. Jeder Part enthält eine Pattern-/Variations-Referenz, Wiederholungen, optional Tempo-Override und optional ein Fill am Ende.
- Grundtempo plus Tempowechsel pro Part (später auch allmählich).
- Ansicht: Blöcke in einer Zeile, per Drag & Drop umsortierbar, mit Taktart je Block sichtbar.

---

## 8. Extras (später)

- **Intelligente Übergänge** (Wunsch des Users): Wechsel zwischen Patterns/Parts nicht abrupt, sondern wie ein echter Drummer: passendes Fill im letzten Takt (bzw. letzter Halbtakt), Crash auf der neuen Eins, ggf. Auftakt, Dynamik-Anstieg/Abfall. Auswahl abhängig von Stil, Tempo und Ziel-Pattern; abschaltbar.
- **Schlag-Visualisierung** (Wunsch des Users, per Schalter): unter dem Lauflicht zusätzlich anzeigen, *welche* Instrumente auf den Zählzeiten spielen (Kick, Snare, HiHat, Becken …) und eine feine Zwischeneinteilung mit kleinen Zeichen für Offbeats/16tel, sodass man den Groove mitlesen kann.
- **Schriftgröße / Skalierung umschaltbar** (Wunsch des Users, 2026-10-29): für verschiedene Monitore, z.B. 100 / 115 / 130 % im System-Dialog; JUCE kann das global (`Desktop::setGlobalScaleFactor`), die Werkstatt hat Platz dafür.
- **Übungs-Loop über Taktbereiche** (Wunsch des Users, 2026-10-29, hohe Priorität): Einen beliebigen Bereich ganzer Takte loopen, auch über Teilgrenzen hinweg, z.B. die letzten 3 Takte von Pattern X plus die ersten 2 Takte von Pattern Y, um gezielt den Übergang zu üben. Kleinste Einheit ist ein Takt.
  - **Setzen:** Strg + mit der Maus über die Song-Leiste streichen. Der Bereich rastet auf ganze Takte ein.
  - **Ein/Aus:** schnell umschaltbar (Taste und Knopf), die Grenzen bleiben beim Ausschalten erhalten.
  - **Sichtbar und bedienbar in der normalen Player-Ansicht** (Song-Leiste), nicht nur im Arranger.
  - Technisch: Der Player bekommt einen Loop-Bereich (Start/Ende in Vierteln, auf Taktgrenzen). Am Ende des Bereichs springt er wie beim Seek an den Anfang, ohne den Plan zu wechseln.
- **Tempokurve über Song-Strukturen** (Wunsch des Users, 2026-10-29): Zusatzbalken über der Song-Ablaufleiste, der das globale Tempo über Bereiche verändert (z.B. allmählich 120 → 150 → wieder langsamer). Nur bei Songs, nicht bei einzelnen Patterns. Technisch passend: Der Player rechnet das Tempo bereits blockweise mit Anker, eine Kurve aus Stützpunkten (linear/weich) lässt sich pro Block auswerten. Bedienung: Punkte im Balken setzen und ziehen.
- **Spulen und Springen im Song** (Wunsch des Users, 2026-10-29): Positionsmarke in der Song-Ablaufleiste mit der Maus ziehen; **N** / **B** springt zum nächsten / vorigen Teil. Bei jedem Sprung oder Spulen zählt das Metronom ein (abschaltbar).
- **Rhythmen offline erzeugen**: Generator schreibt Pattern-Dateien (Tab/JSON) in einen Ordner, das Programm lädt sie (siehe Abschnitt 6).

- Tempo-Trainer: alle X Durchläufe +N BPM
- Spuren stummschalten / solo
- Metronom-Klick zusätzlich, Einzähler mit Sticks
- Export als **MIDI** und **WAV**
- MIDI-Ausgang an externe Drum-Plugins (Superior Drummer, SSD)

---

## 9. Technik & Architektur

**Entscheidung:** **JUCE (C++)** als Standard für Musiksoftware, mit nativem UI (keine Browser-/Web-App).

### Zielsystem

Entwicklung und Nutzung auf einem älteren Notebook (Intel i5 der 5. Generation, 16 GB RAM, Ubuntu 22.04, PipeWire, Onboard-Audio). Die alte CPU dient als Härtetest.

### Architektur-Prinzipien

```
┌──────────────────────────────────────────────┐
│ UI (JUCE Components)                          │
│  Raster · Familienbaum · Bibliothek · Anzeige │
└───────────────▲──────────────────────────────┘
                │ Befehle / Status (lock-free)
┌───────────────┴──────────────────────────────┐
│ Modell: Pattern · Familie · Song · Bibliothek │  ← reines C++, testbar
├───────────────────────────────────────────────┤
│ Engine: Sequencer (sample-genau) · Sampler    │  ← ohne UI-Abhängigkeit
│         Humanisierung · Variations-Operatoren │
└───────────────────────────────────────────────┘
```

- **Engine und Modell strikt getrennt vom UI.** So können später Plugin (VST3/AU), iPad oder Android dieselbe Engine nutzen.
- **Audio-Thread realtime-sicher:** keine Allokation, keine Locks, kein Datei-I/O im Audio-Callback.
- **Sample-genaues Timing:** Der Sequencer plant Events innerhalb der Audio-Blöcke, nicht über Timer.
- **Dateiformate:** versioniertes JSON für Pattern, Familie, Song und Bibliothek. Kits bekommen ein eigenes Kit-Manifest plus Samples (FLAC/WAV).
- **Tests:** Unit-Tests für Taktart- und Timing-Logik, Variations-Operatoren und Dateiformate.
- **Mehrere Kerne nutzen, wo es sich lohnt** (Wunsch des Users). Der Audio-Callback selbst bleibt ein schlanker Echtzeit-Thread. Parallel laufen: Laden und Dekodieren der Samples (Thread-Pool, beschleunigt den Start), Vormischen der Multi-Mikrofon-Kits, Würfeln und Berechnen von Variationen, WAV-Export (schneller als Echtzeit) sowie Bibliothekssuche und Indexierung. Erst messen, dann parallelisieren.

### Benötigte Werkzeuge (Installation mit sudo durch den User)

`build-essential`, `cmake`, `git`, JUCE-Abhängigkeiten (`libasound2-dev`, `libjack-jackd2-dev`, `libfreetype-dev`, `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libgl1-mesa-dev` …)

### Lizenzen (relevant erst bei einem Verkauf)

- JUCE: Bedingungen der aktuellen Version prüfen (frei bis Umsatzgrenze bzw. AGPL).
- Samples: CC-BY mit Namensnennung; CC-BY-SA ebenfalls kommerziell nutzbar, aber mit Share-Alike-Pflicht für bearbeitete Samples.
- MIDI: nur Quellen mit Weitergabeerlaubnis ins Produkt.

---

## 10. Fahrplan

| Phase | Inhalt | Stand |
|---|---|---|
| **0** | Werkzeuge, JUCE-Grundgerüst, Audio-Ausgabe | ✅ |
| **1** | Sampler mit Multi-Velocity/Round-Robin, Kit vorgemischt (kitprep) | ✅ |
| **2** | Taktgeber mit Taktarten, Gruppierung, Tempo, Loop | ✅ |
| **3** | Taktart-Menü, Favoriten, Hilfe, Einstellungen speichern | ✅ |
| **4** | Pattern-Modell, Pattern-Player, Tab-Format, Grundbibliothek (47 Patterns) | ✅ |
| **5** | Raster-Editor (eigenes Fenster), eigene Patterns speichern | ✅ erste Fassung |
| **6** | Variationen: Knöpfe (Ride statt HiHat, Kick verdoppeln …), Würfeln, Familienbaum | offen |
| **7** | Schlag-Visualisierung im Hauptfenster, Einzähler | offen |
| **8** | MIDI-Import | offen |
| **9** | Song-Arranger mit Vorlagen, intelligente Übergänge | offen |
| **10** | Extras (Tempo-Trainer, Export, MIDI-Out) | offen |

## 11. Heikle Entscheidungen (gemeinsam klären)

| # | Frage | Optionen | Empfehlung | Status |
|---|---|---|---|---|
| E1 | Framework | JUCE / Qt / Tauri / Flutter | JUCE | ✅ entschieden |
| E2 | Tempo bei Nenner-Wechsel (4/4 → 6/8) | Achtel konstant / Schlag konstant / pro Übergang wählbar | Intern Tempo immer in Vierteln; Standard: Taktwechsel ändert das Vierteltempo nicht (= Achtel konstant). Pulsübernahme als Einstellung **am Übergang** (z.B. „alte ♩ = neue ♩.“), mit Vorschau des resultierenden Tempos | ✅ entschieden |
| E3 | Variationsmodell | (a) Kopie + Herkunftsverweis / (b) verknüpfte Variante mit versionierter Vererbung | Für V1 (a), Familie bleibt über Herkunft sichtbar; (b) später: Elternänderung = neue Revision, Varianten bieten Update mit Vorschau an, Konflikte nie still auflösen. Zufall mit gespeichertem Seed | ✅ entschieden: (a) für V1 |
| E4 | Kick als eigene Schicht | immer getrennt / optional | immer getrennt im Modell, im Raster gemeinsam sichtbar | offen |
| E5 | Hauptkit | Aasimonster / Crocell / anderes | probehören, dann entscheiden | offen |
| E6 | Dateiformat | JSON / SQLite / eigenes Binärformat | JSON (lesbar, versionierbar) | offen |
| E7 | Würfel-Regeln | rein zufällig / genre-bezogene Regeln / lernend aus MIDI-Daten | genre-bezogene Regeln, später ergänzt durch Statistik aus importierten Grooves | offen |
| E8 | Audio-Backend unter Linux | ALSA direkt / JACK / PipeWire-JACK | über JUCE: ALSA oder PipeWire-JACK, testen | offen |

---

## 12. Ergänzungen aus dem Review (2026-10-28)

- **Realtime:** Patterns außerhalb des Audio-Threads zu unveränderlichen Abspielplänen auflösen. Übergabe über begrenzte Queues, alte Pläne außerhalb des Callbacks freigeben (auch kein letzter `shared_ptr`-Release im Callback). Feste Voice-Pools, definierter Stimmenentzug, kurze Choke-Rampen, Obergrenzen für Events pro Block.
- **Audio unter Linux:** PipeWire-JACK messen, Startpunkt 48 kHz mit 256/512 Frames, Aussetzer unter Last prüfen.
- **Wechselregeln festlegen:** Ein Pattern-Wechsel greift beim nächsten Schlag, Takt oder Pattern-Ende; Prioritäten von Fill, Stop und Wechsel.
- **Human-Timing:** Negative Verschiebungen (Schlag *vor* dem Raster) brauchen Vorausplanung über Loopgrenzen und den Einzähler hinweg.
- **Robustheit:** Undo/Redo, atomisches Speichern, Format-Migrationen, Umgang mit fehlenden Samples.
- **Pegel:** Headroom, Clipping-Schutz, Velocity-Kalibrierung, RAM-Budget.
- **Daten:** stabile Event-IDs statt Array-Indizes; musikalische Positionen als rationale Werte in Vierteln, das Raster ist nur eine Ansicht.
