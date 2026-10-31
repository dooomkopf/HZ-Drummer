# Dekodiert einen Takt aus den MT-Power-Hydrogen-Songs; Instrument-ID + 36 = GM-Note (aus Datei-Namen abgeleitet)
# Aufruf: python3 -I mtpower_decode.py <h2song> <patternname> [takt=1]
import sys, re
from fractions import Fraction
GM = {36: "kick", 38: "snare", 37: "snare.side", 40: "snare", 42: "hihat", 44: "hihat.half", 65: "hihat.pedal", 46: "hihat.open",
      49: "crash1", 57: "crash2", 51: "ride", 53: "ride.bell", 52: "china", 55: "splash",
      48: "tom1", 50: "tom1", 47: "tom2", 45: "tom2", 43: "tom3", 41: "tom4", 56: "cowbell"}
path, want = sys.argv[1], sys.argv[2]; bar = int(sys.argv[3]) if len(sys.argv) > 3 else 1
t = open(path, "rb").read().decode("utf-8")
assert "<!ENTITY" not in t
for m in re.finditer(r"<pattern>\s*<name>([^<]*)</name>(.*?)</pattern>", t, re.S):
    if m.group(1) != want: continue
    body = m.group(2); size = int(re.search(r"<size>(\d+)</size>", body).group(1))
    lo, hi = (bar - 1) * 192, bar * 192
    rows = {}
    for n in re.finditer(r"<note>(.*?)</note>", body, re.S):
        nb = n.group(1)
        pos = int(re.search(r"<position>(\d+)</position>", nb).group(1))
        if not lo <= pos < hi: continue
        iid = int(re.search(r"<instrument>(\d+)</instrument>", nb).group(1))
        vel = float(re.search(r"<velocity>([\d.]+)</velocity>", nb).group(1))
        v = GM.get(iid + 36, f"?GM{iid + 36}")
        rows.setdefault(v, []).append((Fraction(pos - lo, 48), round(vel, 2)))
    print(f"{want} | Größe {size} Ticks = {Fraction(size, 192)} Takte 4/4 | Takt {bar}")
    for v in sorted(rows):
        print(f"  {v:11s}", "  ".join(f"{p}({vel})" for p, vel in sorted(rows[v])))
    break
