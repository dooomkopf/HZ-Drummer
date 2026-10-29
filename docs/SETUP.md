# Entwicklungsumgebung einrichten

Getestet unter Ubuntu 22.04.

## System-Pakete

```bash
sudo apt install -y build-essential cmake ninja-build git pkg-config libasound2-dev libjack-jackd2-dev pipewire-audio-client-libraries libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libglu1-mesa-dev mesa-common-dev
```

Hinweis: Unter Ubuntu 22.04 heißt das PipeWire-JACK-Paket `pipewire-audio-client-libraries` (nicht `pipewire-jack`).

## Guitar-Pro-Import (optional)

`tools/gpdrums` liest Schlagzeugspuren aus Guitar-Pro-Dateien (GP3/4/5) über die TuxGuitar-Bibliothek:

```bash
sudo apt install -y tuxguitar default-jdk python3
```
