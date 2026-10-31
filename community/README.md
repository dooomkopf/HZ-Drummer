# Community-Patterns

Hier liegen Pattern-Sätze zum Mitspielen, je Satz ein Ordner mit `.json`- oder `.tab`-Dateien und einer README. Wie man einen eigenen Satz beisteuert, steht in [CONTRIBUTING.md](../CONTRIBUTING.md), die Formate in [docs/FORMATE.md](../docs/FORMATE.md).

| Satz | Inhalt |
|---|---|
| [carcass](carcass/) | 24 kurze Loops aus *Corporal Jigsore Quandary*, *Incarnated Solvent Abuse* und *No Love Lost* |
| [exhorder](exhorder/) | 13 Ein-Takt-Loops aus *Desecrator*, *Legions of Death* und *My Time* |
| [groove-midi](groove-midi/) | 892 Bausteine und 21 Arrangements aus dem Groove MIDI Dataset von Google Magenta (CC BY 4.0) |
| [bardet](bardet/) | 421 Bausteine aus R.P. Bardets „260 Drum Machine Patterns“, Hydrogen-Fassung (CC BY-SA) |

## Einbau

Ein frischer Install legt beim ersten Start alle Sätze, ihre Arrangements, drei Beispiel-Arrangements und die
Ordner dazu (aus `startpaket/`) automatisch in den Datenordner; vorhandene Daten bleiben unberührt. Von Hand
geht es so: JSON-Dateien in den eigenen Patterns-Ordner kopieren und das Programm neu starten:

```bash
mkdir -p ~/.config/HZ-Drummer/patterns
cp community/carcass/*.json community/exhorder/*.json ~/.config/HZ-Drummer/patterns/
```

Die Kennungen beginnen mit `user.community-` (Groove MIDI `user.gmd-`, Bardet `user.bardet-`), eigene Patterns werden dabei nicht überschrieben. Die Bausteine lassen sich danach wie eigene Patterns bearbeiten, in Songs verwenden und in den Papierkorb legen.

`.tab`-Dateien werden vorher übersetzt, immer in einen leeren Zwischenordner, siehe [CONTRIBUTING.md](../CONTRIBUTING.md).

Jeder Satz wird bei jedem Pull Request automatisch geprüft (`tools/check-community.sh`).
