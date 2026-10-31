# Analysiert Hydrogen-Dateien (.h2song/.h2pattern) nur lesend; Aufruf: python3 -I h2scan.py <datei|ordner> [--detail]
import sys, os, collections
import xml.etree.ElementTree as ET
from fractions import Fraction

def load(path):
    raw = open(path, "rb").read()
    if b"<!ENTITY" in raw or b"<!DOCTYPE" in raw:
        raise ValueError("DTD/ENTITY abgelehnt")
    return ET.fromstring(raw)

def txt(e, tag, default=""):
    x = e.find(tag)
    return x.text if x is not None and x.text is not None else default

def scan(path):
    root = load(path)
    insts = {}
    for ins in root.iter("instrument"):
        i = ins.find("id")
        if i is not None and ins.find("name") is not None:
            insts[i.text] = txt(ins, "name")
    pats = []
    for p in root.iter("pattern"):
        size = int(txt(p, "size", "192")); den = int(txt(p, "denominator", "4") or 4)
        notes = []
        for n in p.iter("note"):
            notes.append((int(txt(n, "position", "0")), insts.get(txt(n, "instrument"), "?" + txt(n, "instrument")), float(txt(n, "velocity", "0.8"))))
        pats.append(dict(name=txt(p, "name"), size=size, den=den, notes=notes))
    return dict(bpm=txt(root, "bpm"), insts=insts, pats=pats)

paths = []
a = sys.argv[1]
if os.path.isdir(a):
    for dp, dn, fn in os.walk(a):
        paths += [os.path.join(dp, f) for f in fn if f.endswith((".h2song", ".h2pattern")) and ".autosave." not in f]
else:
    paths = [a]
tot = 0; sizes = collections.Counter(); used = collections.Counter(); grid = collections.Counter(); empty = 0
for pth in sorted(paths):
    try:
        s = scan(pth)
    except Exception as e:
        print("FEHLER", pth, e); continue
    nonempty = [p for p in s["pats"] if p["notes"]]
    empty += len(s["pats"]) - len(nonempty)
    tot += len(nonempty)
    for p in nonempty:
        sizes[(p["size"], p["den"])] += 1
        for pos, ins, v in p["notes"]:
            used[ins] += 1
            grid[Fraction(pos, 48).limit_denominator(48).denominator] += 1
    if "--detail" in sys.argv:
        print(os.path.basename(pth), "bpm", s["bpm"], "pattern", len(nonempty), [p["name"] for p in nonempty][:5])
print("Dateien", len(paths), "Patterns mit Noten", tot, "leer", empty)
print("Größe (ticks, nenner)", sizes.most_common(10))
print("Instrumente", used.most_common(30))
print("Nenner der Positionen in Vierteln (48 ticks/Viertel)", sorted(grid.items()))
