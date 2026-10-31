# Kopiert neue Bausteine in den Datenordner (nur bei geschlossenem HZ-Drummer): erst Sicherung, nichts überschreiben.
# Optional ein Präfix, dessen Bausteine vorher entfernt werden (ein früherer Import derselben Quelle).
# Aufruf: python3 -I install_patterns.py <patterns-ordner> <daten-ordner> <sicherungs-ordner> [ersetzen-präfix]
import glob
import os
import shutil
import subprocess
import sys

if subprocess.run(["pgrep", "-x", "HZ-Drummer"], capture_output=True).returncode == 0:
    sys.exit("HZ-Drummer läuft: erst beenden, dann einspielen (das Programm schreibt sonst seinen Stand darüber).")

SRC, DATA, BACKUP = sys.argv[1:4]
REPLACE = sys.argv[4] if len(sys.argv) > 4 else None
names = sorted(f for f in os.listdir(SRC) if f.endswith(".json"))
clash = [f for f in names if os.path.exists(os.path.join(DATA, "patterns", f)) and not (REPLACE and f.startswith(REPLACE))]
if clash:
    sys.exit(f"Abbruch, schon vorhanden: {clash[:5]}")
shutil.copytree(DATA, BACKUP, symlinks=True)
removed = 0
if REPLACE:
    for old in glob.glob(os.path.join(DATA, "patterns", REPLACE + "*.json")):
        os.remove(old)
        removed += 1
for f in names:
    shutil.copy2(os.path.join(SRC, f), os.path.join(DATA, "patterns", f))
print("Sicherung:", BACKUP, "| ersetzt:", removed, "| eingespielt:", len(names), "Bausteine")
