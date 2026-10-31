#!/usr/bin/env python3
"""MIDI-Schlagzeugdateien prüfen – nur Standardbibliothek, liest, schreibt nichts in die Quellen.

Aufruf (immer mit python3 -I):
  midiscan.py stats <ordner> [--map gm|gmd] [--json ausgabe.json]
      Anzahl Dateien, Takte, Taktarten, Tempi, PPQ, Kanäle, Noten-Histogramm,
      Anteil exakt quantisierter Schläge (auf 1/16- oder 1/12-Raster).
  midiscan.py bar <datei.mid> [--bar 1] [--map gm|gmd]
      Dekodiert einen Takt als Positionen in Vierteln je HZ-Stimme
      (eingerastet auf 1/16 bzw. 1/12, Abweichung als lag in ms).
  midiscan.py dump <datei.mid> [--n 30]
      Kopf, Meta-Events, erste Noten roh.
"""
import os
import struct
import sys
import json
from collections import Counter, defaultdict
from fractions import Fraction

GM_NAMES = {
    22: "HH closed edge (TD-11)", 26: "HH open edge (TD-11)",
    35: "Acoustic Bass Drum", 36: "Bass Drum 1", 37: "Side Stick", 38: "Acoustic Snare",
    39: "Hand Clap", 40: "Electric Snare", 41: "Low Floor Tom", 42: "Closed Hi-Hat",
    43: "High Floor Tom", 44: "Pedal Hi-Hat", 45: "Low Tom", 46: "Open Hi-Hat",
    47: "Low-Mid Tom", 48: "Hi-Mid Tom", 49: "Crash 1", 50: "High Tom", 51: "Ride 1",
    52: "Chinese Cymbal", 53: "Ride Bell", 54: "Tambourine", 55: "Splash", 56: "Cowbell",
    57: "Crash 2", 58: "Vibraslap", 59: "Ride 2", 60: "Hi Bongo", 61: "Low Bongo",
    62: "Mute Hi Conga", 63: "Open Hi Conga", 64: "Low Conga", 65: "High Timbale",
    66: "Low Timbale", 67: "High Agogo", 68: "Low Agogo", 69: "Cabasa", 70: "Maracas",
    71: "Short Whistle", 72: "Long Whistle", 73: "Short Guiro", 74: "Long Guiro",
    75: "Claves", 76: "Hi Wood Block", 77: "Low Wood Block", 78: "Mute Cuica",
    79: "Open Cuica", 80: "Mute Triangle", 81: "Open Triangle",
}

# General MIDI (Kanal 10) -> HZ-Stimme; wie tools/gpdrums/gp2json.py, ergänzt um 22/26 nicht.
MAP_GM = {
    35: "kick", 36: "kick", 37: "snare.side", 38: "snare", 40: "snare",
    41: "tom4", 43: "tom3", 45: "tom2", 47: "tom2", 48: "tom1", 50: "tom1",
    42: "hihat", 44: "hihat.pedal", 46: "hihat.open",
    49: "crash1", 57: "crash2", 52: "china", 55: "splash",
    51: "ride", 59: "ride", 53: "ride.bell", 56: "cowbell",
}
# Groove MIDI Dataset / E-GMD (Roland TD-11, siehe magenta.tensorflow.org/datasets/groove):
# 40 = Snare-Rand (Rimshot), 22/26 = HiHat-Rand, 55/52 = Crash-1/-2-Rand (KEIN Splash/China),
# 58 = Tom-3-Rand, 59 = Ride-Rand.
MAP_GMD = {
    36: "kick", 38: "snare", 40: "snare.rim", 37: "snare.side",
    48: "tom1", 50: "tom1", 45: "tom2", 47: "tom2", 43: "tom3", 58: "tom3",
    46: "hihat.open", 26: "hihat.open", 42: "hihat", 22: "hihat", 44: "hihat.pedal",
    49: "crash1", 55: "crash1", 57: "crash2", 52: "crash2",
    51: "ride", 59: "ride", 53: "ride.bell",
}
# Groove Monkee „MegaReaper SM Drums“ (laut beiliegender Reaper-Notennamen-Datei)
MAP_SM = {
    36: "kick", 38: "snare", 40: "snare", 39: "snare.rim",
    42: "hihat", 43: "hihat.half", 44: "hihat.pedal", 45: "hihat.pedal", 46: "hihat.open",
    47: "tom1", 48: "tom2", 50: "tom3",
    49: "crash1", 57: "crash2", 51: "ride", 59: "ride", 53: "ride.bell", 60: "ride.bell",
}
MAPS = {"gm": MAP_GM, "gmd": MAP_GMD, "sm": MAP_SM}
FEET = {"kick", "hihat.pedal"}


class MidiError(Exception):
    pass


def _varlen(d, i):
    v = 0
    for _ in range(4):
        if i >= len(d):
            raise MidiError("VLQ über Spurende")
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i
    raise MidiError("VLQ zu lang")


def read_smf(path):
    """path: Dateipfad oder bereits gelesene bytes."""
    if isinstance(path, (bytes, bytearray)):
        data = bytes(path)
    else:
        with open(path, "rb") as f:
            data = f.read()
    if data[:4] == b"RIFF" and data[8:12] == b"RMID":     # RIFF-MIDI-Hülle
        k = data.find(b"MThd")
        if k < 0:
            raise MidiError("RMID ohne MThd")
        data = data[k:]
    if data[:4] != b"MThd":
        raise MidiError("kein MThd")
    if len(data) < 14:
        raise MidiError("Kopf unvollständig")
    hlen = struct.unpack(">I", data[4:8])[0]
    fmt, ntrk, div = struct.unpack(">HHH", data[8:14])
    if div & 0x8000:
        raise MidiError("SMPTE-Zeitbasis nicht unterstützt")
    if div == 0:
        raise MidiError("Zeitbasis 0")
    i = 8 + hlen
    tracks = []
    while i + 8 <= len(data):
        cid, clen = data[i:i + 4], struct.unpack(">I", data[i + 4:i + 8])[0]
        body = data[i + 8:i + 8 + clen]
        i += 8 + clen
        if cid != b"MTrk":
            continue
        tracks.append(_parse_track(body))
    return {"format": fmt, "ntracks": ntrk, "ppq": div, "tracks": tracks}


def _parse_track(d):
    try:
        return _parse_events(d)
    except IndexError:
        raise MidiError("Spur endet mitten in einem Ereignis") from None


def _parse_events(d):
    # Running status survives meta and sysex events: wrong by the standard, but some files rely on it, and a
    # correct file always has a status byte there, so it is read the same.
    ev = []
    i, tick, status = 0, 0, None
    while i < len(d):
        delta, i = _varlen(d, i)
        tick += delta
        if i >= len(d):
            break
        b = d[i]
        if b == 0xFF:
            mtype = d[i + 1]
            ln, i = _varlen(d, i + 2)
            payload = d[i:i + ln]
            i += ln
            if mtype == 0x51 and ln == 3:
                us = (payload[0] << 16) | (payload[1] << 8) | payload[2]
                ev.append((tick, "tempo", us) if us > 0 else (tick, "ignored", "Tempo 0"))   # nothing playable
            elif mtype == 0x58 and ln >= 2:   # 0/x or beyond 1/64: ignored
                ev.append((tick, "timesig", (payload[0], 2 ** payload[1])) if payload[0] > 0 and payload[1] <= 6
                          else (tick, "ignored", f"Taktart {payload[0]}/2^{payload[1]}"))
            elif mtype in (0x01, 0x03, 0x06):
                ev.append((tick, {1: "text", 3: "name", 6: "marker"}[mtype],
                           payload.decode("latin-1", "replace")))
            elif mtype == 0x2F:
                ev.append((tick, "end", None))
                break
            continue
        if b in (0xF0, 0xF7):
            ln, i = _varlen(d, i + 1)
            i += ln
            continue
        if b & 0x80:
            status = b
            i += 1
        elif status is None:
            raise MidiError("Datenbyte ohne Status")
        hi, ch = status & 0xF0, status & 0x0F
        if hi in (0xC0, 0xD0):
            a = d[i]
            i += 1
            if hi == 0xC0:
                ev.append((tick, "program", (ch, a)))
            continue
        if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
            a, v = d[i], d[i + 1]
            i += 2
            if hi == 0x90 and v > 0:
                ev.append((tick, "on", (ch, a, v)))
            elif hi == 0xB0:
                ev.append((tick, "cc", (ch, a, v)))
            continue
        raise MidiError("unbekannter Status %02x" % status)
    return ev


def analyse(path):
    smf = read_smf(path)
    notes, tempos, sigs, names, ccs, markers, ignored = [], [], [], [], Counter(), [], Counter()
    end = offset = 0
    for tr in smf["tracks"]:
        for tick, kind, val in tr:
            tick += offset
            end = max(end, tick)
            if kind == "on":
                notes.append((tick,) + val)
            elif kind == "tempo":
                tempos.append((tick, val))
            elif kind == "timesig":
                sigs.append((tick, val))
            elif kind == "name":
                names.append(val)
            elif kind == "cc":
                ccs[val[1]] += 1
            elif kind == "marker":
                markers.append((tick, val))
            elif kind == "ignored":
                ignored[val] += 1
        if smf["format"] == 2:   # independent patterns: one after the other
            offset = end
    notes.sort()
    tempos.sort()
    sigs.sort()
    # gleiche Taktart doppelt (oft eine je Spur) bzw. mehrere am selben Tick: nur echte Wechsel behalten
    dedup = []
    for t, s in sigs:
        if dedup and dedup[-1][0] == t:
            dedup[-1] = (t, s)
        elif not dedup or dedup[-1][1] != s:
            dedup.append((t, s))
    sigs = dedup
    if not sigs or sigs[0][0] > 0:
        sigs.insert(0, (0, (4, 4)))
    if not tempos or tempos[0][0] > 0:
        tempos.insert(0, (0, 500000))
    return {"ppq": smf["ppq"], "format": smf["format"], "notes": notes, "tempos": tempos,
            "sigs": sigs, "names": names, "ccs": ccs, "end": end, "markers": markers, "ignored": ignored}


def bars_of(a, upto_tick):
    """Liste (start_tick, länge_ticks, zähler, nenner) bis upto_tick (inklusive)."""
    ppq = a["ppq"]
    out = []
    sigs = a["sigs"]
    for k, (t0, (num, den)) in enumerate(sigs):
        t_end = sigs[k + 1][0] if k + 1 < len(sigs) else None
        blen = Fraction(ppq * 4 * num, den)
        t = Fraction(t0)
        while (t_end is None and t <= upto_tick) or (t_end is not None and t < t_end):
            bar = (t, blen, num, den)
            if t_end is not None and t + blen > t_end:   # the next meter starts inside this bar: it ends there
                beats, d = (t_end - t) * den / (ppq * 4), den   # in beats of this meter, finer if needed (2/4, 6/16)
                while beats.denominator != 1 and d < 32:
                    beats, d = beats * 2, d * 2
                beats = max(1, round(beats))   # off the 32nd grid: to the nearest 32nd
                bar = (t, Fraction(beats * ppq * 4, d), beats, d)
            out.append(bar)
            t += blen
        if t_end is None:
            break
    return out


def tempo_at(a, tick):
    us = a["tempos"][0][1]
    for t, v in a["tempos"]:
        if t <= tick:
            us = v
    return us


def snap(q):
    """q = Position in Vierteln (Fraction) -> nächster Punkt auf 1/16- oder 1/12-Raster."""
    c16 = Fraction(round(q * 4), 4)
    c12 = Fraction(round(q * 3), 3)
    return c16 if abs(c16 - q) <= abs(c12 - q) else c12


def cmd_dump(path, n=30):
    a = analyse(path)
    print("Datei:", path)
    print("Format", a["format"], "PPQ", a["ppq"], "Spurnamen", a["names"])
    print("Tempi:", [(t, round(60e6 / v, 2)) for t, v in a["tempos"]][:10])
    print("Taktarten:", a["sigs"][:10])
    print("CC-Nummern:", dict(a["ccs"]))
    for t, ch, note, vel in a["notes"][:n]:
        print("  tick %6d  ch %2d  note %3d %-22s vel %3d" % (t, ch + 1, note, GM_NAMES.get(note, "?"), vel))


def cmd_bar(path, barno=1, mapname="gm"):
    a = analyse(path)
    m = MAPS[mapname]
    ppq = a["ppq"]
    last = a["notes"][-1][0] if a["notes"] else 0
    bars = bars_of(a, last)
    if barno > len(bars):
        print("Datei hat nur", len(bars), "Takte")
        return
    t0, blen, num, den = bars[barno - 1]
    us = tempo_at(a, t0)
    ms_per_tick = us / ppq / 1000.0
    # Fenster: halber 1/16-Schritt vor dem Taktanfang gehört schon zu diesem Takt
    lead = Fraction(ppq, 8)
    rows = defaultdict(list)
    unmapped = Counter()
    for t, ch, note, vel in a["notes"]:
        if t < t0 - lead or t >= t0 + blen - lead:
            continue
        voice = m.get(note)
        if voice is None:
            unmapped[note] += 1
            continue
        q = Fraction(t - t0, 1) / ppq
        p = snap(q)
        lag = float((q - p) * ppq) * ms_per_tick
        rows[voice].append((p, vel, lag))
    print("Datei:", os.path.basename(path))
    print("Takt %d: Taktart %d/%d, Tempo %.1f BPM, PPQ %d, Mapping %s" % (
        barno, num, den, 60e6 / us, ppq, mapname))
    order = ["crash1", "crash2", "china", "splash", "ride", "ride.bell", "hihat", "hihat.half",
             "hihat.open", "snare", "snare.rim", "snare.side", "tom1", "tom2", "tom3", "tom4",
             "cowbell", "kick", "hihat.pedal"]
    for v in order + sorted(set(rows) - set(order)):
        if v not in rows:
            continue
        cells = []
        for p, vel, lag in sorted(rows[v]):
            cells.append("%s(v%.2f%s)" % (p, vel / 127.0, ", lag %+.0fms" % lag if abs(lag) >= 1 else ""))
        print("  %-12s %s" % (v + (" [Fuß]" if v in FEET else ""), "  ".join(cells)))
    if unmapped:
        print("  nicht zugeordnet:", {"%d %s" % (k, GM_NAMES.get(k, "?")): c for k, c in unmapped.items()})


def iter_midis(root):
    for dp, dn, fn in os.walk(root):
        dn.sort()
        for f in sorted(fn):
            if f.lower().endswith((".mid", ".midi", ".smf", ".kar")):
                yield os.path.join(dp, f)


def cmd_stats(root, mapname="gm", out=None):
    m = MAPS[mapname]
    files = list(iter_midis(root))
    tot_bars = 0
    bars_hist = Counter()
    sig_hist = Counter()
    ppq_hist = Counter()
    ch_hist = Counter()
    note_hist = Counter()
    tempo_vals = []
    exact = total = 0
    errors = []
    sig_changes = 0
    tempo_changes = 0
    for p in files:
        try:
            a = analyse(p)
        except (MidiError, IndexError, struct.error) as e:
            errors.append((p, str(e)))
            continue
        ppq_hist[a["ppq"]] += 1
        if not a["notes"]:
            bars_hist[0] += 1
            continue
        last = a["notes"][-1][0]
        bars = bars_of(a, last)
        nb = len(bars)
        tot_bars += nb
        bars_hist[nb if nb <= 16 else ">16"] += 1
        for s in sorted({"%d/%d" % s for _, s in a["sigs"]}):
            sig_hist[s] += 1          # Dateien je Taktart
        sig_changes += len(a["sigs"]) > 1
        tempo_changes += len(set(v for _, v in a["tempos"])) > 1
        tempo_vals.append(round(60e6 / a["tempos"][0][1], 1))
        ppq = a["ppq"]
        for t, ch, note, vel in a["notes"]:
            ch_hist[ch + 1] += 1
            note_hist[note] += 1
            total += 1
            # exakt auf 1/16- oder 1/12-Raster? (ab Dateianfang)
            if (t * 4) % ppq == 0 or (t * 3) % ppq == 0:
                exact += 1
    res = {
        "ordner": root, "dateien": len(files), "fehler": len(errors), "takte_gesamt": tot_bars,
        "takte_je_datei": {str(k): v for k, v in sorted(bars_hist.items(), key=lambda kv: (isinstance(kv[0], str), kv[0]))},
        "taktarten": dict(sig_hist.most_common()), "dateien_mit_taktwechsel": sig_changes,
        "dateien_mit_tempowechsel": tempo_changes,
        "ppq": dict(ppq_hist), "kanaele": dict(sorted(ch_hist.items())),
        "tempo_min_max": [min(tempo_vals), max(tempo_vals)] if tempo_vals else None,
        "schlaege": total, "anteil_exakt_auf_1_16_oder_1_12": round(exact / total, 3) if total else None,
        "noten": {"%d %s -> %s" % (k, GM_NAMES.get(k, "?"), m.get(k, "-")): v for k, v in sorted(note_hist.items())},
        "anteil_zugeordnet": round(sum(v for k, v in note_hist.items() if k in m) / total, 4) if total else None,
        "fehlerliste": errors[:20],
    }
    txt = json.dumps(res, ensure_ascii=False, indent=1)
    if out:
        with open(out, "w", encoding="utf-8") as f:
            f.write(txt + "\n")
    print(txt)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    cmd, target = argv[1], argv[2]
    opts = dict(zip(argv[3::2], argv[4::2]))
    if cmd == "stats":
        cmd_stats(target, opts.get("--map", "gm"), opts.get("--json"))
    elif cmd == "bar":
        cmd_bar(target, int(opts.get("--bar", 1)), opts.get("--map", "gm"))
    elif cmd == "dump":
        cmd_dump(target, int(opts.get("--n", 30)))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
