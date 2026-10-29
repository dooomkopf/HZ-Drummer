# Variationen & Würfeln („intelligenter Modus“) – Entwurf

Stand: 2026-10-28 · Status: **Entwurf zur Entscheidung durch den User**

Grundlage: E3 (Variation = Kopie mit Herkunftsverweis), Pattern-Modell (Layer Hände/Füße, Instrument + Artikulation, exakte Positionen).

## 1. Bedienung im Raster-Editor

Unter dem Raster eine Leiste mit **Variations-Knöpfen**. Jeder Knopf verändert das aktuelle Pattern sofort hörbar. Rückgängig macht ihn wieder ungeschehen.

```
Becken:   [HiHat→Ride] [Ride→China] [HiHat→Kuhglocke] [HiHat öffnen auf Offbeats]
Dichte:   [Kick verdoppeln] [Kick ausdünnen] [HiHat 8tel→16tel] [HiHat 16tel→8tel]
Feel:     [Half-Time] [Double-Time] [Swing an/aus]
Würze:    [Ghost-Notes +] [Ghost-Notes −] [Crash auf die Eins] [Fill im letzten Takt]
Füße:     [Kick-Figur ▾]  (Gallop, 16tel-Doppelbass, Triolen, Bursts, wie Snare ...)

[🎲 Würfeln]  Stärke ──●──   Darf ändern: ☑ Kick ☑ Becken ☐ Snare ☐ Toms
```

- **Würfeln** wendet eine zufällige, zum Stil passende Kombination von Bausteinen an.
  - **Stärke**: wenige oder viele Änderungen.
  - **Darf ändern**: nur ausgewählte Bereiche werden angefasst, z.B. „nur die Kick“.
- Jedes Würfel-Ergebnis ist **reproduzierbar**, weil Seed und angewandte Bausteine gespeichert werden.
- Was gefällt, wird mit **Speichern unter** zur Variante. Die Herkunft (Elternpattern, Bausteine, Seed) wird im Pattern festgehalten.

## 2. Bausteine (Operatoren)

Jeder Baustein ist eine reine Funktion `Pattern → Pattern` in der Engine. Damit ist er testbar, auch ohne Oberfläche, und deterministisch bei gleichem Seed.

| Baustein | Wirkung | Grenzen |
|---|---|---|
| Becken tauschen (A→B) | alle Schläge der Stimme A werden zu B, Velocity bleibt | betrifft nur Hände-Layer |
| HiHat öffnen auf Offbeats | geschlossene HiHat auf „+“-Positionen → offen | nur wo HiHat existiert |
| Kick verdoppeln | jede Kick bekommt einen Nachschlag eine Rasterstufe später (falls frei) | max. bis 16tel bzw. 32tel |
| Kick ausdünnen | Kicks auf schwachen Positionen entfernen, die Eins bleibt | mind. 1 Kick pro Takt |
| HiHat 8tel→16tel / zurück | Zwischenschläge ergänzen/entfernen | Raster wird bei Bedarf feiner |
| Half-Time | Snare von 2+4 auf 3; Becken halbieren | nur bei x/4 sinnvoll; sonst ausgegraut |
| Double-Time | Snare auf jede Zählzeit-Offbeat | Gegenstück |
| Ghost-Notes + / − | leise Snare auf freie 16tel neben der Hauptsnare bzw. entfernen | nie direkt vor/auf der Hauptsnare |
| Crash auf die Eins | Crash am Taktanfang (bei mehrtaktigen Patterns: nur Takt 1) | ersetzt dort HiHat/Ride |
| Fill im letzten Takt | letzter Halbtakt aus einer Fill-Vorlage (Toms/Snare) | Vorlage passend zur Taktart |
| Kick-Figur ersetzen | Kick-Events im Füße-Layer durch eine Figur aus einer Vorlagenliste | HiHat-Pedal bleibt unangetastet |

**Reihenfolge:** Bausteine werden in der Reihenfolge angewandt, in der man sie klickt, und so in `origin.operators` gespeichert, z.B. `"cymbal:hihat>ride"`, `"kick:double"`, `"ghost:+@seed=1234"`. Jeder Eintrag bekommt einen Namen und eine Version.

## 3. Würfeln: Regeln statt reinem Zufall

Pro Stil (erster Tag) eine kleine Regeltabelle: welche Bausteine sinnvoll sind und wie wahrscheinlich.

| Stil | bevorzugt | vermieden |
|---|---|---|
| Thrash | Kick-Figur (Gallop/16tel), Crash auf Eins, Ride/China tauschen | Swing, Ghost-Notes + |
| Death / Tech-Death | Kick-Bursts, Triolen-Kick, China-Offbeats | Swing |
| Grind | Blast-Varianten, D-Beat-Wechsel | Ghost-Notes |
| Doom | Half-Time, Crash-Teppich, Tom-Fill, Ride-Glocke | Double-Time, 16tel-Kick |
| Prog | Becken tauschen, Ghost-Notes, Kick ausdünnen/verdoppeln | – |
| Jazz/Fusion | Ghost-Notes, Swing-Varianten, Ride-Glocke | Doppelbass |

**Später** könnten diese Wahrscheinlichkeiten aus importierten MIDI-Grooves gelernt werden (Statistik je Stil, siehe E7).

## 4. Familienbaum

- Eine Seitenleiste im Editor zeigt die **Familie** des aktuellen Patterns: Elternpattern, Geschwister (Varianten desselben Elternpatterns) und Kinder.
- Ein Klick lädt das Pattern in den Editor, mit Rückfrage bei ungespeicherten Änderungen.
- Grundlage ist `origin.parentId` aller eigenen Patterns. Es gibt keine automatische Vererbung (E3).

```
Thrash Gallop  (Bibliothek)
 ├─ Gallop + Ride            ← eigene
 │   └─ Gallop + Ride + Ghost
 └─ Gallop Half-Time
```

## 5. Präzisierungen nach Review (2026-10-28)

- **Operatoren arbeiten musikalisch, nicht am Bildschirm-Raster:** z.B. „Kick verdoppeln *um eine 16tel*“, mit explizitem Geltungsbereich (Pattern, Takt, Auswahl). Nachschläge bleiben im eigenen Takt. Schläge außerhalb des Rasters (Polyrhythmus) werden exakt behandelt, nie stillschweigend quantisiert.
- **Schwache/starke Positionen** ergeben sich aus Taktart **und Gruppierung** (7/8 = 2+2+3), nicht aus Rasterindizes.
- **Wiederholbarkeit:** Austausch und „Crash setzen“ sind idempotent. Verdichten hat Stufen und Dichtegrenzen, also kein exponentielles Verdoppeln. Swing wird „gesetzt“, nicht umgeschaltet.
- **Half-/Double-Time** nur für erkannte Groove-Strukturen, sonst ausgegraut mit Begründung. Die beiden sind keine Umkehrungen voneinander.
- **„Ghost −“** entfernt nur die selbst hinzugefügten Ghost-Notes, nicht alle leisen Snares. Becken-Tausch mit Artikulationsregeln: Eine offene HiHat wird nicht zu „Ride offen“.
- **Speicherung:** `origin.operators` wird strukturiert (`type`, `version`, Parameter, Geltungsbereich, Seed, Vorlagen-Version) statt einer Textliste. Maßgeblich bleibt das gespeicherte Ergebnis. Unbekannte Operator-Versionen verhindern nie das Laden.
- **Würfeln:** explizites Stilprofil statt „erster Tag“. Stärke ist ein Änderungs-Budget. Widersprüchliche Kombinationen sind ausgeschlossen. Freigaben („darf Becken ändern“) gelten auch für Fills und Crash-Ersatz.
- **Familienbaum:** zeigt die Elternrevision, toleriert gelöschte Eltern und verhindert Zyklen.

**Empfehlung aus dem Review zu den offenen Fragen:**

1. Sichtbare Gruppen für häufige Bausteine, seltene in einem Untermenü, mit Hinweis auf Wirkung und Voraussetzungen.
2. Würfeln bietet **3 Vorschläge zum Vorhören**. Übernehmen ist ein ausdrücklicher Schritt (ein Rückgängig-Schritt).
3. Familienbaum als **einklappbare Seitenleiste** im Editor.

## 6. Offene Fragen an den User

1. Sind die Knopf-Gruppen so verständlich, oder lieber ein einziges Menü „Variieren ▾“?
2. Soll Würfeln das Pattern **direkt ändern** (wie die Knöpfe) oder erst **3–4 Vorschläge** zum Anhören anbieten?
3. Familienbaum als Seitenleiste im Editor oder als eigenes Menü im Hauptfenster?
