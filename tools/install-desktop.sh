#!/bin/bash
# Legt einen Starter für HZ-Drummer im Anwendungsmenü an (danach in der Taskleiste anheftbar).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/tools/run.sh"
ICON="$ROOT/assets/icon.png"
[ -f "$ICON" ] || ICON="audio-x-generic"
mkdir -p "$HOME/.local/share/applications"
cat > "$HOME/.local/share/applications/hz-drummer.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=HZ-Drummer
Comment=Drum-Begleitung zum Gitarre-Üben
Exec=$BIN
Path=$ROOT
Icon=$ICON
Terminal=false
Categories=AudioVideo;Audio;Music;
StartupWMClass=HZ-Drummer
DESKTOP
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
echo "Starter angelegt: ~/.local/share/applications/hz-drummer.desktop (Icon: $ICON)"
