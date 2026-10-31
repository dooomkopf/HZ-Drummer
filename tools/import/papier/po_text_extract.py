"""Pocket Operations (PDF mit Textebene) -> Patterns, nur aus der Textebene (pdftotext -bbox).

Jeder Treffer ist im Heft eine schwarze Zelle mit ihrer Schrittnummer; der Schritt wird aus der
x-Position des Wortes bestimmt (nicht aus der gedruckten Zahl), Abweichungen werden gemeldet.
Aufruf: python3 -I po_text_extract.py <bbox.html> <ausgabe.json>
(bbox.html vorher erzeugen: pdftotext -bbox <pdf> <bbox.html>)
"""
import html
import json
import re
import sys

INST = {'AC', 'BD', 'SN', 'LT', 'MT', 'HT', 'CL', 'SH', 'RS', 'CB', 'CY', 'OH', 'CH', 'HC'}
WORD = re.compile(r'<word xMin="([\d.]+)" yMin="([\d.]+)" xMax="([\d.]+)" yMax="([\d.]+)">(.*?)</word>')


def pages(path):
    txt = open(path, encoding='utf-8').read()
    for i, chunk in enumerate(txt.split('<page ')[1:], start=1):
        ws = []
        for m in WORD.finditer(chunk):
            x0, y0, x1, y1 = map(float, m.groups()[:4])
            ws.append({'x0': x0, 'y0': y0, 'x1': x1, 'y1': y1, 'h': y1 - y0,
                       'xc': (x0 + x1) / 2, 't': html.unescape(m.group(5))})
        yield i, ws


def main():
    src, dst = sys.argv[1], sys.argv[2]
    allp = list(pages(src))
    # Spaltengeometrie aus allen Zahlen in Rasterzeilen (Schriftgroesse ~6) schaetzen
    pts = [(int(w['t']), w['xc']) for _, ws in allp for w in ws
           if w['t'].isdigit() and 5 < w['h'] < 6.5 and w['x0'] > 58 and 1 <= int(w['t']) <= 16]
    n = len(pts)
    sx = sum(v for v, _ in pts); sy = sum(x for _, x in pts)
    sxx = sum(v * v for v, _ in pts); sxy = sum(v * x for v, x in pts)
    w = (n * sxy - sx * sy) / (n * sxx - sx * sx)
    x0 = (sy - w * sx) / n
    col = lambda xc: int(round((xc - x0) / w))  # x = x0 + w * Schritt
    out, warn_total = [], 0
    for pno, ws in allp:
        heads = [x for x in ws if 7.5 < x['h'] < 11]          # Titelzeilen
        grid = [x for x in ws if x['h'] <= 7.5]               # Rasterschrift
        section = ' '.join(x['t'] for x in ws if x['h'] >= 11)
        # Titel zeilenweise zusammenfassen
        lines = []
        for x in sorted(heads, key=lambda a: a['y0']):
            if lines and x['y0'] - lines[-1][0] < 1.5:
                lines[-1][1].append(x)
            else:
                lines.append([x['y0'], [x]])
        titles = [(y, ' '.join(t['t'] for t in sorted(v, key=lambda a: a['x0'])))
                  for y, v in lines]
        titles = [(y, t) for y, t in titles if not t.isdigit()]
        rows = {}
        for x in grid:
            rows.setdefault(round(x['y0'] * 2) / 2, []).append(x)
        # benachbarte y-Werte (+-1) zusammenlegen
        keys = sorted(rows)
        merged = []
        for k in keys:
            if merged and k - merged[-1][0] <= 1.0:
                merged[-1][1].extend(rows[k])
            else:
                merged.append([k, list(rows[k])])
        pats = {}
        for y, items in merged:
            items.sort(key=lambda a: a['x0'])
            label = items[0]['t'] if items[0]['x0'] < 58 and items[0]['t'] in INST else None
            cells = [a for a in items if a['x0'] >= 58]
            if label is None and not all(a['t'] in ('^', '"', '“', '”') for a in cells):
                continue
            if label is None:
                label = 'AC'
            owner = [t for t in titles if t[0] < y]
            if not owner:
                continue
            ty, tname = owner[-1]
            p = pats.setdefault((ty, tname), {'name': tname, 'pdf_page': pno, 'section': section,
                                              'rows': {}, 'warnings': []})
            for a in cells:
                c = col(a['xc'])
                if label == 'AC':
                    if a['t'] == '^':
                        p.setdefault('accents', []).append(c)
                    continue
                if a['t'].isdigit():
                    if int(a['t']) != c:
                        p['warnings'].append(f"{label}: Ziffer {a['t']} in Spalte {c}")
                        warn_total += 1
                    p['rows'].setdefault(label, []).append(c)
                elif a['t'] in ('"', '“', '”'):
                    p['rows'].setdefault(label, []).append(c)
                    p.setdefault('ratchets', []).append(c)
                else:
                    p['warnings'].append(f"{label}: unbekannt {a['t']!r} Spalte {c}")
        for (ty, _), p in sorted(pats.items()):
            if p['rows']:
                out.append(p)
    json.dump({'geometrie': {'x0': x0, 'w': w, 'n_ziffern': n}, 'patterns': out},
              open(dst, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(f'{len(out)} Patterns, Spalte1={x0 + w:.2f} Breite={w:.3f}, Ziffer/Spalte-Abweichungen={warn_total}')


if __name__ == '__main__':
    main()
