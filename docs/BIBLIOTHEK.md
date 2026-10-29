# Bibliothek: flexible Ordnung mit Labels – Entwurf

Stand: 2026-10-29 · Status: **Entwurf zur Entscheidung**

## Problem

Feste Ordner („Eigene Patterns ▸ Carcass CJQ ▸ …“) werden bei vielen Inhalten unübersichtlich und erlauben nur **eine** Sicht. Der User will dieselben Inhalte auf verschiedene Arten finden:

- „Was gibt es von **Carcass**?“ → Songs und Patterns, nach Song gegliedert
- „Zeig mir **alle Patterns**“ (keine Songs)
- „Alle **Refrains**“, „alles in **7/8**“, „**Hardcore** unter 160 BPM“

## Idee: Eigenschaften statt Ordner

Jeder Eintrag (Pattern, Song, Taktart) trägt **Eigenschaften**. Ordnung entsteht erst beim Anzeigen, durch **Filtern** und **Gruppieren**.

| Eigenschaft | Beispiele | Woher |
|---|---|---|
| **Art** | Pattern, Song, Taktart | automatisch |
| **Herkunft** | mitgeliefert, eigenes, importiert | automatisch |
| **Band** | Carcass, Exhorder | Import (Guitar Pro enthält Künstler/Album) oder von Hand |
| **Album** | Necroticism | Import / von Hand |
| **Song** | Corporal Jigsore Quandary | Import / von Hand; bei Abschnitten: zu welchem Song sie gehören |
| **Abschnitt** | Intro, Strophe, Refrain, Solo, Bridge | Import (Marker) / von Hand |
| **Stil** | Thrash, Doom, Hardcore, Prog … | Bibliothek / von Hand |
| **Taktart** | 4/4, 7/8 (2+2+3), 12/8 | automatisch aus dem Inhalt |
| **Tempo** | 140 BPM | automatisch |
| **Labels** | frei: „Aufwärmen“, „schwer“, „Bandprobe“ | von Hand |
| **Favorit** | ★ | von Hand |

Die bisherigen Ordner werden zu **gespeicherten Sichten**.

## Bedienung

### 1. Suche mit Filtern (Schnellsuche und Bibliotheks-Ansicht)

```
🔍 carcass refrain                                    ← freier Text, wie bisher
[Art: Pattern ×] [Band: Carcass ×] [+ Filter]        ← Filter-Chips, per Klick hinzufügen/entfernen
```

- Freitext durchsucht alle Eigenschaften.
- **Filter-Chips** schränken gezielt ein. Sie lassen sich auch tippen: `art:pattern band:carcass abschnitt:refrain 7/8`
- Ein Klick auf eine Eigenschaft in einem Treffer (z.B. „Carcass“) setzt sie als Filter.

### 2. Gruppieren: dieselben Treffer, verschiedene Sichten

```
Gruppieren nach: [Band › Song ▾]    (Alternativen: Stil · Taktart · Herkunft · keine)

▾ Carcass
   ▾ Corporal Jigsore Quandary
       ♪ Ganzer Song          4/4+12/8  140
       ♪ Test-Struktur        4/4+12/8  142
       ▦ Intro                4/4       140
       ▦ Strophe              4/4       140
       ▦ Refrain              12/8      140
   ▸ Incarnated Solvent Abuse
▸ Exhorder
▸ (ohne Band)  → mitgelieferte Bibliothek, nach Stil
```

Die Gruppierung ist frei wählbar und mehrstufig, z.B. „Band › Song“, „Stil › Taktart“, „Herkunft › Stil“.

### 3. Gespeicherte Sichten (statt fester Ordner)

Häufige Kombinationen aus Filter und Gruppierung lassen sich speichern und erscheinen im Pattern-Menü (P), z.B.:

- „Carcass“ = Band:Carcass, gruppiert nach Song
- „Übungsset“ = Label:Aufwärmen, Favoriten zuerst
- „Ungerade Takte“ = Taktart ≠ 4/4, gruppiert nach Taktart

## Daten

Pattern- und Song-Dateien bekommen einen Block `meta`, der rückwärtskompatibel ist (fehlt er, ist alles leer):

```json
"meta": {
  "artist": "Carcass", "album": "Necroticism", "song": "Corporal Jigsore Quandary",
  "section": "Refrain", "style": "death", "labels": ["schwer", "Bandprobe"],
  "source": "import:guitar-pro"
}
```

- Taktart, Tempo, Art und Herkunft werden **nicht gespeichert**, sondern aus dem Inhalt berechnet. So kann nichts veralten.
- Der Import füllt Band, Album, Song und Abschnitt automatisch.
- Mitgelieferte Patterns bekommen Stil und Labels aus ihren Tags.
- Labels zu bearbeiten (auch für viele Einträge auf einmal) gehört in die Bibliotheks-Ansicht.

## Offene Fragen

1. Reichen Filter-Chips plus Gruppieren, oder will der User zusätzlich eine klassische Baum-Navigation links?
2. Sollen gespeicherte Sichten das Pattern-Menü (P) ersetzen oder ergänzen?
3. Freie Labels: beliebig oder aus einer vorgeschlagenen Liste?

## Präzisierungen nach Review (2026-10-29)

- **Zuständigkeiten trennen:** Inhaltsdaten (im Pattern/Song), Herkunft (persistent: `source`, schreibgeschützt bei Bibliothek, Ableitung über `origin`) und **persönliche Ordnung** (Favoriten, eigene Labels auch für mitgelieferte Inhalte) in einem lokalen Zusatzdatensatz per stabiler ID.
- **Song-Zugehörigkeit** aus den Song-Referenzen ableiten (ein Pattern kann in mehreren Songs stecken); Abschnittsnamen gehören zur Verwendung (`SongPart.label`); importierter Originaltitel separat.
- Stil und Labels als Listen; Anzeigenamen erhalten, Suchschlüssel normalisieren; nie automatisch zusammenführen, nur ausdrücklich.
- **Filtersemantik:** Chips und `key:value` = dieselbe Struktur; UND zwischen Facetten, ODER innerhalb; Anführungszeichen, Negation, Zahlenvergleiche; Taktart „enthält“ vs. „ausschließlich“.
- Höchstens zwei Gruppierungsstufen zu Beginn, Gruppe „ohne Angabe“, Trefferzahlen zählen eindeutige Inhalte.
- **Performance:** kompakter Metadaten-Index, keine Dateizugriffe pro Tastendruck, virtualisierte Liste.
- **Tastatur:** Suche direkt fokussiert, Pfeile/Enter/Esc, Vorhören auf eigener Taste, Transportkürzel lösen beim Tippen nicht aus.
- **Antworten auf die offenen Fragen:** (1) keine zusätzliche Baum-Navigation, links höchstens gespeicherte Sichten; (2) Sichten ergänzen das P-Menü; (3) freie Labels mit Vorschlägen und zentralem Umbenennen/Zusammenführen.
- **Erste Ausbaustufe:** Metadaten-Migration, Freitext, wenige Chips, Favoriten, Labels bearbeiten, Gruppierung „Band › Song“.
