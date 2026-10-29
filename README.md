# HZ-Drummer

Drum-Begleitprogramm zum Gitarre-Üben, gebaut für Metal: realistische Samples, Patterns in allen Taktarten bis hin zu Polyrhythmen, eine Bibliothek von D-Beat bis Tech-Death, ein Raster-Editor für eigene Grooves und ein Arranger, der aus Bausteinen ganze Songabläufe macht. C++ mit JUCE, entwickelt und getestet unter Ubuntu 22.04.

Das Programm ist ein Übungswerkzeug, kein Drum-Computer für Aufnahmen: Es soll ohne Umwege den richtigen Groove im richtigen Tempo liefern, damit man zur Gitarre greifen kann.

![Hauptfenster während der Wiedergabe eines Songs](docs/bilder/hauptfenster.png)

<p>
<img src="docs/bilder/raster.png" alt="Raster-Editor mit Mute und Solo je Spur" width="49%">
<img src="docs/bilder/arranger.png" alt="Arranger mit Suche, Ablage und Ablauf" width="49%">
</p>
<p>
<img src="docs/bilder/suche.png" alt="Schnellsuche mit Abzeichen" width="49%">
<img src="docs/bilder/mixer.png" alt="Mixer je Instrument" width="49%">
</p>

## Was es kann

- **Patterns** in beliebigen Taktarten, auch gemischt innerhalb eines Patterns, mit Akzenten, Ghost-Notes, Flams, offener HiHat, Swing und einstellbarer „Menschlichkeit“.
- **Bibliothek** mit Grooves nach Stil (D-Beat, Thrash, Death, Grind, Doom, Hardcore, Prog, Tech-Death, Jazz) und Vier-Takt-Phrasen mit Übergängen.
- **Raster-Editor** („Werkstatt“): Schläge setzen, Lautstärke ziehen, Takte und Taktarten je Takt ändern, Mute und Solo je Spur, Rückgängig und Wiederholen, sofort hörbar.
- **Arranger**: Songabläufe aus Bausteinen, mit Ablage, Taktbereichen, lokalen Varianten und Sprung auf jeden Block.
- **Suche** über alles mit Filtern wie `7/8` oder `160-190 bpm`, Vorhören direkt im Menü.
- **Einzählen** vor dem Start, Tap-Tempo, Mixer je Instrument, Metronom mit eigenen Taktarten.
- **Papierkorb** für eigene Patterns und Songs.
- **Community-Bausteine**: kurze Loops von Carcass und Exhorder zum Mitspielen, siehe [community/](community/).

## Schnellstart

Die Schritte sind so geschrieben, dass sie ein Mensch oder ein KI-Assistent direkt ausführen kann. Getestet unter Ubuntu 22.04.

```bash
sudo apt install -y build-essential cmake ninja-build git pkg-config libasound2-dev libjack-jackd2-dev pipewire-audio-client-libraries libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libglu1-mesa-dev mesa-common-dev
git clone --recurse-submodules https://github.com/dooomkopf/HZ-Drummer.git
cd HZ-Drummer
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
(cd build && ctest --output-on-failure)       # Engine- und App-Tests
./tools/install-desktop.sh                    # Starter im Anwendungsmenü (optional)
./tools/run.sh                                # oder direkt starten
```

Ohne Kit startet das Programm, spielt aber nichts. Deshalb einmalig:

## Drum-Kit einrichten

Die Samples sind nicht Teil dieses Repos. Das Programm nutzt das **Aasimonster**-Kit von [DrumGizmo](https://drumgizmo.org) (CC-BY 4.0), ein mehrfach mikrofoniertes Metal-Kit. Das Werkzeug `kitprep` mischt es einmalig auf Stereo-Samples herunter.

```bash
mkdir -p kits-src && cd kits-src
# Aasimonster-Kit von drumgizmo.org herunterladen (Seite "Kits") und hier entpacken,
# es entsteht ein Ordner wie kits-src/aasimonster2 mit einer aasimonster.xml
cd aasimonster2
../../build/kitprep_artefacts/Release/kitprep aasimonster.xml ../../kits/aasimonster "Aasimonster (Metal)"
```

Danach liegt das fertige Kit unter `kits/aasimonster/` (etwa 400 MB), das Programm findet es beim Start. Ordner `kits/` und `kits-src/` bleiben außerhalb der Versionsverwaltung.

## Aufbau

| Ordner | Inhalt |
|---|---|
| `src/engine/` | Reines C++ ohne JUCE: Taktarten, Pattern-Modell, Tab-Parser, Pattern-Player (sample-genau, Humanisierung), Sample-Player, Bearbeitungsfunktionen |
| `src/app/` | JUCE-Oberfläche: Hauptfenster, Werkstatt mit Raster-Editor und Arranger, Suche, Mixer, Dateiformate |
| `library-src/` | Mitgelieferte Patterns als Drum-Tabs (Quelle, lesbar) |
| `library/` | Daraus übersetzte JSON-Bibliothek (wird vom Programm geladen) |
| `community/` | Pattern-Sätze von Mitwirkenden, siehe [CONTRIBUTING.md](CONTRIBUTING.md) |
| `tools/` | `patternc` (Tabs → JSON), `kitprep` (DrumGizmo-Kit → Stereo), `gpdrums` (Guitar-Pro-Schlagzeugspur → Patterns), Starter, Prüfskript für Community-Sätze |
| `tests/` | `engine_tests` (Timing, Taktarten, Parser, Player, Bearbeitung), `app_tests` (Dateiformate, Suche, Arranger-Modell) |
| `docs/` | Bedienung, Konzept, Formate, Entscheidungen |

Nutzerdaten liegen unter `~/.config/HZ-Drummer/`: eigene Patterns, Songs, Papierkorb, Einstellungen. Protokolle liegen unter `~/.cache/HZ-Drummer/`.

## Dokumentation

- [Bedienung](docs/BEDIENUNG.md): Hauptfenster, Tasten, Raster-Editor, Arranger
- [Dateiformate](docs/FORMATE.md): Drum-Tabs, Pattern-JSON, Song-JSON, Datenordner
- [Mitmachen](CONTRIBUTING.md): eigene Patterns beisteuern, Code-Beiträge, Fehler melden
- [Konzept](docs/KONZEPT.md), [Pattern-Modell](docs/PATTERN-MODELL.md), [Arranger](docs/ARRANGER.md), [Bibliothek](docs/BIBLIOTHEK.md), [Variationen (Entwurf)](docs/VARIATIONEN.md)
- [Entwicklungsumgebung](docs/SETUP.md)

## Lizenz

HZ-Drummer steht unter der **GNU Affero General Public License v3.0**, siehe [LICENSE](LICENSE). Das schließt die mitgelieferten Patterns ein. Pattern-Sätze unter `community/` tragen ihre eigene Lizenz, siehe die jeweilige README.

- [JUCE](https://juce.com) wird als Submodul eingebunden und unter der AGPLv3 genutzt.
- Das Aasimonster-Kit stammt von [DrumGizmo](https://drumgizmo.org) und steht unter CC-BY 4.0. Es ist nicht Teil dieses Repos.
- `tools/gpdrums` nutzt zur Laufzeit die TuxGuitar-Bibliothek (LGPL), die nicht mitgeliefert wird.

Das Repo enthält keine Transkriptionen kompletter Songs und keine Guitar-Pro- oder MIDI-Dateien fremder Stücke.

## Unterstützen

**Buy me a coffee, if you like it …** ☕

Per Bitcoin Lightning:

<img src="docs/bilder/lightning.png" alt="QR-Code für eine Lightning-Spende" width="220">

<details>
<summary>Als Text zum Kopieren (BOLT12-Angebot, die Wallet muss BOLT12 unterstützen)</summary>

```text
lno1pgqppmsrse80qf0aara4slvcjxrvu6j2rp5ftmjy4yntlsmsutpkvkt6878sxu4q9q68umuk7azekmrdd5wpsxc3duv4pxj736tkv6qpmsaq7g3uqgp05s9vqknhkv9hrdqfgl374flv0p5npr4xv0yq9wz6rxgfn6pvdzgqxdtly6z4y2q5khs4hsvwgyh4c586r66pwfhmu27h4aw2qtn9v54ez0rm3q9qn7fnf9kwl8yvc3q55yk40yvqyq5dxwwrc2rxgah0g2d0lg6mn50rq53dw96d2hzy6wae20xer4g7qqerh58wkn38e540v93f9sscxpzvr2qp0gtwmrla5y9me7kq2hl4wa0n4nfrdc7qctsvhs7ysr9thrtlwknq
```

</details>
