#!/bin/bash
# Startet HZ-Drummer und schreibt Ausgaben nach ~/.cache/HZ-Drummer/run.log (zur Fehlersuche)
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG="$HOME/.cache/HZ-Drummer/run.log"
mkdir -p "$(dirname "$LOG")"
{
    echo "=== Start $(date '+%Y-%m-%d %H:%M:%S')"
    "$ROOT/build/HZDrummer_artefacts/Release/HZ-Drummer" "$@"
    echo "=== Ende, Exit-Code $?"
} >> "$LOG" 2>&1
