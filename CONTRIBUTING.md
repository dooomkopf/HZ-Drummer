# Mitmachen

Beiträge laufen über Pull Requests. Am willkommensten sind **eigene Patterns**: Grooves, Fills und Phrasen, die beim Üben helfen.

## Patterns beisteuern

1. Einen Ordner `community/<dein-satz>/` anlegen, zum Beispiel `community/meinname-thrash/`.
2. Patterns als **Drum-Tabs** (`.tab`, bevorzugt, lesbar und leicht zu korrigieren) oder als **Pattern-JSON** (`.json`, wie das Programm sie unter `~/.config/HZ-Drummer/patterns/` speichert) hineinlegen. Beide Formate sind in [docs/FORMATE.md](docs/FORMATE.md) beschrieben.
3. Eine `README.md` in den Ordner: wer, was, woher die Idee stammt, und die Lizenz des Satzes. Empfohlen ist **CC0 1.0** (gemeinfrei), sonst **CC-BY 4.0**.
4. In Tabs eine Kennung `user.community-<satz>-<name>` setzen, zum Beispiel `id: user.community-meinname-thrash-gallop-fill`. So behandelt das Programm die Patterns beim Nutzer als eigene: sie lassen sich überschreiben und in den Papierkorb legen. Ohne `id:` bekämen sie eine `lib.`-Kennung und wären schreibgeschützt.
5. Prüfen, dann Pull Request:

```bash
cmake --build build                 # falls noch nicht gebaut
tools/check-community.sh            # übersetzt alle Tabs und lädt alle JSON-Dateien probeweise
```

Die gleiche Prüfung läuft bei jedem Pull Request automatisch.

### Was hineingehört und was nicht

- Bausteine bis etwa acht Takte: Grooves, Fills, Übergänge, Phrasen. Herkunftsangaben wie „Band – Song – Teil“ im Namen sind in Ordnung.
- **Nicht:** komplette Schlagzeugspuren ganzer Songs, Guitar-Pro- oder MIDI-Dateien fremder Stücke, Bildschirmfotos von Tab-Seiten, Dateien mit unklarer Lizenz.

### Einbau beim Nutzer

- `.json`-Dateien nach `~/.config/HZ-Drummer/patterns/` kopieren, dann das Programm neu starten.
- `.tab`-Dateien erst übersetzen, **immer in einen leeren Zwischenordner**, nie direkt in den Patterns-Ordner, denn `patternc` ersetzt den Inhalt seines Zielordners:

```bash
./build/patternc_artefacts/Release/patternc community/<satz> /tmp/hz-out
cp /tmp/hz-out/*.json ~/.config/HZ-Drummer/patterns/
```

## Code-Beiträge

- Modular, keine Datei über etwa 500 Zeilen, Kommentare auf Englisch, Oberfläche auf Deutsch (eine englische Oberfläche ist geplant).
- Engine (`src/engine/`) bleibt frei von JUCE und wird mit `engine_tests` geprüft; Dateiformate und Modelle mit `app_tests`.
- Vor dem Pull Request: `cmake --build build && (cd build && ctest --output-on-failure)`.
- Größere Änderungen an Bedienung oder Formaten bitte vorher als Issue beschreiben.

## Fehler melden

Ein Issue mit: was du getan hast, was passiert ist, was du erwartet hast, und wenn möglich die letzten Zeilen aus `~/.cache/HZ-Drummer/app.log` (im Programm unter System › Logdateien) sowie `~/.cache/HZ-Drummer/run.log` bei Abstürzen.
