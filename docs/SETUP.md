# Entwicklungsumgebung einrichten

Getestet unter Ubuntu 22.04.

## System-Pakete

```bash
sudo apt install -y build-essential cmake git pkg-config ninja-build libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libmp3lame0
```

Hinweis: JACK über PipeWire ist optional. Unter Ubuntu 22.04 heißt das Paket dafür `pipewire-audio-client-libraries` (nicht `pipewire-jack`), freischalten wie in der README beschrieben.

## Guitar-Pro-Import (optional)

`tools/gpdrums` liest Schlagzeugspuren aus Guitar-Pro-Dateien; Guitar Pro 6/7/8 und MuseScore gehen ohne Java, GP3/4/5
über die TuxGuitar-Bibliothek. Der
Band-Import (`import_band.py`), System › Quellen und der Eingangsordner nutzen den Leser `GpDrums2.java` für **TuxGuitar 2.x** (aktuell 2.1.1, github.com/helge17/tuxguitar, auch als .deb) oder `GpDrums156.java` für **TuxGuitar 1.5.6**, je nachdem, was installiert ist:
das `.deb` installieren (legt TuxGuitar nach `/opt/tuxguitar`) oder das Linux-Archiv so entpacken, dass
`/opt/tuxguitar/lib/tuxguitar-lib.jar` entsteht. Das Archiv enthält einen eigenen Oberordner, deshalb
`--strip-components=1`:

```bash
sudo mkdir -p /opt/tuxguitar && sudo tar xzf tuxguitar-*.tar.gz -C /opt/tuxguitar --strip-components=1
```

Liegt TuxGuitar woanders, die Variable `TUXGUITAR=<ordner>` setzen. Dazu Java und Python (eine JRE reicht, ein JDK
ist nicht nötig):

```bash
sudo apt install -y default-jre python3
```

Der ältere Leser `GpDrums.java` passt zum TuxGuitar-Paket von Ubuntu (`sudo apt install tuxguitar`).
