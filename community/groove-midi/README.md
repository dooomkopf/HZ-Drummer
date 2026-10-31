# Groove MIDI Dataset (Google Magenta)

892 Bausteine aus echten Einspielungen von zehn Schlagzeugern (Roland TD-11), in Stilgruppen von Rock und Funk
bis Latin, Jazz und Afro, dazu 21 Arrangements aus langen Einspielungen. Das Timing bleibt, wie gespielt: Jeder
Schlag steht auf dem passenden Rasterplatz, die Abweichung als Versatz in ms.

- **Quelle:** Groove MIDI Dataset v1.0.0, Google Magenta (Jon Gillick, Adam Roberts, Jesse Engel, Douglas Eck,
  David Bamman: „Learning to Groove with Inverse Sequence Transformations“, ICML 2019),
  <https://magenta.tensorflow.org/datasets/groove>
- **Lizenz:** [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Namensnennung: „Groove MIDI Dataset,
  Google Magenta“.
- **Geändert:** in Ausschnitte zu 8 Takten zerlegt (die abwechslungsreichsten Einspielungen ganz, als
  Arrangement), auf Raster gesetzt, ins Pattern-Format von HZ-Drummer übertragen. Jeder Baustein nennt seine
  Einspielung und die Takte im Feld `origin`.
- **Umwandler:** [tools/import/midi/gmd_import.py](../../tools/import/midi/gmd_import.py)

Die Arrangements liegen in `songs/`, Kennungen `user.gmd-…`.
