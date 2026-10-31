"""Pocket-Operations-Patterns (JSON aus dnvsfn/ableton-pocket-operations) -> HZ-Drummer-Tabs.

Varianten A/B/... mit gleichem Namen werden zu einem mehrtaktigen Pattern verkettet.
Akzente aus der eigenen Textebenen-Extraktion werden ergaenzt (dort fehlt z. B. bei
O-O-H CHILD - B das AC-Etikett, die Pixel-Fassung hat die Akzente nicht).
Aufruf: python3 -I po_to_tab.py <patterns.json> <po_text.json> <ausgabe.tab>
Zuordnung: BD->bd, SN->sn, CL->sn, RS->ss, CH->hh x, OH->hh o, SH->hh g, CY->rd (>=4 Schlaege
im Takt) sonst cr, HT/HC->t1, MT->t2, LT->t3, CB->cb, AC -> Grossbuchstaben.
"""
import json
import re
import sys
import unicodedata

ORDER = ['cr', 'rd', 'hh', 'cb', 't1', 't2', 't3', 'ss', 'sn', 'bd']


def slug(s):
    s = unicodedata.normalize('NFKD', s).encode('ascii', 'ignore').decode().lower()
    return re.sub(r'[^a-z0-9]+', '-', s).strip('-')


def norm(s):
    s = s.upper().replace('↳', '').replace(' OR FOLLOW', ' FOLLOW')
    return re.sub(r'[^A-Z0-9]', '', s)


def bar(p, text_acc):
    rows = {k: [bool(v) for v in a] for k, a in p['rows'].items()}
    acc = [bool(v) for v in p.get('accents', [0] * 16)]
    for s in text_acc:
        acc[s - 1] = True
    tr = {}
    notes = []

    def put(t, i, ch):
        cells = tr.setdefault(t, ['.'] * 16)
        rank = {'.': 0, 'g': 1, 'x': 2, 'o': 3}
        if rank[ch] > rank[cells[i]]:
            cells[i] = ch

    for k, a in rows.items():
        hits = sum(a)
        for i, on in enumerate(a):
            if not on:
                continue
            if k in ('BD',):
                put('bd', i, 'x')
            elif k in ('SN', 'CL'):
                put('sn', i, 'x')
            elif k == 'RS':
                put('ss', i, 'x')
            elif k == 'CH':
                put('hh', i, 'x')
            elif k == 'OH':
                put('hh', i, 'o')
            elif k == 'SH':
                put('hh', i, 'g')
            elif k == 'CY':
                put('rd' if hits >= 4 else 'cr', i, 'x')
            elif k in ('HT', 'HC'):
                put('t1', i, 'x')
            elif k == 'MT':
                put('t2', i, 'x')
            elif k == 'LT':
                put('t3', i, 'x')
            elif k == 'CB':
                put('cb', i, 'x')
        for src in ('CL', 'SH', 'HC'):
            if src in rows and any(rows[src]):
                notes.append(src)
    for cells in tr.values():
        for i in range(16):
            if acc[i] and cells[i] in ('x', 'o'):
                cells[i] = cells[i].upper()
    return tr, sorted(set(notes))


def main():
    P = json.load(open(sys.argv[1]))['patterns']
    T = json.load(open(sys.argv[2]))['patterns']
    tacc = {}
    for t in T:
        tacc.setdefault((t['pdf_page'], norm(t['name'])), []).append(sorted(set(t.get('accents', []))))
    groups = []
    for p in P:
        key = (p['genre'], p['name'])
        if p.get('variant') and groups and groups[-1][0] == key and groups[-1][1][-1].get('variant'):
            groups[-1][1].append(p)
        else:
            groups.append((key, [p]))
    out = ['# Pocket Operations (Paul Wenzel, Rev. 3.1, shittyrecording.studio) - nur privat',
           '# automatisch erzeugt aus dnvsfn/ableton-pocket-operations data/patterns.json',
           '# Zuordnung: CL->sn, RS->ss, SH->hh g, CY->rd/cr, HC->t1; Ratchets als normaler Schlag',
           '']
    seen = set()
    for (genre, name), ps in groups:
        bars, notes = [], set()
        for p in ps:
            k = (p['pdf_page'], norm(p['name'] + (p.get('variant') or '')))
            extra = tacc.get(k, [[]])
            tr, nt = bar(p, extra.pop(0) if extra else [])
            bars.append(tr)
            notes.update(nt)
        tracks = [t for t in ORDER if any(t in b for b in bars)]
        variants = '+'.join(p['variant'] for p in ps if p.get('variant'))
        title = name.title().replace('’S', '’s')
        disp = f'PO {genre}: {title}' + (f' ({variants})' if variants else '')
        pid = 'user.po-' + slug(f'{genre}-{name}')
        while pid in seen:
            pid += '-2'
        seen.add(pid)
        out.append(f'# S. {ps[0]["pdf_page"]}' + (f'; ersetzt: {", ".join(sorted(notes))}' if notes else ''))
        out.append(f'name:  {disp}')
        out.append(f'id:    {pid}')
        out.append(f'tags:  {slug(genre)}, pocket-operations')
        out.append('grid:  1/16')
        for t in tracks:
            cells = ' | '.join(' '.join(b.get(t, ['.'] * 16)) for b in bars)
            out.append(f'{t} | {cells} |')
        out.append('---')
    if out[-1] == '---':
        out.pop()
    open(sys.argv[3], 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print(len(groups), 'Tab-Patterns aus', len(P), 'Einzeltakten')


if __name__ == '__main__':
    main()
