#!/bin/bash
# Gleicht den Datenordner (~/.config/HZ-Drummer: eigene Patterns, Songs, Ordner) mit seinem
# Git-Remote ab, falls einer eingerichtet ist. tools/run.sh ruft es vor dem Start und nach dem Ende:
#   sync-daten.sh holen    eigene Änderungen committen, dann den neuesten Stand holen
#   sync-daten.sh senden   eigene Änderungen committen, holen, hochladen
# Ohne Git-Repo oder ohne Remote "origin" im Datenordner passiert nichts. Konflikte werden nicht
# automatisch aufgelöst: Dann bleibt der lokale Stand, und eine Meldung steht im Startprotokoll.
DATA="$HOME/.config/HZ-Drummer"
export GIT_TERMINAL_PROMPT=0   # never wait for a password prompt

git -C "$DATA" rev-parse --git-dir >/dev/null 2>&1 || exit 0
git -C "$DATA" remote get-url origin >/dev/null 2>&1 || exit 0
branch="$(git -C "$DATA" branch --show-current)"

commit_local() {
    git -C "$DATA" add -A
    git -C "$DATA" diff --cached --quiet \
        || git -C "$DATA" commit -q -m "Abgleich $(hostname) $(date '+%Y-%m-%d %H:%M')" \
        || echo "Abgleich: lokaler Commit fehlgeschlagen (git user.name/user.email gesetzt?)"
}

pull() {
    if ! timeout 20 git -C "$DATA" pull -q --rebase origin "$branch"; then
        git -C "$DATA" rebase --abort 2>/dev/null
        echo "Abgleich: neuer Stand nicht geholt (kein Netz oder Konflikt), es gilt der lokale Stand"
        return 1
    fi
}

case "$1" in
    holen)
        commit_local
        pull
        ;;
    senden)
        commit_local
        if pull; then
            timeout 20 git -C "$DATA" push -q origin "$branch" || echo "Abgleich: hochladen fehlgeschlagen, beim nächsten Mal wieder"
        fi
        ;;
    *)
        echo "Aufruf: $0 holen|senden" >&2
        exit 1
        ;;
esac
exit 0
