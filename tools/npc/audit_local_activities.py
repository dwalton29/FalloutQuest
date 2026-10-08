#!/usr/bin/env python3
"""Inspect original resident Sandbox/Accompany/Use Item At fields; no asset export."""
import json, struct, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'dialogue'))
from audit_esm import records, first, values, text, form
path = sys.argv[1]
index = {r['id']: r for r in records(Path(path).read_bytes())}
census = json.loads(subprocess.check_output([sys.executable, str(Path(__file__).with_name('audit_residents.py')), path]))
result = []
for actor in census['actors']:
    for package in actor['packages']:
        if package['type'] not in ('Sandbox', 'Accompany', 'Use Item At'):
            continue
        record = index[int(package['id'], 16)]
        raw = first(record, 'PKDT')
        entry = dict(actor=actor['name'], reference=actor['reference'], cell=actor['cell'], **package)
        entry['flags'] = f'{form(raw):08X}'
        entry['typeFlags'] = f'{struct.unpack_from("<H", raw, 8)[0] if len(raw) >= 10 else 0:04X}'
        entry['target'] = list(struct.unpack('<IIi', first(record, 'PTDT')[:12])) if len(first(record, 'PTDT')) >= 12 else None
        entry['scripts'] = [text(v) for v in values(record, 'SCTX')]
        entry['idles'] = [f'{form(v[offset:]):08X}' for v in values(record, 'IDLA') for offset in range(0, len(v), 4)]
        result.append(entry)
print(json.dumps(result, indent=2))
