# HZ-Drummer – Logik und Regeln

Stand: 31.10.2026, v0.2 · Status: **Ist-Stand** („as is“), vom User noch nicht im Einzelnen abgenommen

Diese Datei beschreibt, wie sich das Programm heute verhält und welche Regeln dahinterstehen. Sie ist die
Grundlage für Reviews: Was hier steht, ist Absicht und kein Fehler. Weicht der Code davon ab, ist das ein
Befund. Fällt beim Testen etwas auf, wird die Regel hier geändert oder unter **Zu klären** eingetragen.

Quellen hinter einer Regel:
**[E 29.10.]** Entscheidung des Users mit Datum · **[Code]** heutiges Verhalten im Code ·
**[Bed.]** steht so in [BEDIENUNG.md](BEDIENUNG.md). Ältere Entwürfe ([KONZEPT](KONZEPT.md),
[ARRANGER](ARRANGER.md), [PATTERN-MODELL](PATTERN-MODELL.md), [BIBLIOTHEK](BIBLIOTHEK.md)) gelten nur,
soweit sie hier nicht abweichen; überholte Stellen stehen im vorletzten Abschnitt.

---

## 1. Begriffe

| Begriff | Bedeutung |
|---|---|
| **Baustein** (Pattern) | ein oder mehrere Takte, die im Loop laufen |
| **Bibliothek-Baustein** | mitgeliefert, ID `lib.…`, schreibgeschützt |
| **eigener Baustein** | ID `user.…`, liegt in `~/.config/HZ-Drummer/patterns/` |
| **Arrangement** (Song) | Folge von Blöcken, ID `song.…`, liegt in `songs/` |
| **Block** | ein Teil im Ablauf eines Arrangements: eine **Kopie** des Bausteins mit Quellen-ID |
| **Karte** | ein Eintrag in der Ablage, ebenfalls eine Kopie mit Quellen-ID |
| **Werkstatt** | ein Fenster mit den Reitern Editor und Arranger |
| **Editor** | der Reiter der Werkstatt, in dem man einen Baustein bearbeitet |
| **Raster** | das Gitter im Editor aus Spuren und Rasterschritten: das, was man bearbeitet |
| **Hauptplayer** | die Wiedergabe im Hauptfenster (Baustein, Arrangement oder Metronom) |
| **Abzeichen** | Farbe + Form, kennzeichnet die Quelle eines Bausteins überall gleich |

---

## 2. Daten und Speichern

1. **Atomar speichern:** Bausteine, Arrangements, Ablage, Ordner und Abzeichen werden über eine versteckte
   Temporärdatei geschrieben und dann ersetzt. Ein Absturz hinterlässt keine halbe Datei, Reste werden nie
   geladen. [Code]
2. **Bibliothek bleibt unverändert.** Wer einen Bibliothek-Baustein speichert, bekommt einen neuen eigenen
   Baustein mit Herkunftsverweis (Eltern-ID und Eltern-Revision). [E 28.10., Code]
3. **IDs** bleiben beim Umbenennen gleich. Neue eigene Bausteine heißen `user.<name-als-slug>-<12 Hex>`.
   Taucht eine ID doppelt auf, gilt die erste, die zweite wird ignoriert und im Protokoll gemeldet. [Code]
4. **Revision:** Jedes Speichern eines eigenen Bausteins zählt die Revision hoch, eine Kopie beginnt bei 1.
   [Code]
5. **Nichts wird endgültig gelöscht.** Löschen verschiebt Bausteine und Arrangements in den Papierkorb, von
   dort lassen sie sich wiederherstellen. [Bed.]
6. **Löschen ist gesperrt**, solange ein gespeichertes Arrangement, der Ablauf oder die Ablage den Baustein
   verwendet oder er im Editor offen ist. Die Meldung sagt, wo er steckt. Bibliothek-Bausteine lassen sich
   nicht löschen. [Bed., Code]
7. **Umbenennen** geht bei eigenen Bausteinen und Arrangements, nicht bei der Bibliothek. Kopien in
   gespeicherten Arrangements, im Arbeitsstand des Arrangers und in der Ablage heißen mit, solange sie noch
   den alten Namen tragen. Blöcke mit lokalem Namen behalten ihn. Ein Arrangement ändert dabei nur seinen
   Namen in der eigenen Datei. [Bed., Code]
8. **Arrangements sind fix:** Sie speichern Kopien ihrer Bausteine (Format v2). Ändert man den Baustein
   später, klingt das Arrangement weiter wie vorher. Alte Arrangements mit Verweisen (v1) bekommen beim
   Öffnen Kopien. [E 29.10.]
9. **Grenzen:** höchstens 512 Takte pro Baustein, das gilt auch für ein zusammengesetztes Arrangement.
   Rückgängig reicht 200 Schritte weit, im Editor wie im Arranger. Alte Arrangements über der Grenze lassen
   sich verkürzen, aber nicht verlängern. [Code]

---

## 3. Editor (Werkstatt › Editor)

### Spuren
1. Zu sehen sind die Kernspuren und alle Spuren, die der Baustein benutzt. **Alle Spuren** zeigt auch
   seltene wie Crash 2, China, Sidestick, HiHat-Pedal und die Besen. [Bed.]
2. Der Name zeigt, was das Kit wirklich spielt („Zil-Bel“ statt Kuhglocke). Spuren ohne Klang im Kit oder
   mit demselben Klang wie eine andere erscheinen nur, wenn der Baustein sie benutzt, dann kursiv. [Bed.]
3. **Klick auf den Spurnamen** spielt einen Probeklang, ohne etwas zu setzen. Unter der Maus wird der Name
   grün, außer über den M- und S-Kästchen. [Bed., Code]
4. **Rechtsklick auf den Spurnamen**, „Übertragen auf“: alle Schläge der Spur über alle Takte auf eine
   andere. Lautstärke, Versatz und Flam gehen mit. Wo beide Spuren einen Schlag haben, bleibt der lautere
   samt seinen Eigenschaften. Ein Rückgängig-Schritt. [Bed.]

### Schläge
5. Klick setzt einen Schlag oder löscht ihn. **Shift+Klick** Akzent (1,0), **Alt+Klick** Ghost (0,2),
   normal ist 0,7. **Rechtsklick** wechselt Ghost, normal, Akzent. **Ziehen** nach oben oder unten ändert
   die Lautstärke. [Bed.]
6. **Flam:** Strg+Rechtsklick schaltet ihn ein und aus. Der Hauptschlag bleibt auf seinem Platz samt
   Versatz, der Vorschlag kommt 25 ms davor mit 45 % der Lautstärke. [E 30.10. „ach lass so“, Code]
7. **Rauten** sind Schläge außerhalb des Rasters, etwa „3 gegen 2“. Rechtsklick löscht sie. [Bed.]
8. Positionen sind exakte Brüche in Vierteln. Das Raster ist nur eine Ansicht pro Takt. [E 28.10.]

### Timing
9. **Versatz** von −100 bis +100 ms: Strg+Klick 10 ms später, Strg+Shift+Klick 10 ms früher. Ein krummer
   Wert aus einem Import (+40,8 ms) bleibt, bis man ihn bearbeitet; der erste Klick springt auf die nächste
   10er-Stufe. [Bed.]
10. **Am Schleifenrand** wird ein verschobener Schlag auf die Grenze begrenzt, im Player wie in der Anzeige.
    Kein Schlag rutscht über die Baustein-Grenze. [E 28.10., Code]
11. **Swing** besteht aus Unterteilung (Achtel oder 16tel) und Verhältnis von 0,5 (gerade) bis 0,75. Er
    wirkt nur auf exakte Offbeats dieser Unterteilung. Trifft er im ganzen Baustein keine Stelle, weil das
    Raster sie nicht hat, sagt das die Kopfzeile des ersten Takts. Das Rastermenü schlägt passende Raster
    vor. [E 28.10., Code]
12. **Anzeige wie gehört:** Das Kästchen steht dort, wo der Schlag klingt, mit Swing und Versatz. Der
    Rasterplatz, auf den man klickt, bleibt als blasser Rahmen. Die ms im Kästchen gelten beim Tempo des
    Bausteins. [Bed.]

### Kopfzeile, Takte und Ansicht
13. Je Takt: Taktart, Raster und das Takt-Menü mit Takt kopieren, leeren Takt einfügen, Takt löschen und
    Spur leeren. [Bed.]
14. **Tempo** ist das Tempo des Bausteins, mit dem er startet. Der Hauptregler zieht mit. Tempo und Tap im
    Hauptfenster ändern den Baustein nicht. Bei einem Block gilt das Arrangement-Tempo, der Regler ist
    gesperrt. Bei einer Karte bleibt er frei. [Bed., Code]
15. **Menschlich** von 0 bis 100 %: höchstens 12 ms Zeit und 0,15 Lautstärke Streuung bei 100 %, mal dem
    globalen Wert. Die Streuung ist aus Seed, Schlag und Durchlauf berechnet, also jedes Mal anders, aber
    wiederholbar. Reihenfolge im Abspielplan: Position, dann Swing, dann Menschlich. [E 28.10., Code]
16. **Breite** von 100 bis 800 % und Strg+Mausrad, etwa 15 % pro Raste. Die Stelle unter der Maus bleibt
    stehen. Gilt nur in der Ansicht und nur für den geöffneten Baustein. Bei einer anderen ID springt sie auf
    100 %, beim Speichern oder Umbenennen desselben bleibt sie. [E 31.10.]
17. Ist das Fenster zu schmal für die obere Leiste, rutschen Menschlich und Breite in eine eigene Zeile unter
    die Knopfreihe. [E 31.10.]

### Hören
18. Der Editor spielt sofort die bearbeitete Fassung. Leertaste, Tempo und Tap wirken auch hier. [Bed.]
19. **M und S** sind Hörhilfen und nie Teil des Bausteins. Solo gewinnt: Ist eine Spur auf Solo, klingen nur
    Solo-Spuren. M und S derselben Spur schließen sich aus. Beim Laden eines Bausteins ist alles wieder an.
    [Code]

### Rückgängig und Speichern
20. Ein Ziehen ist ein Rückgängig-Schritt, ein getippter Wert ebenfalls. [Code]
21. **Speichern** überschreibt eigene Bausteine. **Speichern unter** legt einen neuen eigenen Baustein an,
    Vorschlag „Name mod1“ mit der nächsten freien Nummer. Er verliert dabei die Band- und Song-Angaben in
    jeder Schreibweise und steht deshalb oben bei den Bausteinen, nicht im Ordner seiner Quelle. [Bed., Code]
22. **Block:** Speichern heißt „In Ablauf übernehmen“ und ändert nur diesen Block in diesem Arrangement,
    Markierung ✎. [E 29.10.]
23. **Karte:** Speichern heißt „In Ablage übernehmen“ und ändert nur die Karte. Speichern unter macht daraus
    einen neuen Baustein und legt ihn als neue Karte daneben. Der Editor bearbeitet danach den neuen
    Baustein, ein Block im Ablauf bleibt unberührt. [Bed.]
24. **Löschen** im Editor verschiebt den eigenen Baustein in den Papierkorb, der Editor wird leer, die
    Werkstatt bleibt offen. [Bed.]

### Hilfezeile
25. Sie nennt nur, was man vergisst: Akzent, Ghost, Flam, Lautstärke, Timing, Übertragen, Breite und im
    Ablauf oder in der Ablage Strg+S. Was der Bildschirm ohnehin zeigt, bleibt draußen. [E 31.10.]
26. Passt der Text nicht in die Breite, wird die Schrift kleiner statt abgeschnitten. Eine eigene Hilfezeile
    im Datenordner ersetzt die eingebaute. [Code, Bed.]

---

## 4. Arranger (Werkstatt › Arranger)

1. **Finden:** Treffer gruppiert, je Gruppe fünf, darunter „Alle … anzeigen“. Klick auf eine Zeile hört an,
   nochmal stoppt. Das Kästchen markiert, ohne zu spielen. „+ Ablage“ und „+ Ablauf“ übernehmen die
   markierten Zeilen, ohne Markierung die aktuelle. [E 29.10., Bed., Code]
2. **Ablage** ist eine dauerhafte Sammlung, unabhängig vom Arrangement. Jede Änderung wird sofort
   gespeichert. Übernehmen in den Ablauf lässt die Karte stehen. Geleert wird nur ausdrücklich, und das ist
   rückgängig machbar. Jede Karte hat eine eigene Kennung, Bearbeiten trifft nur sie. [E 29.10., Code]
3. **Ablauf:** Blöcke sind Kopien mit Quellen-ID. Hauptbedienung ist Ziehen mit der Maus, die Knöpfe der
   Detailzeile gibt es zusätzlich. [E 29.10.] Spulen: Der weiße Positionsstrich hat oben einen Griff (nur
   wenn der Transport das Arrangement hält, spielend oder pausiert); Ziehen rastet auf Takte, Loslassen
   springt dorthin. Klick und Ziehen auf Blöcken bleiben unverändert. [E 31.10.]
4. **Lokale Umbenennung** ändert nur den Block, der Baustein behält seinen Namen. [Bed.]
5. **Takte:** Ein Block kann einen Taktbereich spielen, einsbasiert und einschließlich angezeigt. Ungültige
   Bereiche werden beim Laden gemeldet. [E 29.10.]
6. **Pause- und Klick-Blöcke** haben Taktart, Taktanzahl und Wiederholungen. [E 29.10.]
7. **Original** holt die aktuelle Fassung des Bausteins in einen Block oder alle Blöcke dieser Quelle, immer
   mit Rückfrage und rückgängig machbar. ↻ zeigt, dass die Quelle neuer ist, ✎ eine lokale Änderung.
   [E 29.10.]
8. **Fehlende Quellen** werden nie still übersprungen, der Block bleibt und ist markiert. [E 29.10.]
9. **Strg+C** kopiert den Block, **Strg+V** fügt die Kopie dahinter ein, **Strg+X** legt ihn in die Ablage,
   aber nur, wenn derselbe Baustein dort noch fehlt. [Bed.]
10. **Rückgängig** umfasst Einfügen, Entfernen, Verschieben, Wiederholungen, Bereiche und Ablage leeren. Ein
    Ziehvorgang ist ein Schritt. Vorhören und Auswahl gehören nicht dazu. [E 29.10.]
11. Der Arbeitsstand, auch ungespeichert, und die Ablage werden automatisch gemerkt. [Bed.]

---

## 5. Hauptfenster und Wiedergabe

1. Zwei Betriebsarten: **Baustein** spielt einen Baustein oder ein Arrangement, **Metronom** nur die
   Taktart. Einen Baustein wählen schaltet auf Baustein, eine Taktart wählen auf Metronom. [Bed.]
2. Wechsel mit ← → während der Wiedergabe greifen am nächsten Taktende. [Bed.]
   **Blättern wie im Menü:** Reihenfolge und Ordner sind die des Baustein-Menüs: Arrangements, Bausteine,
   Bibliothek, in jedem Ordner erst die Unterordner, dann die eigenen Einträge. ← → blättert nur im Ordner
   des aktuellen Eintrags, Strg+← → wechselt zum ersten Eintrag des vorigen bzw. nächsten Ordners. Halt ist
   jeder Ordner mit eigenen Einträgen; reine Sortier-Ordner führen in ihren ersten Unterordner, leere und im
   Menü ausgeblendete werden übersprungen. Shift+← → blättert durch die Favoriten in der Reihenfolge ihres
   Menü-Abschnitts. Nirgends geht es im Kreis: Am Ende ist Schluss, das zeigt man nicht durch Springen.
   Im Metronom gilt dasselbe für die Taktarten; Strg+← → führt von dort in den ersten bzw. letzten Ordner.
   [E 31.10.]
   **Geister-Menü:** Jeder Blätter-Tastendruck zeigt, wo man ist: linksbündig direkt unter dem Titel, bis
   zum unteren Fensterrand, damit es von Taste zu Taste stillsteht. Song-Leiste, Taktart, BPM, Lauflicht
   und Fußzeilen blenden solange aus und danach weich wieder ein. Oben ein knapper Baum bis zum Ordner
   („R.P. Bardet“, „└ Funk, Disco, R&B“, „└ Latin“), den Bereichsnamen nur für Einträge ohne Ordner.
   Darunter so viele Einträge, wie hineinpassen, möglichst alle, sonst ein Ausschnitt um den aktuellen;
   „Anfang“ und „Ende“ markiert, dazu „3 / 9“. Bei
   Strg: der Baum bis zum übergeordneten Ordner und die Nachbar-Ordner mit der Zahl ihrer Einträge. Bei
   Shift die Favoriten, im Metronom die Taktarten. Neongrün mit leichtem Schleier, keine Klicks, kein Fokus;
   nach der eingestellten Dauer blendet es in 0,25 s aus. Unter System › Menü-Ordner ganz oben: „Grüne
   Einblend-Menüs aktiv“ (Standard an) und „Dauer“ 0,5 bis 5 s (Standard 1 s), in den Einstellungen gemerkt.
   [E 31.10.] **Tooltips:** System, ganz unten rechtsbündig, „Tooltips“ an/aus für alle Fenster
   (Standard an, gemerkt); Text im Rot der Hilfezeilen (0xffe07a70). [E 31.10.]
3. **Einzählen** mit drei Stellungen: aus, START nur beim Start von vorn, AN vor jedem Start, auch nach einer
   Pause. Der Klick-Takt hat die Taktart des Starttakts. Gilt nur für den Hauptplayer. [Bed.]
4. **Tasten:** M schaltet den Mixer an und aus, im Hauptfenster, in der ganzen Werkstatt und im Mixer
   selbst. E im Hauptfenster öffnet die Werkstatt oder lädt den gewählten Baustein hinein. E in der Werkstatt
   schließt sie wieder, solange im Editor nichts ungespeichert ist, sonst passiert nichts. Das Taktart-Menü
   öffnet man per Klick. In der Werkstatt wirken nur Leertaste, Tempo hoch und runter, T, M und E; ← →
   wechseln dort nicht den Baustein. [E 31.10., Code]
5. **Loop:** Bereich ganzer Takte per Shift + Ziehen in der Takt-Leiste, die es dafür auch bei Bausteinen
   mit mehreren Takten gibt (ein Block, keine Song-Teile); auch über Blöcke eines Arrangements hinweg.
   Shift + Ziehen nahe einer Grenze (14 px) verschiebt nur diese; innerhalb des Bereichs passiert nichts,
   erst außerhalb entsteht ein neuer Bereich.
   Knopf „Loop (L)“ (links neben „Umbenennen“, so groß wie „Löschen“, nur bei Takt-Leiste) und Taste L schalten um; der Bereich
   bleibt, ist aber nur bei aktivem Loop gelb markiert; Doppelklick darauf löscht ihn. Am Bereichsende
   springt die Wiedergabe sample-genau zum Anfang (im Player, nicht über die Oberfläche) und zählt das als
   Durchlauf. Ein früher Schlag auf dem ersten Schlag des Bereichs wird nur nach einem Rücksprung auf den
   Anfang geklemmt, ein später am Ende immer (auch gegen die Sample-Rundung: höchstens aufs letzte Sample
   vor dem Rücksprung). Welche Schläge zur Runde gehören, entscheidet ihr geschriebener Platz: keine, die am
   oder hinter dem Rundenende stehen (Planende, Bereichsende oder das Taktende, an dem ein wartender Plan
   übernimmt; also auch kein Flam-Vorschlag oder früher Schlag des Takts danach), nach einem Rücksprung
   keine aus den Takten davor (auch kein spät versetzter). Ein später Schlag vor einem Rundenende erklingt
   spätestens auf dessen letztem Sample, auch vor einem Planwechsel. Am Anfang einer Runde, nach einem
   Sprung und beim Start erklingen frühe Schläge, die dort oder dahinter stehen, auf dem Anfang (wie am
   Planbeginn), ebenso ein erster Schlag, der ein halbes Sample davor rundet. Ein Tempowechsel mitten in
   der Runde und Pause/Weiter lassen keinen Schlag aus und spielen keinen doppelt; Weiter setzt die Runde
   fort (gleiche Humanisierung, gleiche Versätze, gleicher Bereichszustand). Wird der Loop in einer Runde
   ausgeschaltet, die am Bereichsanfang begann, bleiben Schläge aus den Takten davor bis zur nächsten Runde
   draußen. Liegt die Position beim Setzen oder Einschalten außerhalb, geht es am Bereichsanfang los.
   Grundsatz: Solange der Loop an ist, gibt es keine Position außerhalb des Bereichs; jeder Sprung aus der
   Oberfläche (Takt-Leiste, Zurückspulen, N/B) dorthin landet am Bereichsanfang (seekToBar). [E 31.10.] Der Bereich gehört zur gewählten Auswahl (deren
   zuletzt gesendetem Plan) und verfällt bei einer anderen; Einzählen und Vorhören setzen ihn nur aus.
   Solange die Werkstatt offen ist (oder etwas aus ihr spielt), sind Loop- und Trainer-Knopf ausgeblendet
   und die Tasten L und S wirkungslos. Beim Öffnen gehen Loop und Trainer aus (der Bereich bleibt), weil die
   Werkstatt die Position beliebig verschiebt; danach wieder eingeschaltet, springen Loop bzw. Trainer (bei
   eingeschaltetem Loop) an den Bereichsanfang, wenn die Position außerhalb liegt. [E 31.10.] Dass sie offen ist,
   zeigt „Bearbeiten“: lila getönt mit hellem Rand und einem atmenden Neon-Schein (theme::workshopGlow). Gesetzt wird er nur, solange die Takt-Leiste den zuletzt
   gesendeten Plan der Auswahl zeigt (nicht die vorige Auswahl oder eine ältere Fassung, deren Wechsel noch
   aufs Taktende wartet), damit Bereich und Sprung aus denselben Takten kommen. Shift + Ziehen innerhalb des
   Bereichs bewirkt gar nichts, auch keinen Sprung. Hat die Auswahl weniger Takte als der Bereich, wird sein
   Ende gekürzt; beginnt er hinter dem Ende, verfällt er. [E 31.10., Code]
6. **Speed-Trainer:** Knopf „Trainer (S)“ (so groß wie „Löschen“) öffnet ein kleines Menü: „Trainer an“, Von,
   Bis, Inkrement, Wdh. pro Inkrement (1 bis 5; gespeichert), je ein kleiner Schieberegler mit dem Wert rechts.
   Von und Bis in Zweierschritten, Von bleibt unter Bis (jeder Regler stoppt 2 BPM vor dem anderen, gleiche
   Skala). Voreinstellung
   Inkrement 1, Wdh. 1. An: Tempo auf „Von“, nach je n Durchläufen + Inkrement bis „Bis“, dort geht es so
   weiter. Durchlauf = Runde durch den Loop-Bereich, sonst durch das Ganze; gezählt werden nur Runden des
   Plans der Auswahl (kein Einzählen, kein Vorhören, nichts aus der Werkstatt), ein neuer Start beginnt
   die Zählung neu, schon gezählte Runden bleiben. Jeder gehörte Puls wird einzeln gezählt, auch wenn die
   Anzeige mehrere auf einmal nachholt; mehrere fällige Inkremente gehen nicht verloren. Jede Tempoänderung, die nicht vom Trainer kommt, beendet ihn. Taste S schaltet um.
   Bekannte Grenze: Das neue Tempo greift erst, wenn der Rundenanfang zu hören ist (einige zehn ms in die
   neue Runde hinein). [E 31.10.]
   **Speicherplätze:** zehn Plätze oben im Trainer-Menü, in einem eigenen Feld (`trainer.json` im
   Datenordner). Klick auf einen leeren legt genau die jetzige Einstellung ab (Auswahl als ID bzw. Taktart,
   Loop-Bereich mit an/aus, Von, Bis, Inkrement, Wdh.) und öffnet das Namensfeld mit dem Titel als Vorschlag.
   Vor dem × (mit kleinem Abstand) ein Kasten: oben der Bereich („T. a-b“, sonst „ganz“), darunter knapp
   „Von→Bis“. Der zuletzt geladene oder abgelegte Platz ist gelb umrandet, bis eine andere Auswahl kommt. Klick auf einen
   belegten lädt: Auswahl wählen, Bereich setzen und an dessen Anfang springen (beim Abspielen startet der
   neue Plan dort), Tempo auf Von, Trainer aus bis S. Doppelklick benennt um, Rechtsklick überschreibt oder
   leert, × leert. Gelöschte Einträge: „(fehlt)“, Laden geht nicht. Dasselbe Lied auf mehreren Plätzen ist
   erlaubt. [E 31.10.]
7. **Verlauf:** Alt+← / Alt+→ wie im Browser durch die zuletzt gewählten Einträge (Bausteine, Arrangements,
   Taktarten mit Betriebsart), höchstens 50, nach IDs, gelöschte übersprungen; vor nur nach zurück, eine
   neue Wahl verwirft das Vorwärts-Stück; nach dem Neustart leer. Keine Verlaufsliste im Geister-Menü, es
   zeigt nur den Ordner des zurückgeholten Eintrags. [E 31.10.]
8. **Vorhören** gibt es im Baustein-Menü mit P, in der Suche mit ▶ und im Finden mit Klick. Esc stellt
   wieder her, was vorher lief. Hat man das Anspielen selbst gestoppt, bleibt es still. Beim Vorhören ist
   der Titel im Hauptfenster grün. Grün heißt überall „klingt“ oder „ein Klick spielt“. [Bed., E 31.10.]

---

## 6. Ordner, Menü und Suche

1. **Standardplätze**, solange ein Eintrag keinem Ordner zugeordnet ist: Arrangements oben, eigene Bausteine
   nach Band und dann Song, Bausteine ohne Band oben, die Bibliothek nach Stil. [Bed., Code]
2. Die **Ordnerstruktur** steht in `ordner.json`. Nach der ersten Bearbeitung eines Bereichs ist dessen
   ganzer Baum gespeichert. Die Baustein- und Arrangement-Dateien bleiben dabei unverändert. [Code]
3. Gleichnamige Ordner werden zusammengelegt, ohne Rücksicht auf Groß- und Kleinschreibung. Löschen geht nur
   bei leeren Ordnern. [Bed.]
   **Sortieren** ordnet einmalig: Unterordner und Einträge je alphabetisch nach dem angezeigten Namen, ohne
   Groß- und Kleinschreibung, Zahlen nach Wert, ä ö ü wie a o u, Zeichen vorn zählen nicht; mit „mit
   Unterordnern“ rekursiv. Neue Einträge kommen weiter ans Ende, von Hand verschieben geht wie vorher. [Code]
4. **Menü-Ordner** blendet Ordner der obersten Ebene nur im Baustein-Menü aus. Suche und Arranger kennen
   weiterhin alles. Die Liste dort steht alphabetisch, die Reihenfolge im Menü bleibt die der Ordnerstruktur.
   **Konfigurationen:** zehn Plätze (`menue-ordner.json`). 1 Minimal (Bibliothek, Arrangement-Ordner
   „Beispiele“) und 10 Maximal (alles) sind fest und werden nicht gespeichert; 8 „Nur Bausteine“ und 9 „Nur
   Arrangements“ werden vorbelegt, solange es die Datei nicht gibt, und lassen sich leeren. Ein Platz hält je
   Bereich „alle“ (beim Ablegen nichts ausgeblendet; spätere Ordner kommen mit) oder die Liste der sichtbaren
   Ordner (spätere bleiben aus); Namen ohne Groß- und Kleinschreibung, verschwundene werden übergangen.
   Abrufen setzt alle Haken in einem Schreibvorgang; die Ausblendliste wird dabei ganz ersetzt. Wird ein
   Ordner der obersten Ebene umbenannt, ziehen die gespeicherten Listen mit, auch wenn er dabei in einen
   vorhandenen Ordner aufgeht. Gelb umrandet ist der Platz, dessen Auswahl genau den
   jetzigen Haken entspricht. [Code] Benennt man einen ausgeblendeten Ordner um, bleibt er ausgeblendet. Verschwindet ein Name
   von der obersten Ebene, vergisst die Liste ihn. [Bed., Code]
5. Im Menü fällt vorn weg, was die Ordner schon sagen: „Carcass – CJQ – 01 Intro“ im Ordner Carcass › CJQ
   heißt „01 Intro“. [Code]
6. **Suche** über Name, Band, Stil, Taktart (`7/8`), Tempo (`160-190 bpm`) und die benutzten Spuren und
   Spielweisen, etwa `besen`, `sidestick`, `glocke`, `flam`. Alle Wörter müssen passen. [Bed., E 29.10.]

---

## 7. Abzeichen

1. **Farbe mal Form:** 12 Farben. Formen 0 bis 16 sind eigene geometrische Marken, ab 17 die Google-Symbole
   aus `assets/material-symbols/auswahl.txt`. Die Reihenfolge der Liste ist die Bedeutung, nach der
   endgültigen Auswahl wird nur noch angehängt. [E 31.10.]
2. Ein Baustein bekommt sein Abzeichen beim ersten Auftauchen, nach der gewählten Regel, und zwar ein freies,
   solange es freie gibt. Danach bleibt es, bis man neu verteilt. [Code]
3. Ein gelöschter Baustein gibt sein Abzeichen frei, ein wiederhergestellter bekommt ein neues. [Code]
4. **Wächst die Palette**, ziehen Bausteine, die sich ein Abzeichen teilen, auf freie um. Eindeutige bleiben.
   [Code]
5. **Erst eigene, dann Google:** Gruppen sind die Bibliothek, jede Band-Angabe und die eigenen Bausteine ohne
   Band. Die kleinsten Gruppen bekommen die eigenen Marken, solange eine ganze Gruppe noch in die 204
   Abzeichen passt. Alle anderen bekommen Google-Symbole. [E 31.10.]
6. **Gemischt:** Ein zweiter Hash entscheidet je Baustein. Der Anteil eigener Marken ist etwa 204 geteilt
   durch die Zahl der Bausteine, gleichmäßig über alle Ordner. [E 31.10.]
7. **Neu verteilen** vergibt alle Abzeichen nach der Regel und würfelt dabei jedes Mal neu. Vorher sichert die
   App die alte Zuordnung, die letzten zehn bleiben. Klappt die Sicherung nicht, wird nicht gewürfelt.
   [E 31.10., Code]
8. **Darstellung:** Google-Symbole fett und gefüllt im Grau der eigenen Marken, ohne Rand. Eigene Marken grau
   mit dunkler Linie, das X weiß. Pause- und Klick-Blöcke haben feste Grautöne mit Balken bzw. Punkt.
   [E 31.10., Code]

---

## 8. Fenster und Start

1. **Startbild** ist der erste Inhalt des Hauptfensters. Das Schlagzeug liegt genau dort, wo danach das
   Hintergrundbild liegt, mit 45 % statt 5 %. Oben steht „Initializing“, unten fett „Drummer“. Es blendet in
   etwa 130 ms ein, dann lädt die Bibliothek, dann erscheint die Oberfläche. [E 31.10.]
2. Das Hauptfenster kommt erst auf den Desktop, wenn Größe und Platz feststehen. Es startet immer mit
   1000 × 700 Pixeln in der Bildschirmmitte. [Code]
3. Die Werkstatt merkt sich Größe und Lage. [Code]
4. **Version** v0.2.‹Commits›, beim Bauen gesetzt. [E 31.10.]

---

**Startpaket und Quellen** [E 31.10.]
- **Startpaket:** Nur beim ersten Start (weder `patterns/` noch `songs/` im Datenordner) kopiert das Programm
  `community/*/` (Bausteine), `community/*/songs/` und `startpaket/songs/` (Arrangements) sowie
  `startpaket/ordner.json` hinein; nie wieder danach. Vorhandenes wird nie überschrieben.
- **Quellen (System › Quellen):** `tools/import/quellen.py` lädt je Quelle von der Originalquelle, wandelt in einen
  Zwischenordner um und schreibt ins Import-Log, was wegfiel oder vereinfacht wurde (Zeilen „HINWEIS:“ der
  Umwandler). Das Programm kopiert nur neue Dateien in den Datenordner und legt nur die neu angelegten
  Arrangements in einen Ordner je Band; Vorhandenes, ob eigenes oder von einer älteren Installation, bleibt
  unverändert an seinem Platz. Danach lädt es die Bibliothek neu und behält die Auswahl.
- **Abstürze** landen mit Aufrufstapel in `app.log`.

## 9. Überholt in den älteren Entwürfen

- **KONZEPT.md:** „Status: Planung (noch kein Code)“ und der Fahrplan sind veraltet. E5 (Kit: Aasimonster),
  E6 (JSON) und E8 (ALSA bzw. PipeWire, 48 kHz) sind praktisch entschieden. Die Auflösung gilt pro Takt,
  nicht pro Pattern. „Im Programm stehen keine Bandnamen“ gilt nur für die mitgelieferte Bibliothek.
- **ARRANGER.md:** Die erste Tastentabelle ist überholt: Verschieben ist Alt+Pfeil, Entf legt nichts in die
  Ablage zurück, Strg+D, L und Strg+F gibt es nicht, die Suche ist Strg+K. Die Ablage wird nicht mehr mit dem
  Song gespeichert (Entscheidung 5). Bausteine im Song sind Kopien, keine Verweise (Entscheidung 2 und 7).
  Aufklappbare Takt-Kacheln, „Teilen“ und „+ leerer Takt“ in der Ablage sind nicht gebaut. Entscheidung 8
  sagt „Raster“ für den Editor und „Bibliotheks-Pattern“ für das Ergebnis von „Speichern unter“; gemeint
  ist ein eigener Baustein.
- **PATTERN-MODELL.md:** Die ID ist kein reines UUID, sondern `lib.…` bzw. `user.<slug>-<12 Hex>`.
- **BIBLIOTHEK.md:** Labels, gespeicherte Sichten und der `meta`-Block sind nicht gebaut. Stattdessen gibt
  es die Ordnerstruktur, Band- und Song-Angaben als Tags und die Menü-Ordner.

---

## 10. Zu klären

Inkonsequenzen und Lücken, die beim Erstellen aufgefallen sind. Jede wird entschieden und dann oben als
Regel eingetragen oder gestrichen.

1. *(erledigt 31.10.: Loop mit Shift+Ziehen und Speed-Trainer gebaut, siehe Abschnitt 5)*
2. **Doppelklick im Finden:** Laut Entscheidung 1 vom 29.10. soll ein Doppelklick in die Ablage übernehmen.
   Gebaut sind nur die Knöpfe und Enter.
3. **Fensterlage:** KONZEPT sagt, Lage und Größe werden gespeichert. Das gilt nur für die Werkstatt, das
   Hauptfenster startet immer mittig mit 1000 × 700.
4. *(erledigt 31.10.: Das Werkstatt-Fenster kommt wie das Hauptfenster erst auf den Desktop, wenn Titelleiste
   und Größe feststehen; das leere Rechteck oben links sollte weg sein)*
5. **„Kleinste Ordner“ bei den Abzeichen:** Die Regel zählt nach Band-Angabe, nicht nach dem tatsächlichen
   Ordner in `ordner.json`. Wer Bausteine in andere Ordner verschiebt, ändert ihre Gruppe nicht. Die
   Bedienungsanleitung spricht von Ordnern.
6. **Abzeichen in Song-Leisten beim Start:** Die Leisten gespeicherter Arrangements entstehen, bevor neue
   Bausteine ihr Abzeichen bekommen. Ein neuer Baustein in einem gespeicherten Arrangement zeigt dort bis zum
   nächsten Neuladen ein vorläufiges Abzeichen.
7. **Alte Einträge im Abzeichen-Register:** Bausteine, die außerhalb der App verschwinden, halten ihr
   Abzeichen, bis man neu verteilt. Bei 9804 Abzeichen unkritisch.
8. **Symbol-Auswahl:** Die 800 Google-Symbole sind vorläufig. Die Durchsicht der Bögen steht aus, danach
   einmal neu verteilen.
9. **Lizenzen:** Im Lizenzabschnitt der README fehlen die Google-Symbole (Apache-2.0), das CrocellKit
   (CC-BY 4.0, Sidestick und HiHat-Pedal) und Karoryfer Swirly Drums (CC0, Besen). Die Quellen und Lizenzen
   der importierten Pattern-Sammlungen fehlen ebenfalls im Repo.
10. **Versionsnummer:** Sie zählt alle Commits, nicht die seit v0.2. Nach 0.1.27 folgt 0.2.28.
11. *(erledigt 31.10.: L schaltet jetzt den Loop, der Mixer liegt auf M)*
12. *(erledigt: Geister-Menü abschaltbar unter System › Menü-Ordner)*
