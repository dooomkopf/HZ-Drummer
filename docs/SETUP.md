# Entwicklungsumgebung einrichten

Getestet unter Ubuntu 22.04.

## System-Pakete

```bash
sudo apt install -y build-essential cmake git pkg-config ninja-build libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev
```

Hinweis: JACK über PipeWire ist optional. Unter Ubuntu 22.04 heißt das Paket dafür `pipewire-audio-client-libraries` (nicht `pipewire-jack`), freischalten wie in der README beschrieben.

## Guitar-Pro-Import (optional)

`tools/gpdrums` liest Schlagzeugspuren aus Guitar-Pro-Dateien (GP3/4/5) über die TuxGuitar-Bibliothek:

```bash
sudo apt install -y tuxguitar default-jdk python3
```
