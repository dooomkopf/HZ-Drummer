# Arranger („Werkstatt“) – Entwurf

Stand: 2026-10-29 · Status: **erste Ausbaustufe umgesetzt** (Reiter „Arranger“ in der Werkstatt, Taste E bei einem Song). Offen: Loop einer Auswahl, Mehrfachauswahl im Ablauf, aufklappbare Takt-Kacheln, Teilen.

## Ziel

Aus vorhandenen Bausteinen (Patterns, Song-Abschnitten, einzelnen Takten) **kinderleicht** einen Song bauen oder einen vorhandenen umbauen.

Leitgedanke: **Finden → Sammeln → Anordnen.** Was man anklickt, ist nur **gesammelt**, nicht verwendet.

## Aufbau: drei Bereiche in einem Fenster

```
┌─ Werkstatt ─ [ Raster | Arranger ] ───────────────────────────────────── Song: Meine Version ▾ ┐
│ ① FINDEN                    │ ② ABLAGE  (gesammelt, noch nicht verwendet)                       │
│ 🔍 carcass refr_            │ ┌──────────┐┌──────────┐┌──────────┐┌──────────┐                   │
│ [Band ▾][Stil ▾][Taktart ▾] │ │Refrain   ││Groove A  ││Thrash    ││+ leerer  │                   │
│                             │ │12/8 · 8 T││4/4 · 4 T ││Gallop    ││  Takt …  │                   │
│ ▾ Carcass                   │ │▪▫▪▪▫▪▪▫  ││▪▫▫▪▪▫▪▫  ││4/4 · 1 T ││          │                   │
│   ▾ Corporal Jigsore Q.     │ │ ▸ Takte  ││ ▸ Takte  ││ ▸ Takte  ││          │                   │
│      ▦ Intro      24 T      │ └──────────┘└──────────┘└──────────┘└──────────┘                   │
│      ▦ Refrain     8 T  ◀── Klick: anhören + in die Ablage                                         │
│ ▸ Thrash (Bibliothek)       ├────────────────────────────────────────────────────────────────────┤
│ ▸ Doom (Bibliothek)         │ ③ ABLAUF  (der Song)                                                 │
│                             │ ┌──────┐┌─────────────┐┌──────┐┌──────────────┐┌────────┐┊          │
│                             │ │Intro ││Strophe   ×2 ││Fill  ││Vor-Ref.   ×2 ││Refrain │┊ hierher  │
│                             │ │ 4 T  ││ 4 T  [–][+] ││1 T   ││ 4 T   [–][+] ││ 8 T    │┊ ziehen   │
│                             │ └──────┘└─────────────┘└──────┘└──────────────┘└────────┘           │
│                             │ 41 Takte · ca. 1:12 min     [▶ ab Auswahl] [⟳ Auswahl loopen] [Speichern]│
└─────────────────────────────┴────────────────────────────────────────────────────────────────────┘
```

### ① Finden

- Ein **Suchfeld** ist immer sichtbar, mit derselben unscharfen Suche wie die Schnellsuche („arc“ findet Carcass, „cjq“ Corporal Jigsore Quandary).
- **Filter-Chips** für Band, Stil, Taktart und Herkunft (siehe [BIBLIOTHEK.md](BIBLIOTHEK.md)).
- Darunter eine **gruppierte Liste**: Band › Song › Abschnitte, dann die Bibliothek nach Stil. Gruppen lassen sich auf- und zuklappen.
- **Einfachklick** auf einen Treffer: **anhören** (Vorschau im Loop) **und in die Ablage legen**. Doppelte Einträge landen nur einmal in der Ablage.
- Mit der Tastatur: Pfeile wählen, **Leertaste** hört an, **Enter** legt in die Ablage.

### ② Ablage (Scratchpad)

- Eine Sammelstelle für Kandidaten. **Nichts darin wird automatisch verwendet.**
- Jede Karte zeigt: Name, Taktart, Länge in Takten und eine **Mini-Vorschau** der Schläge (Kick/Snare/Becken als Punkte).
- **Klick auf eine Karte**: anhören. **Ziehen in den Ablauf**: einfügen, und zwar genau an der Stelle, wo die Einfügemarke erscheint.
- **„▸ Takte“** klappt die Karte auf und zeigt jeden Takt als **kleine eigene Kachel**. So lassen sich **einzelne Takte** einzeln anhören und in den Ablauf ziehen, z.B. nur Takt 4 (der Fill).
- **„+ leerer Takt …“**: ein Takt Pause oder nur Metronom in frei wählbarer Taktart, z.B. als Generalpause oder Einzähler.
- Die Karte enthält eine **Quelle** (Pattern-ID, optional Taktbereich). Ändert man das Pattern später im Raster, ändert sich auch der Baustein im Song (Verweis, keine Kopie).
- Die Ablage wird **mit dem Song gespeichert**, damit die Kandidaten beim nächsten Öffnen noch da sind. Sie lässt sich leeren.

### ③ Ablauf (der Song)

- **Blöcke** in einer Zeile, Breite proportional zur Länge. Farbe pro Baustein, Wiederholungen als feine Unterteilung. Das ist dieselbe Darstellung wie die Song-Leiste im Hauptfenster.
- **Ziehen** zum Umsortieren; eine Einfügemarke zeigt, wo der Block landet.
- **[–] [+]** am Block ändern die Wiederholungen.
- **Rechtsklick, „Lokale Umbenennung“**: umbenennen (aus „Groove A“ wird „Strophe“). Der Baustein selbst behält seinen Namen.
- **Rechtsklick**: Duplizieren, Löschen, Takte eingrenzen („nur Takte 3–4“), Teilen an einem Takt, Baustein im Raster öffnen.
- **Auswahl** per Klick, mit Shift mehrere. **▶ ab Auswahl** spielt ab dort. **⟳ Auswahl loopen** ist zum Üben einer Stelle.
- Unten stehen Gesamtlänge (Takte, Minuten bei aktuellem Tempo) und **Speichern** / **Speichern unter …**
- **Rückgängig/Wiederholen** für alles (Strg+Z / Strg+Y).

### Tastatur (Hände an der Gitarre: das Nötigste ohne Maus)

| Taste | im Ablauf |
|---|---|
| ← → | Block wählen |
| Shift + ← → | Block verschieben |
| + / – | Wiederholungen |
| Entf | Block entfernen (zurück in die Ablage) |
| Enter | ab hier abspielen |
| L | Auswahl loopen |
| Strg+D | duplizieren |
| Strg+F | Suchfeld |

## Daten

Ein Song-Teil bekommt einen optionalen Taktbereich:

```json
{ "pattern": "user.carcass-cjq-01-intro", "label": "Intro-Ende", "repeats": 2, "bars": [21, 24] }
```

- Ohne `bars` gilt das ganze Pattern; mit `bars` nur dieser Ausschnitt. So entstehen „einzelne Takte“, ohne Kopien anzulegen.
- **Leere Takte:** `{ "rest": "4/4", "repeats": 1 }` (Pause) bzw. `{ "click": "7/8", ... }` (nur Metronom).
- **Ablage:** `"scratch": [ { "pattern": "...", "bars": [4, 4] }, ... ]`
- Rückwärtskompatibel: Alte Songs ohne diese Felder laden unverändert.

## Wege zum Arranger

- Im Werkstatt-Fenster: Reiter **„Arranger“** (Taste **R** wechselt zwischen Raster und Arranger).
- Im Hauptfenster: Bei einem Song öffnet **„Bearbeiten“** direkt den Arranger, bei einem Pattern das Raster.
- **„Neuer Song“** im Pattern-Menü, dann leerer Arranger mit leerer Ablage.

## Offene Fragen an den User

1. Ein Klick im Finden-Bereich hört an **und** legt in die Ablage. Passt das, oder lieber: Klick = anhören, Enter/Doppelklick = in die Ablage?
2. Soll ein Baustein im Song ein **Verweis** sein (Änderungen am Pattern wirken überall) oder eine **Kopie** (der Song bleibt, wie er ist)? Vorschlag: Verweis, mit „Als Kopie lösen“ im Rechtsklick-Menü.
3. Arranger als Reiter im Werkstatt-Fenster (Vorschlag) oder als eigenes Fenster?

## Präzisierungen nach Review (2026-10-29)

- **Vorhören, Sammeln und Verwenden klar trennen:** Klick = **vorhören**; sichtbarer Knopf **„+ Ablage“** (oder Enter) = sammeln; zusätzlich direkt **„+ Ablauf“** beim Treffer. Die Ablage ist eine Hilfe, kein Pflichtumweg. Beim Durchhören landet nichts automatisch in der Ablage.
- **Sichtbare Knöpfe statt versteckter Gesten:** Einfügen nach Auswahl, nach links/rechts, entfernen, umbenennen und Taktbereich in einer kleinen **Detailzeile** zum gewählten Block. Ziehen mit der Maus geht zusätzlich, ist aber nicht der einzige Weg.
- Blöcke haben eine **Mindestbreite** mit lesbarem Namen; der Ablauf scrollt bei langen Songs automatisch.
- **Tastatur nach Standard:** Shift+Pfeil erweitert die Auswahl, **Alt+Pfeil verschiebt**; Entf entfernt (ohne Rücklage in die Ablage); Kürzel lösen beim Tippen nicht aus; sichtbarer Fokus.
- **Rückgängig** umfasst Einfügen, Entfernen, Verschieben, Wiederholungen, Bereiche und Ablage leeren; ein Zieh-Vorgang ist ein Schritt. Vorhören und Auswahl gehören nicht dazu.
- **Taktbereich:** einsbasiert und einschließlich in der Anzeige, intern Start + Anzahl; ungültige Bereiche werden beim Laden gemeldet.
- **Kopie statt Verweis als Standard** (Song klingt immer gleich): Der Song speichert eine eigene Kopie des Bausteins mit Quellen-ID. „Original“ wird später ausdrücklich angeboten.
- **Fehlende Quellen** werden nie still übersprungen: Der Block bleibt erhalten und wird markiert.
- **Bausteintyp** eindeutig: Pattern, Pause oder Klick, jeweils mit Taktart, Taktanzahl und Wiederholungen.
- Ablage wird mitgespeichert; Änderungen daran machen den Song „ungespeichert“.
- Vorhören und Song-Wiedergabe sind getrennt; **Esc** beendet die Vorschau; Start/Stop gilt global.

### Erste Ausbaustufe (Empfehlung aus dem Review)

Suche, optionale Ablage, ganze Patterns einfügen, Reihenfolge ändern, Wiederholungen, entfernen, eine zusammenhängende Stelle loopen, Rückgängig, sicheres Speichern. **Einzelne Takte** über einen einfachen Bereichs-Dialog („Takte von … bis …“). Aufklappbare Takt-Kacheln, Teilen und Mehrfachbearbeitung kommen später.

## Entscheidungen des Users (2026-10-29)

1. **Klick = anhören, erneuter Klick = Stopp, Doppelklick = in die Ablage übernehmen.** Zusätzlich die sichtbaren Knöpfe „+ Ablage“ / „+ Ablauf“.
2. Songs speichern **Kopien** ihrer Bausteine (mit Quellen-ID).
3. Arranger als **Reiter im Werkstatt-Fenster** neben dem Raster.
4. **Arrangieren per Maus** ist die Hauptbedienung (User, 2026-10-29): Bausteine aus Finden/Ablage in den Ablauf ziehen, Blöcke im Ablauf per Ziehen umsortieren; die Knöpfe der Detailzeile gibt es zusätzlich. Drag & Drop gehört deshalb schon in die erste Ausbaustufe.

## Entscheidungen des Users, Teil 2 (2026-10-29)

5. **Die Ablage ist eine dauerhafte Sammlung, unabhängig vom Song.** Sie wird bei jeder Änderung sofort gespeichert (`~/.config/HZ-Drummer/ablage.json`) und bleibt auch nach dem Schließen erhalten, damit man in Ruhe stöbern kann. Geleert wird sie nur ausdrücklich mit „Ablage leeren“ (rückgängig machbar). Übernehmen in den Ablauf lässt den Eintrag in der Ablage stehen.
6. **Intelligente Suche** (Wunsch des Users):
   - **Kein Datenbank-System nötig.** Suche im Speicher über vorbereitete Metadaten; reicht für tausende Einträge.
   - **Treffer gruppiert:** Songs · Song-Abschnitte (Band › Song) · Bibliothek nach Stil · weitere eigene Patterns. Höchstens fünf je Gruppe, dazu „Alle … anzeigen“.
   - **Taktart und Tempo im Suchtext erkannt:** `7/8` filtert die Taktart, `180 bpm`, `160-190 bpm`, `>180 bpm` filtern das Tempo. Erkannte Filter erscheinen als entfernbare Chips. Alles andere ist Freitext; alle Wörter müssen passen (z.B. `carcass strophe`).
   - **Bedienung:** Klick auf die Zeile = anhören, nochmal = Stopp. Kästchen = markieren, ohne abzuspielen. Aktionszeile „3 markiert · In Ablage · Auswahl aufheben“. Tastatur: Pfeile, Leertaste = anhören, X = markieren, Shift+Pfeil = Bereich, Enter = markierte in die Ablage. Doppelklick zusätzlich, aber nicht nötig.
   - **Taste:** Strg+S bleibt Speichern. Die Suche ist Strg+K bzw. `/`, im Arranger springt Strg+K ins Suchfeld.
   - **Ort:** der Finden-Bereich im Arranger ist der Hauptort zum Sammeln. Die Schnellsuche im Hauptfenster nutzt denselben Suchkern.
7. **Arrangements sind fix** (User, 2026-10-29): Ändert man ein Pattern in der Bibliothek, bleibt jeder Song, der es verwendet, unverändert. **„Original“** (Knopf in der Detailzeile oder Rechtsklick) holt die neue Fassung ausdrücklich in einen Block oder in alle Blöcke dieser Quelle, **immer mit Warnung**; Strg+Z macht es rückgängig. Blöcke, deren Quelle sich geändert hat, tragen ein ↻.
8. **Lokale Änderungen:** „Raster“ (Knopf) oder „Bearbeiten“ (Rechtsklick) öffnet die **Kopie des Blocks** im Raster. Dort heißt Speichern „In Ablauf übernehmen“ und ändert nur diesen Block in diesem Song; die Bibliothek bleibt unberührt. Solche Blöcke tragen ein ✎. „Speichern unter“ im Raster macht aus der Variante bei Bedarf ein eigenes Bibliotheks-Pattern.

