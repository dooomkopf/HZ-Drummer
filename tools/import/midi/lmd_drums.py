#!/usr/bin/env python3
"""Lakh MIDI clean_midi: Schlagzeugspuren (Kanal 10) herausziehen, ohne das Archiv zu entpacken.

Liest clean_midi.tar.gz als Datenstrom (nichts landet ungeprüft auf der Platte) und schreibt
  <ziel>/lmd_clean_drums.zip   je Song eine MIDI-Datei nur mit Kanal 10 + Tempo/Taktart/Marker
  <ziel>/index.csv             Künstler, Titel, Takte mit Schlagzeug, Taktarten, Tempo, Quantisierung

Aufruf: python3 -I lmd_drums.py <clean_midi.tar.gz> <zielordner> [min_schlaege=64]
"""
import csv
import io
import os
import struct
import sys
import tarfile
import zipfile
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import midiscan as m  # noqa: E402

KEEP_META = {0x01, 0x02, 0x03, 0x06, 0x51, 0x58, 0x59}


def _vlq(v):
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.append((v & 0x7F) | 0x80)
        v >>= 7
    return bytes(reversed(out))


def raw_drum_track(d):
    """(tick, rohbytes) je Event; Kanal-Events nur Kanal 10, Meta nur Tempo/Taktart/Text."""
    out = []
    i = tick = 0
    status = None
    while i < len(d):
        delta, i = m._varlen(d, i)
        tick += delta
        if i >= len(d):
            break
        b = d[i]
        if b == 0xFF:
            mtype = d[i + 1]
            ln, j = m._varlen(d, i + 2)
            raw = d[i:j + ln]
            i = j + ln
            status = None
            if mtype == 0x2F:
                break
            if mtype in KEEP_META:
                out.append((tick, bytes(raw)))
            continue
        if b in (0xF0, 0xF7):
            ln, j = m._varlen(d, i + 1)
            i = j + ln
            status = None
            continue
        if b & 0x80:
            status = b
            i += 1
        elif status is None:
            raise m.MidiError("Datenbyte ohne Status")
        hi, ch = status & 0xF0, status & 0x0F
        n = 1 if hi in (0xC0, 0xD0) else 2
        if i + n > len(d):
            break
        data = d[i:i + n]
        i += n
        if ch == 9:
            out.append((tick, bytes([status]) + bytes(data)))
    return out


def drums_only(data):
    if data[:4] == b"RIFF" and data[8:12] == b"RMID":
        data = data[data.find(b"MThd"):]
    if data[:4] != b"MThd":
        raise m.MidiError("kein MThd")
    hlen = struct.unpack(">I", data[4:8])[0]
    fmt, ntrk, div = struct.unpack(">HHH", data[8:14])
    if div & 0x8000:
        raise m.MidiError("SMPTE")
    i = 8 + hlen
    tracks = []
    while i + 8 <= len(data):
        cid, clen = data[i:i + 4], struct.unpack(">I", data[i + 4:i + 8])[0]
        body = data[i + 8:i + 8 + clen]
        i += 8 + clen
        if cid == b"MTrk":
            ev = raw_drum_track(body)
            if ev:
                tracks.append(ev)
    out = io.BytesIO()
    out.write(b"MThd" + struct.pack(">IHHH", 6, 1, len(tracks), div))
    for ev in tracks:
        buf = io.BytesIO()
        last = 0
        for tick, raw in ev:
            buf.write(_vlq(tick - last) + raw)
            last = tick
        buf.write(b"\x00\xFF\x2F\x00")
        body = buf.getvalue()
        out.write(b"MTrk" + struct.pack(">I", len(body)) + body)
    return out.getvalue()


def clean(name):
    parts = [p for p in name.replace("\\", "/").split("/") if p not in ("", ".", "..")]
    if parts and parts[0] == "clean_midi":
        parts = parts[1:]
    return "/".join(parts)


def main(src, dst, min_hits=64):
    os.makedirs(dst, exist_ok=True)
    zpath = os.path.join(dst, "lmd_clean_drums.zip")
    cpath = os.path.join(dst, "index.csv")
    stats = Counter()
    sig_files = Counter()
    notes_all = Counter()
    with tarfile.open(src, "r|gz") as tf, \
            zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as zf, \
            open(cpath, "w", newline="", encoding="utf-8") as cf:
        w = csv.writer(cf)
        w.writerow(["pfad", "kuenstler", "titel", "schlaege", "takte_mit_schlagzeug", "takte_bis_ende",
                    "taktarten", "tempo_start", "tempowechsel", "anteil_exakt_1_16_1_12", "anteil_gm_zugeordnet"])
        for mem in tf:
            if not mem.isfile() or not mem.name.lower().endswith((".mid", ".midi", ".kar")):
                continue
            stats["dateien"] += 1
            if mem.size > 4_000_000:
                stats["zu_gross"] += 1
                continue
            data = tf.extractfile(mem).read()
            try:
                dd = drums_only(data)
                a = m.analyse(dd)
            except Exception:  # noqa: BLE001  (kaputte Dateien gibt es im LMD reichlich)
                stats["fehler"] += 1
                continue
            hits = [n for n in a["notes"] if n[1] == 9]
            if len(hits) < min_hits:
                stats["ohne_schlagzeug"] += 1
                continue
            ppq = a["ppq"]
            bars = m.bars_of(a, hits[-1][0])
            starts = [b[0] for b in bars]
            used = set()
            k = 0
            for t, *_ in hits:
                while k + 1 < len(starts) and starts[k + 1] <= t:
                    k += 1
                used.add(k)
            exact = sum(1 for t, *_ in hits if (t * 4) % ppq == 0 or (t * 3) % ppq == 0)
            mapped = sum(1 for n in hits if n[2] in m.MAP_GM)
            for n in hits:
                notes_all[n[2]] += 1
            sigs = sorted({"%d/%d" % s for _, s in a["sigs"]})
            for s in sigs:
                sig_files[s] += 1
            rel = clean(mem.name)
            artist, _, title = rel.partition("/")
            zf.writestr(rel, dd)
            w.writerow([rel, artist, os.path.splitext(title)[0], len(hits), len(used), len(bars), " ".join(sigs),
                        round(60e6 / a["tempos"][0][1], 1), len({v for _, v in a["tempos"]}) > 1,
                        round(exact / len(hits), 3), round(mapped / len(hits), 3)])
            stats["mit_schlagzeug"] += 1
            stats["takte_mit_schlagzeug"] += len(used)
    print(dict(stats))
    print("Taktarten (Dateien):", dict(sig_files.most_common(15)))
    tot = sum(notes_all.values())
    print("GM-zugeordnet:", round(sum(v for k, v in notes_all.items() if k in m.MAP_GM) / tot, 4))
    print("Häufigste nicht zugeordnete:", [(k, m.GM_NAMES.get(k, "?"), v) for k, v in notes_all.most_common() if k not in m.MAP_GM][:8])


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 64)
