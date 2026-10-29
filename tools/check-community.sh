#!/bin/bash
# Prüft Pattern-Sätze unter community/: Tabs werden mit patternc übersetzt, alle JSON-Dateien
# (übersetzte und beigelegte) werden mit app_tests probeweise geladen.
# Erwartet einen fertigen Build (build/).   Aufruf: tools/check-community.sh [satz-ordner ...]
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATTERNC="$ROOT/build/patternc_artefacts/Release/patternc"
APPTESTS="$ROOT/build/app_tests_artefacts/Release/app_tests"
if [ ! -x "$PATTERNC" ] || [ ! -x "$APPTESTS" ]; then
    echo "Bitte erst bauen: cmake --build build"
    exit 1
fi

SETS=("$@")
if [ ${#SETS[@]} -eq 0 ]; then
    shopt -s nullglob
    SETS=("$ROOT"/community/*/)
    shopt -u nullglob
fi
if [ ${#SETS[@]} -eq 0 ]; then
    echo "Keine Pattern-Sätze unter community/"
    exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
status=0
for set in "${SETS[@]}"; do
    set="${set%/}"
    [ -d "$set" ] || continue
    name="$(basename "$set")"
    out="$TMP/$name"
    mkdir -p "$out"
    if compgen -G "$set/*.tab" > /dev/null; then
        # patternc ersetzt den Inhalt seines Zielordners: deshalb immer ein frischer Temp-Ordner
        if ! "$PATTERNC" "$set" "$out/tabs"; then
            echo "FEHLER in $name (Tabs)"
            status=1
            continue
        fi
        cp "$out/tabs"/*.json "$out/" 2>/dev/null || true
    fi
    cp "$set"/*.json "$out/" 2>/dev/null || true
    if ! compgen -G "$out/*.json" > /dev/null; then
        echo "LEER  $name (keine .tab oder .json)"
        continue
    fi
    if (cd "$ROOT" && "$APPTESTS" library-src "$out") > "$out/log.txt" 2>&1; then
        echo "OK    $name: $(ls "$out"/*.json | wc -l) Patterns"
    else
        echo "FEHLER in $name:"
        tail -20 "$out/log.txt"
        status=1
    fi
done
exit $status
