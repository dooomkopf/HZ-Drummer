# HZ-Drummer – Bedienung

## Starten

- Über das Anwendungsmenü (Super-Taste, „HZ“ tippen) oder das Symbol im Dock.
- Läuft das Programm schon, holt ein Klick auf das Symbol das Fenster nach vorn.
- Beenden: Fenster schließen (nur dann werden die Einstellungen gesichert). Notfalls im Terminal: `killall HZ-Drummer`.

## Hauptfenster

```
[Start] [Tap] [Mixer] [System] [Click]  ─────────●──────── 170 BPM
[Pattern ▾                      ★] [Suchen] [Bearbeiten]  [Taktart ▾] [Klang ▾]

 Thrash Gallop
 ┌ Taktart ┐ ┌ BPM ┐ ┌ Takt ┐
     4/4       170     2/4
   ●  ●  ●  ●      ← Lauflicht (rot = Eins, orange = Gruppenanfang, blau = übrige)
```

Zwei Betriebsarten:

- **Pattern:** spielt einen Groove aus der Bibliothek oder ein eigenes Pattern.
- **Metronom:** spielt nur die Taktart, als Klick, Bassdrum, Snare oder Kick+Snare.

Im Menü ein Pattern wählen schaltet auf Pattern, eine Taktart wählen schaltet auf Metronom.

**Umbenennen:** Rechtsklick im Pattern-Menü auf ein eigenes Pattern oder einen Song. Ein umbenanntes Pattern heißt auch in den gespeicherten Songs, im Arbeitsstand des Arrangers und in der Ablage neu; Blöcke, die im Arranger einen eigenen Namen bekommen haben, behalten ihn. Mitgelieferte Patterns lassen sich nicht umbenennen. Ordner benennst du unter **System › Ordnerstruktur** um.

**Eigene Patterns und Songs löschen:** Rechtsklick auf den Eintrag im Pattern-Menü, oder der Knopf **Löschen** neben dem Titel, wenn ein eigener Eintrag geladen ist. Nichts wird endgültig gelöscht: Die Datei wandert in den Papierkorb (**System › Papierkorb**, dort „Wiederherstellen“). Ein Pattern, das ein gespeicherter Song, der Ablauf oder die Ablage noch verwendet, lässt sich nicht löschen; die Meldung sagt, wo es steckt.

**Ordner:** Unter **System › Ordnerstruktur** stehen Songs, eigene Patterns und die mitgelieferte Bibliothek als Baum. Dort legst du Ordner an, auch beliebig verschachtelt, benennst sie um und löschst leere. Bekommt ein Ordner den Namen eines Ordners daneben, werden beide zu einem zusammengelegt, etwa „techdeath“ in „Tech-Death“; auch eine reine Änderung der Groß- und Kleinschreibung geht. Einträge und Ordner ziehst du mit der Maus an ihren neuen Platz; **Hoch** und **Runter** ändern die Reihenfolge. Solange ein Eintrag keinem Ordner zugeordnet ist, steht er wie bisher: Songs oben, eigene Patterns nach Band und Song, die Bibliothek nach Stil. Die Ordner stehen in `ordner.json`, die Pattern- und Song-Dateien bleiben dabei unverändert.

**System** zeigt außerdem die Audio-Einstellungen, die freien Abzeichen und die Logdateien (App-Protokoll und Startprotokoll), ganz unten die Versionsnummer. Sie lautet v0.1.X; X ist die Zahl der Commits und wird beim Bauen automatisch gesetzt.

**Einzählen (Knopf „Click“):** Der Knopf schaltet durch drei Stellungen: grau *Click* = aus, blau *START* = ein Takt Klicks, wenn der Song oder das Pattern von vorn startet, grün *AN* = vor jedem Start, auch nach einer Pause mitten im Song. Der Klick-Takt hat die Taktart des Takts, an dem es losgeht, und der Song setzt danach genau dort ein. Zeit genug, vom Laptop zur Gitarre zu greifen. Gilt nur für den Hauptplayer, nicht für Raster, Arranger oder Vorhören.

## Tasten

| Taste | Funktion |
|---|---|
| Leertaste | Start / Stop |
| ↑ / ↓ | Tempo ±1 (mit Shift ±5) |
| T | Tap-Tempo: mehrmals im Takt tippen |
| ← / → | nächstes / voriges **Pattern** (im Pattern) bzw. **Taktart** (im Metronom); während der Wiedergabe wechselt es am nächsten Taktende |
| Shift + ← / → | dasselbe, nur durch die Favoriten |
| Strg + ← / → | Pattern-Stil wechseln (D-Beat, Thrash, Death, Doom …) |
| Bild ↑ / Bild ↓ | Patterns durchblättern (auch im Metronom-Modus) |
| P | Pattern-Menü |
| M | Taktart-Menü (inkl. „Eigene Taktart eingeben …“, z.B. `17/16 4+4+4+5`) |
| Strg + F (oder *) | aktuelles Pattern bzw. aktuelle Taktart als Favorit (goldener Stern) |
| K | Metronom-Klang wechseln |
| E | Raster-Editor öffnen |
| F1 oder ? | Tastenübersicht |

## Raster-Editor (eigenes Fenster, Taste E)

- **Zeilen** sind Instrumente (Becken, HiHat, Toms, Snare, Kick), **Spalten** die Rasterschritte. Dicke Linien sind Taktstriche, mittlere die Zählzeiten.
- **Klick** in ein leeres Feld setzt einen Schlag, ein Klick auf einen Schlag löscht ihn.
  - **Shift + Klick** setzt einen Akzent, **Alt + Klick** eine Ghost-Note.
  - **Ziehen nach oben/unten** ändert die Lautstärke. Größe und Helligkeit des Kästchens zeigen sie an.
  - **Rechtsklick** schaltet zwischen Ghost, normal und Akzent um.
- **Rauten** sind Schläge außerhalb des Rasters, z.B. die drei Becken bei „3 gegen 2“. Rechtsklick löscht sie.
- **Kopfzeile je Takt:**
  - Klick auf die Taktart (z.B. `4/4 ▾`) ändert sie, auch auf eine eigene.
  - Klick auf das Raster (z.B. `1/16 ▾`) ändert die Auflösung: Viertel bis 32tel, Triolen.
  - Klick auf „Takt n“ öffnet das Takt-Menü: Takt kopieren, leeren Takt einfügen, Takt löschen, Spur leeren.
- **Oben:**
  - Name
  - „Tempo übernehmen“: Das aktuelle Tempo wird das empfohlene Tempo des Patterns.
  - Swing
  - „Menschlich“: wie stark Timing und Lautstärke leicht schwanken
- **Alle Spuren** blendet auch selten genutzte Instrumente ein (Splash, Kuhglocke, Sidestick …).
- **Rückgängig/Wiederholen:** Strg+Z / Strg+Y
- **Speichern:**
  - **Speichern unter …** (Strg+Shift+S) legt ein **eigenes Pattern** an. Mitgelieferte Patterns bleiben immer unverändert.
  - Eigene Patterns: **Speichern** (Strg+S) überschreibt sie, **Löschen** verschiebt sie in den Papierkorb (siehe Hauptfenster). Das Raster wird danach leer, die Werkstatt bleibt offen.
  - Eigene Patterns erscheinen im Pattern-Menü unter „Eigene Patterns“.
- Während der Editor offen ist, **hörst du die bearbeitete Fassung sofort**. Leertaste, Tempo und Tap funktionieren auch im Editor-Fenster.

## Dateien

| Was | Wo |
|---|---|
| Einstellungen (Tempo, Favoriten, Audio …) | `~/.config/HZ-Drummer/HZ-Drummer.settings` |
| Eigene Patterns | `~/.config/HZ-Drummer/patterns/*.json` |
| Eigene Songs (Abläufe) | `~/.config/HZ-Drummer/songs/*.json` |
| Papierkorb (gelöschte Patterns und Songs, je ein Ordner mit `data.json` und `metadata.json`) | `~/.config/HZ-Drummer/trash/` |
| Ordnerstruktur (Ordner für Songs, eigene Patterns und Bibliothek) | `~/.config/HZ-Drummer/ordner.json` |
| Abzeichen-Register | `~/.config/HZ-Drummer/abzeichen.json` |
| App-Protokoll (Papierkorb, Fehler) | `~/.cache/HZ-Drummer/app.log` |
| Mitgelieferte Patterns (lesbare Quelle) | `library-src/*.tab` |
| Übersetzte Bibliothek | `library/*.json` |
| Drum-Kit | `kits/aasimonster/` |
| Startprotokoll (bei Problemen) | `~/.cache/HZ-Drummer/run.log` |

## Eigene Tabs schreiben (für Fortgeschrittene)

```
name:  Mein Groove
tags:  thrash
tempo: 180 (150-210)
meter: 4/4
grid:  1/16
cr    | X . . . x . . . x . . . x . . . |
sn    | . . . . X . . . . . . . X . . . |
bd    | x . x x x . x x x . x x x . x x |
```

Spuren: `bd` Kick, `sn` Snare, `sr` Rimshot, `hh` HiHat (`o` = offen), `rd` Ride, `rb` Ride-Glocke, `cr`/`c2` Crash, `ch` China, `t1`–`t4` Toms, `cb` Kuhglocke. Zeichen: `x` normal, `X` Akzent, `g` Ghost, `f` Flam, `.` Pause. Polyrhythmus: `cr(3) | X x x |`.

Datei in `library-src/` ablegen, dann übersetzen:

```
cd ~/HZ-Drummer && ./build/patternc_artefacts/Release/patternc library-src library
```

## Arranger (Werkstatt › Reiter „Arranger“)

Songs aus Bausteinen bauen: **Finden → Ablage → Ablauf.** Öffnen mit **E**, wenn ein Song gewählt ist, oder über den Reiter in der Werkstatt.

- **Finden (links):** tippen, Treffer kommen gruppiert (Songs, Song-Abschnitte, Bibliothek, eigene Patterns). `7/8` filtert die Taktart, `180 bpm` oder `160-190 bpm` das Tempo. Klick auf eine Zeile = anhören, nochmal = Stopp. Kästchen = markieren. „+ Ablage“ / „+ in Ablauf“ übernehmen die markierten Zeilen (ohne Markierung: die aktuelle), „Abwählen“ leert die Markierung. Tastatur: Pfeile, Leertaste anhören, X markieren, Enter in die Ablage, Strg+K ins Suchfeld.
- **Ablage (rechts oben):** gesammelte Bausteine, bleiben auch nach dem Schließen erhalten. Klick = anhören, „+ Ablauf“ hängt an, × entfernt, „Leeren“ leert alles (rückgängig machbar).
- **Ablauf (rechts unten):** der Song. Bausteine aus Finden oder Ablage **hierher ziehen** (Einfügemarke zeigt die Stelle), Blöcke per Ziehen umsortieren. Klick wählt einen Block, [−] [+] ändern die Wiederholungen. **Rechtsklick** öffnet das Block-Menü (Umbenennen, Bearbeiten, Duplizieren, Takte, Aus Quelle, Play ab hier, Entfernen), **Doppelklick** benennt den Block um (nur für diesen Song, das Pattern bleibt). Detailzeile darunter: Wiederholungen, „Takte“ (nur einen Taktbereich spielen), Duplizieren, ◀ ▶ verschieben, Entfernen, Bearbeiten, Aus Quelle, Play, Stopp.
- **Tasten im Ablauf:** ← → wählen, Alt+← → verschieben, + / − Wiederholungen, Entf entfernen, Enter abspielen, Esc Stopp, Strg+Z/Y Rückgängig/Wiederholen, Strg+S speichern.
- **Speichern** legt den Song unter `~/.config/HZ-Drummer/songs/` ab; er erscheint danach im Pattern-Menü und in der Suche. „Speichern unter“ fragt nach einem Namen. Der Song speichert **Kopien** seiner Bausteine, Änderungen an den Quell-Patterns ändern ihn nicht.
- Der aktuelle Arbeitsstand (auch ungespeichert) und die Ablage werden automatisch gemerkt und beim nächsten Öffnen wiederhergestellt.
- **Patterns im Song ändern:** Ein Song bleibt fix, auch wenn du das Pattern in der Bibliothek später änderst. „Bearbeiten“ (oder Rechtsklick auf den Block, oder Wechsel auf den Raster-Reiter) öffnet die Kopie des Blocks im Raster; „In Ablauf übernehmen“ ändert nur diesen Block (Markierung ✎). „Aus Quelle“ holt mit Rückfrage die aktuelle Bibliotheks-Fassung in den Block oder in alle Blöcke dieser Quelle (Markierung ↻ zeigt, dass die Quelle neuer ist).

## Vorhören im Pattern-Menü

Menü mit P (oder Klick) öffnen, Maus über einen Eintrag halten und **P** drücken: Der Eintrag läuft in Schleife, das Menü bleibt offen. Anderer Eintrag + P wechselt, P auf demselben Eintrag stoppt, Klick oder Enter übernimmt den Eintrag, Esc stellt den Zustand von vorher wieder her. Der Titel im Hauptfenster ist beim Vorhören grün.
