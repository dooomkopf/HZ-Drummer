# HZ-Drummer – Bedienung

## Starten

- Über das Anwendungsmenü (Super-Taste, „HZ“ tippen) oder das Symbol im Dock.
- Läuft das Programm schon, holt ein Klick auf das Symbol das Fenster nach vorn.
- Beenden: Fenster schließen (nur dann werden die Einstellungen gesichert). Notfalls im Terminal: `killall HZ-Drummer`.

## Hauptfenster

```
[Start] [Tap] [Mixer] [System] [Click]  ─────────●──────── 170 BPM
[Baustein ▾                     ★] [Suchen] [Bearbeiten]  [Taktart ▾] [Klang ▾]

 Thrash Gallop
 ┌ Taktart ┐ ┌ BPM ┐ ┌ Takt ┐
     4/4       170     2/4
   ●  ●  ●  ●      ← Lauflicht (rot = Eins, orange = Gruppenanfang, blau = übrige)
```

Zwei Betriebsarten:

- **Baustein:** spielt einen Baustein (aus der Bibliothek oder selbst gebaut) oder ein Arrangement.
- **Metronom:** spielt nur die Taktart, als Klick, Bassdrum, Snare oder Bass+Snare.

Im Menü einen Baustein wählen schaltet auf Baustein, eine Taktart wählen schaltet auf Metronom. Das Baustein-Menü zeigt **Arrangements (Songs)**, **Bausteine (Pattern)** und **Bibliothek-Bausteine**.

**Umbenennen:** Rechtsklick im Baustein-Menü auf einen Baustein oder ein Arrangement, oder der Knopf **Umbenennen** neben dem Titel. Ein umbenannter Baustein heißt auch in den gespeicherten Arrangements, im Arbeitsstand des Arrangers und in der Ablage neu; Blöcke, die im Arranger einen lokalen Namen bekommen haben, behalten ihn. Bibliothek-Bausteine lassen sich nicht umbenennen. Ordner benennst du unter **System › Ordnerstruktur** um.

**Bausteine und Arrangements löschen:** Rechtsklick auf den Eintrag im Baustein-Menü, der Knopf **Löschen** neben dem Titel, oder unter **System › Ordnerstruktur**. Nichts wird endgültig gelöscht: Die Datei wandert in den Papierkorb (**System › Papierkorb**, dort „Wiederherstellen“ oder „Papierkorb leeren“). Ein Baustein, den ein gespeichertes Arrangement, der Ablauf oder die Ablage noch verwendet, lässt sich nicht löschen; die Meldung sagt, wo er steckt. Bibliothek-Bausteine lassen sich nicht löschen.

**Ordner:** Unter **System › Ordnerstruktur** stehen Arrangements, Bausteine und die Bibliothek als Baum. Dort legst du Ordner an, auch beliebig verschachtelt, benennst sie um und löschst leere. Bekommt ein Ordner den Namen eines Ordners daneben, werden beide zu einem zusammengelegt, etwa „techdeath“ in „Tech-Death“; auch eine reine Änderung der Groß- und Kleinschreibung geht. Einträge und Ordner ziehst du mit der Maus an ihren neuen Platz; **Hoch** und **Runter** ändern die Reihenfolge. Solange ein Eintrag keinem Ordner zugeordnet ist, steht er wie bisher: Arrangements oben, Bausteine nach Band und Song, die Bibliothek nach Stil. Die Ordner stehen in `ordner.json`, die Baustein- und Arrangement-Dateien bleiben dabei unverändert.

**System** zeigt außerdem die Audio-Einstellungen, die freien Abzeichen und die Log-Dateien (App-Protokoll und Startprotokoll), ganz unten die Versionsnummer. Sie lautet v0.1.X; X ist die Zahl der Commits und wird beim Bauen automatisch gesetzt.

**Einzählen (Knopf „Click“):** Der Knopf schaltet durch drei Stellungen: grau *Click* = aus, blau *START* = ein Takt Klicks, wenn das Arrangement oder der Baustein von vorn startet, grün *AN* = vor jedem Start, auch nach einer Pause mitten im Arrangement. Der Klick-Takt hat die Taktart des Takts, an dem es losgeht, und das Arrangement setzt danach genau dort ein. Zeit genug, vom Laptop zur Gitarre zu greifen. Gilt nur für den Hauptplayer, nicht für Editor, Arranger oder Vorhören.

## Tasten

| Taste | Funktion |
|---|---|
| Leertaste | Start / Stop |
| ↑ / ↓ | Tempo ±1 (mit Shift ±5) |
| T | Tap-Tempo: mehrmals im Takt tippen |
| ← / → | nächster / voriger **Baustein** bzw. nächste / vorige **Taktart** (im Metronom); während der Wiedergabe wechselt es am nächsten Taktende |
| Shift + ← / → | dasselbe, nur durch die Favoriten |
| Strg + ← / → | Baustein-Stil wechseln (D-Beat, Thrash, Death, Doom …) |
| Bild ↑ / Bild ↓ | Bausteine durchblättern (auch im Metronom-Modus) |
| P | Baustein-Menü |
| M | Taktart-Menü (inkl. „Eigene Taktart eingeben …“, z.B. `17/16 4+4+4+5`) |
| Strg + F (oder *) | aktueller Baustein bzw. aktuelle Taktart als Favorit (goldener Stern) |
| K | Metronom-Klang wechseln |
| E | Werkstatt öffnen: Editor (Baustein) bzw. Arranger (Arrangement) |
| F1 oder ? | Tastenübersicht |

## Editor (Werkstatt › Reiter „Editor“, Taste E)

- **Zeilen** sind Instrumente (Becken, HiHat, Snare, Toms, Bass), **Spalten** die Rasterschritte. Ein **Klick auf den Spurnamen** spielt einen Probeklang des Instruments, ohne etwas zu setzen. Die Namen zeigen, was das Kit wirklich spielt (beim Aasimonster: „Zil-Bel“ statt Kuhglocke, „China 8"“ für den Splash). Spuren ohne Klang im Kit (Sidestick, HiHat-Pedal) oder mit demselben Klang wie eine andere (HiHat halb) erscheinen nur, wenn der Baustein sie benutzt, dann kursiv. Dicke Linien sind Taktstriche, mittlere die Zählzeiten.
- **Klick** in ein leeres Feld setzt einen Schlag, ein Klick auf einen Schlag löscht ihn.
  - **Shift + Klick** setzt einen Akzent, **Alt + Klick** eine Ghost-Note.
  - **Ziehen nach oben/unten** ändert die Lautstärke. Größe und Helligkeit des Kästchens zeigen sie an.
  - **Rechtsklick** schaltet zwischen Ghost, normal und Akzent um.
  - **Strg + Rechtsklick** schaltet den Flam ein oder aus, einen leisen Vorschlag kurz vor dem Schlag. Er steht als kleiner Strich vor dem Kästchen.
  - **Strg + Klick** auf einen Schlag lässt ihn etwas später klingen („laid back“), **Strg + Shift + Klick** früher, je 10 ms pro Klick bis ±100 ms; zurück auf das Raster mit Klicks in die Gegenrichtung. Ein genauer Wert aus einem Import (z. B. +40,8 ms) bleibt, bis man ihn bearbeitet; der erste Klick springt dann auf die nächste 10er-Stufe. Ganz am Anfang und Ende der Baustein-Schleife wird der Schlag auf die Schleifengrenze begrenzt.
  - Die **ms im Kästchen** sagen, wie viel später (−: früher) der Schlag klingt als sein Rasterplatz, Swing eingerechnet, beim Tempo des Bausteins. Das Kästchen steht dort, wo der Schlag klingt; sein Rasterplatz, auf den man klickt, bleibt als blasser Rahmen.
- **Rauten** sind Schläge außerhalb des Rasters, z.B. die drei Becken bei „3 gegen 2“. Rechtsklick löscht sie.
- **Kopfzeile je Takt:**
  - Klick auf die Taktart (z.B. `4/4 ▾`) ändert sie, auch auf eine eigene.
  - Klick auf das Raster (z.B. `1/16 ▾`) ändert die Auflösung: Viertel bis 32tel, Triolen.
  - Klick auf „Takt n“ öffnet das Takt-Menü: Takt kopieren, leeren Takt einfügen, Takt löschen, Spur leeren.
- **Oben:**
  - Name
  - **Tempo** (Schieberegler mit „… BPM“): das Tempo des Bausteins, mit dem er startet, wenn man ihn im Hauptfenster wählt. Schieben oder die Zahl anklicken und eintippen; der Regler im Hauptfenster zieht mit, „Speichern“ speichert es mit, Strg+Z nimmt es zurück. Tempo-Änderungen im Hauptfenster (auch Tap) ändern den Baustein nicht. Bei einem Block aus dem Arranger gilt das Arrangement-Tempo, das Feld ist dann gesperrt.
  - Swing
  - „Menschlich“: wie stark Timing und Lautstärke leicht schwanken
- **Alle Spuren** blendet auch selten genutzte Instrumente ein (Crash 2, China, Zil-Bel, Toms 2 und 4 …).
- **Rückgängig/Wiederholen:** Strg+Z / Strg+Y
- **Speichern:**
  - **Speichern unter …** (Strg+Shift+S) legt einen **neuen Baustein** an, Vorschlag „Name mod1“ (die nächste freie Nummer). Bibliothek-Bausteine bleiben immer unverändert.
  - Eigene Bausteine: **Speichern** (Strg+S) überschreibt sie, **Löschen** verschiebt sie in den Papierkorb (siehe Hauptfenster). Der Editor wird danach leer, die Werkstatt bleibt offen.
  - Neue Bausteine erscheinen im Baustein-Menü unter „Bausteine (Pattern)“.
- Während der Editor offen ist, **hörst du die bearbeitete Fassung sofort**. Leertaste, Tempo und Tap funktionieren auch im Editor.

## Dateien

| Was | Wo |
|---|---|
| Einstellungen (Tempo, Favoriten, Audio …) | `~/.config/HZ-Drummer/HZ-Drummer.settings` |
| Bausteine | `~/.config/HZ-Drummer/patterns/*.json` |
| Arrangements | `~/.config/HZ-Drummer/songs/*.json` |
| Papierkorb (gelöschte Bausteine und Arrangements, je ein Ordner mit `data.json` und `metadata.json`) | `~/.config/HZ-Drummer/trash/` |
| Ordnerstruktur (Ordner für Arrangements, Bausteine und Bibliothek) | `~/.config/HZ-Drummer/ordner.json` |
| Abzeichen-Register | `~/.config/HZ-Drummer/abzeichen.json` |
| Eigene Hilfezeilen (Editor, Arranger) | `~/.config/HZ-Drummer/hilfezeile-raster.txt`, `hilfezeile.txt` |
| App-Protokoll (Papierkorb, Fehler) | `~/.cache/HZ-Drummer/app.log` |
| Bibliothek-Bausteine (lesbare Quelle) | `library-src/*.tab` |
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

Spuren: `bd` Bass, `sn` Snare, `sr` Rimshot, `hh` HiHat (`o` = offen), `rd` Ride, `rb` Ride-Glocke, `cr`/`c2` Crash, `ch` China, `t1`–`t4` Toms, `cb` Kuhglocke. Zeichen: `x` normal, `X` Akzent, `g` Ghost, `f` Flam, `.` Pause. Polyrhythmus: `cr(3) | X x x |`.

Datei in `library-src/` ablegen, dann übersetzen:

```
cd ~/HZ-Drummer && ./build/patternc_artefacts/Release/patternc library-src library
```

## Arranger (Werkstatt › Reiter „Arranger“)

Arrangements aus Bausteinen bauen: **Finden → Ablage → Ablauf.** Öffnen mit **E**, wenn ein Arrangement gewählt ist, oder über den Reiter in der Werkstatt.

- **Finden (links):** tippen, Treffer kommen gruppiert (Arrangements, Bausteine, Bibliothek; Teile aus einem Song tragen „Band – Song“ hinter dem Namen). `7/8` filtert die Taktart, `180 bpm` oder `160-190 bpm` das Tempo. Klick auf eine Zeile = anhören, nochmal = Stopp. Kästchen = markieren. „+ Ablage“ / „+ Ablauf“ übernehmen die markierten Zeilen (ohne Markierung: die aktuelle), „Abwählen“ leert die Markierung. Tastatur: Pfeile, Leertaste anhören, X markieren, Enter in die Ablage, Strg+K ins Suchfeld.
- **Ablage (rechts oben):** gesammelte Bausteine als Karten, bleiben auch nach dem Schließen erhalten. Klick = anhören, „+ Ablauf“ hängt an, × entfernt, „Leeren“ leert alles (rückgängig machbar). Rechtsklick auf eine Karte: „Bearbeiten (Kopie im Editor)“, darunter „vor Baustein einfügen“ und „nach Baustein einfügen“: neben dem im Ablauf angeklickten/markierten Block, ohne Markierung gesperrt (dann nur per Ziehen mit der Maus). Beim Bearbeiten einer Karte schreibt „In Ablage übernehmen“ die Änderung in die Karte (Strg+Z im Arranger nimmt es zurück); „Speichern unter“ legt einen neuen Baustein („… mod1“) an und diesen als neue Karte direkt daneben, danach bearbeitet der Editor den neuen Baustein. Ein Block im Ablauf bleibt davon unberührt.
- **Ablauf (rechts unten):** das Arrangement. Bausteine aus Finden oder Ablage **hierher ziehen** (Einfügemarke zeigt die Stelle), Blöcke per Ziehen umsortieren. Klick wählt einen Block, [−] [+] ändern die Wiederholungen. **Rechtsklick** öffnet das Block-Menü (Lokale Umbenennung, Bearbeiten, Duplizieren, Takte, Original, Play ab hier, Entfernen); „Lokale Umbenennung“ benennt den Block nur in diesem Arrangement um, der Baustein bleibt. Tastatur im Ablauf: Strg+C kopiert den gewählten Block, Strg+V fügt die Kopie hinter dem gewählten ein, Strg+X nimmt ihn aus dem Ablauf und legt ihn in die Ablage (nur, wenn derselbe Baustein dort noch fehlt); Strg+Z macht es rückgängig. Detailzeile darunter: Wiederholungen, „Takte“ (nur einen Taktbereich spielen), Duplizieren, ◀ ▶ verschieben, Entfernen, Editor, Original, Play/Stopp (ein Knopf, zeigt > bzw. ||).
- **Tasten im Ablauf:** ← → wählen, Alt+← → verschieben, + / − Wiederholungen, Entf entfernen, Enter abspielen, Esc Stopp, Strg+Z/Y Rückgängig/Wiederholen, Strg+S speichern.
- **Speichern** legt das Arrangement unter `~/.config/HZ-Drummer/songs/` ab; es erscheint danach im Baustein-Menü und in der Suche. „Speichern unter“ fragt nach einem Namen. Das Arrangement speichert **Kopien** seiner Bausteine, Änderungen an den Bausteinen ändern es nicht.
- Der aktuelle Arbeitsstand (auch ungespeichert) und die Ablage werden automatisch gemerkt und beim nächsten Öffnen wiederhergestellt.
- **Bausteine im Arrangement ändern:** Ein Arrangement bleibt fix, auch wenn du den Baustein später änderst. „Editor“ (oder Rechtsklick auf den Block, „Bearbeiten“) öffnet die Kopie des Blocks im Editor; „In Ablauf übernehmen“ ändert nur diesen Block (Markierung ✎). „Original“ holt mit Rückfrage die aktuelle Fassung des Bausteins in den Block oder in alle Blöcke dieser Quelle (Markierung ↻ zeigt, dass die Quelle neuer ist).

## Vorhören im Baustein-Menü

Menü mit P (oder Klick) öffnen, Maus über einen Eintrag halten und **P** drücken: Der Eintrag läuft in Schleife, das Menü bleibt offen. Anderer Eintrag + P wechselt, P auf demselben Eintrag stoppt, Klick oder Enter übernimmt den Eintrag, Esc stellt den Zustand von vorher wieder her. Der Titel im Hauptfenster ist beim Vorhören grün.
