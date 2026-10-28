#!/usr/bin/env python3
"""Read original actor/template metadata without extracting or committing assets."""
import argparse
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'dialogue'))
from audit_esm import records, first, values, text, form

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('master', type=Path)
    args = parser.parse_args()
    index = {r['id']: r for r in records(args.master.read_bytes())}
    result = {}
    for ident in (0x1fb3a, 0xc6e91, 0x41610, 0x41611, 0x41614):
        reference = index[ident]
        base = index[form(first(reference, 'NAME'))]
        chain = []
        pending = [base['id']]
        seen = set()
        while pending:
            current = pending.pop()
            if current in seen:
                continue
            seen.add(current)
            record = index[current]
            template = form(first(record, 'TPLT'))
            entries = values(record, 'LVLO')
            chain.append({'form': f'{current:08X}', 'type': record['type'],
                          'editor': text(first(record, 'EDID')),
                          'template': f'{template:08X}',
                          'levels': [int.from_bytes(e[:2], 'little') for e in entries]})
            if template:
                pending.append(template)
            pending.extend(int.from_bytes(e[4:8], 'little') for e in entries if len(e) == 12)
        result[f'{ident:08X}'] = {
            'editor': text(first(reference, 'EDID')),
            'base': f'{base["id"]:08X}', 'name': text(first(base, 'FULL')),
            'reference_flags': reference['flags'],
            'enable_parent': f'{form(first(reference, "XESP")):08X}',
            'script': f'{form(first(base, "SCRI")):08X}', 'template_chain': chain}
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
