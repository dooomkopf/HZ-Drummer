#!/bin/bash
# Startet HZ-Drummer und schreibt Ausgaben nach ~/.cache/HZ-Drummer/run.log (zur Fehlersuche).
# Hat der Datenordner ein Git-Remote, wird er vorher abgeglichen und danach hochgeladen (sync-daten.sh).
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG="$HOME/.cache/HZ-Drummer/run.log"
mkdir -p "$(dirname "$LOG")"
{
    echo "=== Start $(date '+%Y-%m-%d %H:%M:%S')"
    "$ROOT/tools/sync-daten.sh" holen
    "$ROOT/build/HZDrummer_artefacts/Release/HZ-Drummer" "$@"
    echo "=== Ende, Exit-Code $?"
    "$ROOT/tools/sync-daten.sh" senden
} >> "$LOG" 2>&1
