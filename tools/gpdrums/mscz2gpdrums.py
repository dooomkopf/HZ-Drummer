#!/usr/bin/env python3
"""Liest die Schlagzeug-Notenzeile einer MuseScore-Datei (.mscz = Zip, .mscx = XML) als Textzeilen wie GpDrums.java
(TRACK/BAR/MARKER/HIT, 960 Ticks je Viertel), damit gp2json.py sie wie eine Guitar-Pro-Datei weiterverarbeitet.

Aufruf (immer mit python3 -I):
  mscz2gpdrums.py <datei.mscz|.mscx>              die erste Schlagzeug-Notenzeile
  mscz2gpdrums.py <datei.mscz|.mscx> --track T     Notenzeile T (Nummer aus den TRACK-Zeilen)

- MuseScore 3 und 4 (bis Dateiformat 5.00); 1.x und 2.x, soweit ihre Positionsangaben (<tick>) reichen.
- Takte, Taktarten (auch verkürzte Takte), Tempo am Taktanfang, Wiederholungen mit Anzahl, Volten (alternative
  Enden), Probenzeichen als MARKER, N-tolen, Tremolo-Wirbel als Einzelschläge, Ein-Takt-Wiederholungszeichen.
- Tonhöhe = General-MIDI-Schlagzeugnote; liegt sie außerhalb von GM (27–87), gilt der Name aus dem Schlagzeug-Set der
  Datei ("Snare", "Kick" …).
- Lautstärke: die eigene Velocity der Note, sonst die Dynamik an ihrer Stelle, gleich in welcher Stimme sie steht
  (mf = 80); Akzent, Marcato, sforzato = Akzent.
- Steht das Schlagzeug nicht oben: Tempo, Probenzeichen, Wiederholungen und Volten aus der obersten Notenzeile
  (MuseScore schreibt sie nur dorthin).
- Was nicht ausgewertet wird, steht als Zeile "HINWEIS: …" am Ende (Sprünge D.S./D.C./Coda, Vorschlagsnoten,
  Swing, Gabeln, Fermaten, Wiederholungszeichen über mehrere Takte). Defekte Datei: eine Zeile auf stderr, Ausgang 1.
Nur Standardbibliothek, liest nur.
"""
import os
import re
import sys
import zipfile
import xml.etree.ElementTree as ET
from collections import Counter
from fractions import Fraction as F

TPQ = 960
LENGTH = {"long": F(4), "breve": F(2), "whole": F(1), "half": F(1, 2), "quarter": F(1, 4), "eighth": F(1, 8),
          "16th": F(1, 16), "32nd": F(1, 32), "64th": F(1, 64), "128th": F(1, 128), "256th": F(1, 256),
          "512th": F(1, 512), "1024th": F(1, 1024)}
DYNAMIC = {"pppppp": 1, "ppppp": 5, "pppp": 10, "ppp": 16, "pp": 33, "p": 49, "mp": 64, "mf": 80, "f": 96, "ff": 112,
           "fff": 126, "ffff": 127, "fffff": 127, "ffffff": 127, "fp": 96, "sf": 112, "sfz": 112, "sff": 126, "sffz": 126,
           "sfp": 112, "sfpp": 112, "rfz": 112, "rf": 112, "fz": 112}
GRACE = ("acciaccatura", "appoggiatura", "grace4", "grace16", "grace32", "grace8after", "grace16after", "grace32after")
NOT_COVERED = {"Jump": "Sprünge (D.S., D.C., Coda) nicht ausgewertet, der Ablauf folgt den Takten",
               "Marker": "Sprungmarken (Segno, Coda, Fine) nicht ausgewertet",
               "HairPin": "Crescendo-/Decrescendo-Gabeln nicht übernommen",
               "Fermata": "Fermaten nicht übernommen, die Takte behalten ihre Länge",
               "swing": "Swing-Angabe nicht übernommen, die Schläge stehen gerade"}


def gm_by_name(name):
    """General-MIDI-Note zu einem Schlagzeug-Namen aus der Datei, None wenn unklar."""
    n = name.lower()
    rules = [(("cowbell",), 56), (("kick", "bass drum", "bassdrum"), 36), (("side stick", "cross stick", "rim click", "stick"), 37),
             (("snare",), 38), (("pedal",), 44), (("open hi", "open hat", "hi-hat open", "hihat open"), 46),
             (("hat", "hh"), 42), (("bell",), 53), (("ride",), 51), (("china",), 52), (("splash",), 55), (("crash",), 49),
             (("floor",), 41), (("low tom", "tom 4", "tom4"), 43), (("mid tom", "tom 3", "tom3", "tom 2", "tom2"), 47),
             (("high tom", "hi tom", "tom 1", "tom1", "tom"), 50)]
    for words, note in rules:
        if any(w in n for w in words):
            return 57 if note == 49 and "2" in n else note
    return None


def load(path):
    """Die Partitur als XML-Wurzel: aus dem Zip die Hauptdatei (META-INF/container.xml), sonst die Datei selbst."""
    if not zipfile.is_zipfile(path):
        return ET.parse(path).getroot()
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        main = None
        if "META-INF/container.xml" in names:
            main = next((rf.get("full-path") for rf in ET.fromstring(z.read("META-INF/container.xml")).iter("rootfile")
                         if (rf.get("full-path") or "").endswith(".mscx") and rf.get("full-path") in names), None)
        main = main or next((n for n in names if n.endswith(".mscx") and not n.startswith("Excerpts/")), None)
        if main is None:
            raise ValueError("keine .mscx-Datei im Archiv")
        if z.getinfo(main).file_size > 64 << 20:
            raise ValueError("Partitur zu groß")
        return ET.fromstring(z.read(main))


def text_of(el):
    return " ".join("".join(el.itertext()).split()) if el is not None else ""


def staves_of(score):
    """[(Index, Staff-id, Name, Schlagzeug ja/nein, {Tonhöhe: Name})] je Notenzeile, in der Reihenfolge der Datei."""
    out = []
    for part in score.findall("Part"):
        inst = part.find("Instrument")
        drums, perc_inst = {}, False
        if inst is not None:
            drums = {int(d.get("pitch")): d.findtext("name") or "" for d in inst.findall("Drum") if (d.get("pitch") or "").isdigit()}
            perc_inst = inst.findtext("useDrumset") == "1" or bool(drums)
        name = part.findtext("trackName") or text_of(part.find("Instrument/longName")) or text_of(part.find("Instrument/InstrumentLabel/longName"))
        for sd in part.findall("Staff"):
            st = sd.find("StaffType")
            perc = perc_inst or (st is not None and (st.get("group") == "percussion" or st.findtext("group") == "percussion"))
            out.append((len(out), sd.get("id") or str(len(out) + 1), " ".join(name.split()) or f"Zeile {len(out) + 1}", perc, drums))
    return out


class StaffReader:
    """Liest die Takte einer Notenzeile; MuseScore 3+ schreibt je Stimme ein <voice>, 1.x/2.x Positionen mit <tick>."""

    def __init__(self, drums, division, velocity_offset=False):
        self.drums, self.division = drums, division
        self.velocity_offset = velocity_offset   # MuseScore before 4: <velocity> without <veloType> is an offset in %
        self.tempo_seen = False                  # a tempo mark in this staff (else the top staff's count)
        self.num, self.den, self.tempo, self.dyn = 4, 4, 120.0, 80
        self.tuplet_defs, self.tie_ids, self.volta_start, self.alt = {}, set(), {}, Counter()
        self.mstart = 0     # start of the measure in Division ticks (positions of MuseScore 1.x/2.x)
        self.by_name = {}   # pitch -> GM note found by its drum name
        self.seen = Counter()
        self.bars = []

    def read(self, measures, score):
        for nr, m in enumerate(measures, 1):
            self.measure(nr, m)
        for v in score.findall("Volta"):   # MuseScore 1.x: voltas in the score, with start and end tick
            starts, at = [], 0
            for b in self.bars:
                starts.append(at)
                at += int(b["length"] * 4 * self.division)
            inside = [k + 1 for k, s in enumerate(starts) if int(v.findtext("tick") or -1) <= s < int(v.findtext("tick2") or -1)]
            if inside:
                self.volta(v, inside[0], inside[-1])
        for b in self.bars:
            b["alt"] = self.alt[b["nr"]]
        return self.bars

    def measure(self, nr, m):
        ts = next(m.iter("TimeSig"), None)
        if ts is not None:
            self.num = int(ts.findtext("sigN") or ts.findtext("nom1") or self.num)
            self.den = int(ts.findtext("sigD") or ts.findtext("den") or self.den)
        length = F(m.get("len")) if m.get("len") else F(self.num, self.den)
        num, den = self.num, self.den
        if length != F(num, den):   # a shortened or longer bar (pickup): its own meter
            num, den = (length * den).numerator, (length * den).denominator * den
        self.bar = dict(nr=nr, length=length, num=num, den=den, tempo=self.tempo, open=m.find("startRepeat") is not None,
                        close=max(0, int(m.findtext("endRepeat") or 2) - 1) if m.find("endRepeat") is not None else 0,
                        marker="", hits=[])
        self.nr, self.length, self.tempos = nr, length, []
        self.dyns, self.pending = [], []   # dynamics with their place, hits waiting for the loudness at theirs
        for voice in m.findall("voice") or [m]:
            self.walk(voice, F(0), [])
        # the loudness of a hit: the last dynamic up to its place, in whichever voice it stands (MuseScore: per staff)
        dyns = sorted(self.dyns, key=lambda d: d[0])
        for pos, gm, spec, ghost, acc in self.pending:
            dyn = self.dyn
            for at, level in dyns:
                if at <= pos:
                    dyn = level
            vel = dyn if spec is None else dyn * (100 + spec[0]) // 100 if spec[1] else spec[0]
            self.bar["hits"].append((round(pos * 4 * TPQ), gm, max(1, min(127, vel)), ghost, acc))
        if dyns:
            self.dyn = dyns[-1][1]
        if self.tempos:
            at_start = [t for pos, t in self.tempos if pos == 0]
            self.bar["tempo"] = at_start[-1] if at_start else self.tempo
            self.tempo = sorted(self.tempos, key=lambda x: x[0])[-1][1]
        self.bars.append(self.bar)
        self.mstart += int(length * 4 * self.division)

    def walk(self, container, cursor, scales):
        for el in container:
            tag = el.tag
            if tag in NOT_COVERED or (tag == "StaffText" and el.find("swing") is not None):
                self.seen[tag if tag != "StaffText" else "swing"] += 1
            if tag == "tick":                # MuseScore 1.x/2.x: absolute position
                cursor = F(int(el.text) - self.mstart, 4 * self.division)
            elif tag == "location":          # MuseScore 3+: a step within the voice
                cursor += F(el.findtext("fractions") or 0)
            elif tag == "Tuplet" and el.get("id") is not None:   # MuseScore 2.x: a definition the chords refer to
                self.tuplet_defs[el.get("id")] = F(int(el.findtext("normalNotes")), int(el.findtext("actualNotes")))
            elif tag == "Tuplet":
                scales = scales + [F(int(el.findtext("normalNotes")), int(el.findtext("actualNotes")))]
            elif tag == "endTuplet":
                scales = scales[:-1]
            elif tag == "Tempo" and el.findtext("tempo"):
                self.tempos.append((cursor, float(el.findtext("tempo")) * 60))
                self.tempo_seen = True
            elif tag == "Dynamic":
                v = int(el.findtext("velocity") or 0)   # 0 or less: the mark's own loudness, as MuseScore plays it
                level = v if v > 0 else DYNAMIC.get(el.findtext("subtype") or "")
                if level:
                    self.dyns.append((cursor, level))   # for every voice, from its place on
            elif tag == "RehearsalMark" and not self.bar["marker"]:
                self.bar["marker"] = text_of(el.find("text")) or text_of(el)
            elif tag == "Spanner" and el.get("type") == "Volta" and el.find("Volta") is not None:
                self.volta(el.find("Volta"), self.nr, self.nr + max(1, int(el.findtext("next/location/measures") or 1)) - 1)
            elif tag == "Volta" and el.get("id") is not None:   # MuseScore 2.x: start, its end is an <endSpanner>
                self.volta_start[el.get("id")] = (self.nr, el)
            elif tag == "endSpanner" and el.get("id") in self.volta_start:
                first, v = self.volta_start.pop(el.get("id"))
                self.volta(v, first, max(first, self.nr - 1 if cursor == 0 else self.nr))
            elif tag in ("RepeatMeasure", "MeasureRepeat"):
                if int(el.findtext("subtype") or 1) == 1 and self.bars:
                    self.bar["hits"] += self.bars[-1]["hits"]
                else:
                    self.seen["MeasureRepeat"] += 1
                cursor += self.length
            elif tag in ("Chord", "Rest"):
                if el.findtext("tick"):      # MuseScore 1.x: the position inside the chord
                    cursor = F(int(el.findtext("tick")) - self.mstart, 4 * self.division)
                if tag == "Chord" and any(el.find(g) is not None for g in GRACE):
                    self.seen["grace"] += 1
                    continue
                scale = F(1)
                for s in scales + ([self.tuplet_defs[el.findtext("Tuplet")]] if el.findtext("Tuplet") in self.tuplet_defs else []):
                    scale *= s
                if el.findtext("durationType") == "measure":
                    dur = F(el.findtext("duration")) if el.findtext("duration") else self.length
                elif el.findtext("durationType") in LENGTH:
                    dur = LENGTH[el.findtext("durationType")] * (2 - F(1, 2 ** int(el.findtext("dots") or 0))) * scale
                else:
                    raise ValueError(f"unbekannter Notenwert {el.findtext('durationType')!r}")
                if tag == "Chord":
                    self.chord(el, cursor, dur, scale)
                cursor += dur
        return cursor

    def chord(self, el, cursor, dur, scale):
        acc = int(any(re.search(r"accent|marcato|sforzato", a.findtext("subtype") or "", re.I) for a in el.iter("Articulation")))
        trem = el.find("TremoloSingleChord") if el.find("TremoloSingleChord") is not None else el.find("Tremolo")
        strokes = [cursor]
        if trem is not None:
            kind = trem.findtext("subtype") or ""
            if re.fullmatch(r"r(8|16|32|64)", kind):   # a roll: single strokes of the tremolo value (a shorter note: one)
                count = dur / (F(1, int(kind[1:])) * scale)
                if count.denominator == 1 and count > 1:
                    strokes = [cursor + k * dur / count for k in range(int(count))]
            else:
                self.seen["tremolo"] += 1
        for note in el.findall("Note"):
            self.tie_ids.update(t.get("id") for t in note.iter("Tie") if t.get("id") is not None)
            tied = (any(s.get("type") == "Tie" and s.find("prev") is not None for s in note.findall("Spanner"))
                    or any(e.get("id") in self.tie_ids for e in note.findall("endSpanner")))
            if tied or note.findtext("play") == "0" or not (note.findtext("pitch") or "").isdigit():
                continue   # the end of a tie sounds no new hit, a muted note does not sound
            pitch = gm = int(note.findtext("pitch"))
            if not 27 <= pitch <= 87 and pitch in self.drums:   # no GM drum note: a sound of its own drum set
                self.by_name.setdefault(pitch, gm_by_name(self.drums[pitch]))
                gm = self.by_name[pitch] or pitch
            v, kind = note.findtext("velocity"), note.findtext("veloType")
            offset = kind == "offset" or (kind is None and self.velocity_offset)
            spec = None if v is None else (int(v), offset)   # its own loudness, absolute or in % of the dynamic
            ghost = int(note.findtext("ghost") == "1" or any("Parenthesis" in (s.findtext("name") or "") for s in note.findall("Symbol")))
            for pos in strokes:
                self.pending.append((pos, gm, spec, ghost, acc))

    def volta(self, v, first, last):
        mask = sum(1 << (int(n) - 1) for n in re.split(r"[,\s]+", v.findtext("endings") or "") if n.isdigit() and 0 < int(n) <= 32)
        for k in range(first, last + 1):
            self.alt[k] |= mask

    def notes(self):
        out = []
        for tag, count in self.seen.items():
            if tag in NOT_COVERED:
                out.append(NOT_COVERED[tag])
            elif tag == "grace":
                out.append(f"{count} Vorschlagsnoten weggelassen")
            elif tag == "tremolo":
                out.append(f"{count} Tremolos ohne Notenwert (Buzz, zwischen zwei Akkorden) als Einzelschlag gelesen")
            elif tag == "MeasureRepeat":
                out.append(f"{count} Wiederholungszeichen über mehrere Takte nicht ausgewertet, die Takte bleiben leer")
        for pitch, gm in sorted(self.by_name.items()):
            if gm:   # one without a match is left to gp2json.py, which reports it with the other unknown notes
                out.append(f"Tonhöhe {pitch} („{self.drums[pitch]}“) als GM-Note {gm} gelesen")
        return out


def merge_top_staff(bars, reader, top, score, division, velocity_offset):
    """What MuseScore writes into the top staff only (tempo, rehearsal marks, repeat signs, voltas, jumps) into the
    drum staff's bars: the top staff read the same way, those taken from it."""
    upper = StaffReader({}, division, velocity_offset)
    try:
        system = upper.read(top.findall("Measure"), score)
    except (ValueError, ZeroDivisionError, TypeError, AttributeError):
        return   # something in the top staff the reader does not know: the drum staff as it is
    for b, s in zip(bars, system):
        b["open"] = b["open"] or s["open"]
        b["close"] = b["close"] or s["close"]
        b["alt"] |= s["alt"]
        b["marker"] = b["marker"] or s["marker"]
        if not reader.tempo_seen:
            b["tempo"] = s["tempo"]
    for tag in ("Jump", "Marker", "swing"):
        reader.seen[tag] += upper.seen[tag]


def main():
    args = sys.argv[1:]
    path = args[0]
    name = os.path.basename(path)
    want = int(args[args.index("--track") + 1]) if "--track" in args else None
    try:
        root = load(path)
    except (OSError, zipfile.BadZipFile, ET.ParseError, ValueError, KeyError) as e:
        sys.exit(f"{name}: nicht lesbar ({e})")
    score = root.find("Score") if root.find("Score") is not None else root   # MuseScore 1.x has no <Score>
    staves = staves_of(score)
    for idx, _, tname, perc, _ in staves:
        print(f"TRACK {idx} {1 if perc else 0} {tname}")
    pick = next((s for s in staves if (s[0] == want if want is not None else s[3])), None)
    data = {s.get("id"): s for s in score.findall("Staff")}
    staff = data.get(pick[1]) if pick is not None else None
    if staff is None:
        print("NODRUMS")
        return
    version = re.match(r"\d+", root.get("version") or "")   # <museScore version="3.02">
    division = int(score.findtext("Division") or root.findtext("Division") or 480)
    offset = version is not None and int(version.group()) < 4
    reader = StaffReader(pick[4], division, offset)
    try:
        bars = reader.read(staff.findall("Measure"), score)
        top = score.find("Staff")
        if top is not None and top is not staff:
            merge_top_staff(bars, reader, top, score, division, offset)
    except (ValueError, ZeroDivisionError, TypeError, AttributeError) as e:
        sys.exit(f"{name}: Aufbau nicht lesbar ({type(e).__name__}: {e})")
    start = 0
    for b in bars:
        ln = int(b["length"] * 4 * TPQ)
        print(f"BAR {b['nr']} {start} {ln} {b['num']} {b['den']} {round(b['tempo'])} {1 if b['open'] else 0} {b['close']} {b['alt']}")
        if b["marker"]:
            print(f"MARKER {b['nr']} {b['marker']}")
        for off, note, vel, ghost, acc in sorted(b["hits"]):
            print(f"HIT {b['nr']} {off} {note} {vel} {ghost} {acc}")
        start += ln
    for what in reader.notes():
        print(f"HINWEIS: {name}: {what}")


if __name__ == "__main__":
    main()
