#!/usr/bin/env python3
"""Reproducible metadata inventory; never copies game source or bytecode to Git.

Source tokens are lexical observations, not claims of executable compatibility.
SCDA framing/opcodes/signatures intentionally remain unverified until M5.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'dialogue'))
from audit_esm import records, first, values, text, form


def audit(path):
    data = Path(path).read_bytes()
    counts = collections.Counter()
    events, commands, variables, conditions, containers = (collections.Counter() for _ in range(5))
    scripts, references, candidates = [], {}, []
    source_count = compiled_count = both = compiled_bytes = 0
    for r in records(data):
        counts[r['type']] += 1
        for v in values(r, 'CTDA'):
            if len(v) >= 12:
                conditions[str(struct.unpack_from('<H', v, 8)[0])] += 1
        source = values(r, 'SCTX')
        bytecode = values(r, 'SCDA')
        if source or bytecode:
            containers[r['type']] += 1
        if r['type'] in ('REFR', 'ACHR', 'ACRE'):
            parent = first(r, 'XESP')
            references[f'{r["id"]:08X}'] = {
                'editor': text(first(r, 'EDID')), 'base': f'{form(first(r, "NAME")):08X}',
                'initially_disabled': bool(r['flags'] & 0x800),
                'parent': f'{form(parent):08X}', 'parent_flags': parent[4] if len(parent) >= 5 else 0}
        for raw in source:
            for line in text(raw).splitlines():
                # Quoted strings may contain semicolons; only inspect the first token.
                clean = line.strip()
                if not clean or clean.startswith(';'):
                    continue
                tokens = clean.split()
                op = tokens[0].lower()
                if op == 'begin' and len(tokens) > 1:
                    events[tokens[1].lower()] += 1
                elif op in ('short', 'long', 'int', 'float', 'ref'):
                    variables[op] += 1
                elif op not in ('scn', 'scriptname', 'end'):
                    commands[op.rsplit('.', 1)[-1]] += 1
        if r['type'] == 'SCPT':
            s = any(v.rstrip(b'\0') for v in source)
            b = any(bytecode)
            source_count += s
            compiled_count += b
            both += s and b
            compiled_bytes += sum(map(len, bytecode))
            headers = values(r, 'SCHR')
            scripts.append({'form': f'{r["id"]:08X}', 'editor': text(first(r, 'EDID')),
                            'source': s, 'compiled': b,
                            'byte_count': sum(map(len, bytecode)),
                            'header_sizes': [len(h) for h in headers],
                            'reference_count': len(values(r, 'SCRO'))})
        editor = text(first(r, 'EDID'))
        if re.search(r'megaton.*(gate|entrance)|weld|ms11detonator|misterburkeref', editor, re.I):
            candidates.append({'form': f'{r["id"]:08X}', 'type': r['type'], 'editor': editor,
                               'script': f'{form(first(r, "SCRI")):08X}',
                               'base': f'{form(first(r, "NAME")):08X}'})
    parented = {k: v for k, v in references.items() if v['parent'] != '00000000'}
    return {'schema': 1, 'esm_sha256': hashlib.sha256(data).hexdigest(),
            'record_counts': dict(sorted(counts.items())),
            'scpt': {'records': counts['SCPT'], 'with_source': source_count,
                     'with_compiled': compiled_count, 'with_both': both, 'compiled_bytes': compiled_bytes},
            'script_containers': dict(sorted(containers.items())),
            'events_lexical': dict(events.most_common()), 'statement_heads_lexical': dict(commands.most_common()),
            'declaration_types_lexical': dict(variables.most_common()),
            'condition_function_ids': dict(conditions.most_common()),
            'native_signatures': {'status': 'extracted separately; see SCRIPTING_NATIVE_COMMANDS.json'},
            'compiled_opcodes': {'status': 'envelope inventory in SCRIPTING_SCDA_INVENTORY.json; payload execution not implemented'},
            'reference_summary': {'placed': len(references), 'parented': len(parented),
                                  'initially_disabled': sum(v['initially_disabled'] for v in references.values()),
                                  'parent_flags': dict(collections.Counter(str(v['parent_flags']) for v in parented.values())),
                                  'missing_parents': sum(v['parent'] not in references for v in parented.values())},
            'integration_records': candidates, 'scripts': scripts,
            'validation': {'parsed': counts['SCPT'], 'executed_by_this_tool': 0, 'gameplay_validated': 0},
            'limitations': ['Lexical heads include statements, aliases and control flow; expression functions are not counted as statements.',
                           'All SCTX owners are counted, including INFO/QUST/PACK result fragments.',
                           'No native signature, opcode, runtime-success or gameplay-compatibility claim is inferred from metadata.']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('esm')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    report = audit(args.esm)
    Path(args.output).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ('scpt', 'reference_summary', 'events_lexical')}, indent=2))
