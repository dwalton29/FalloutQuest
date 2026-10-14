#!/usr/bin/env python3
"""Inventory SCDA instruction envelopes only, without executing payloads.

FOSE documents control opcodes 0x10..0x1F. The dot opcode is a two-word
reference-prefix (opcode, SCRO/local reference index), not a length envelope.
Other observed instructions have (u16 opcode, u16 payload bytes, payload).
Expressions may contain nested native calls: those are not counted here.
"""
import argparse
import collections
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'dialogue'))
from audit_esm import records, values


def decode_envelopes(data, known):
    at, instructions = 0, []
    while at < len(data):
        if at + 4 > len(data):
            raise ValueError(f'truncated header at {at}')
        opcode, length = struct.unpack_from('<HH', data, at)
        if opcode not in known:
            raise ValueError(f'unknown envelope opcode {opcode:04X} at {at}')
        end = at + 4 if opcode == 0x1c else at + 4 + length
        if end > len(data):
            raise ValueError(f'payload exceeds SCDA at {at}')
        instructions.append(opcode)
        at = end
    return instructions


def audit(esm, native):
    known = {int(c['opcode']) for c in native['commands']} | set(range(0x10, 0x20))
    counts, failures = collections.Counter(), []
    total = valid = 0
    for r in records(Path(esm).read_bytes()):
        for i, data in enumerate(values(r, 'SCDA')):
            if not data:
                continue
            total += 1
            try:
                instructions = decode_envelopes(data, known)
                counts.update(f'{op:04X}' for op in instructions)
                valid += 1
            except ValueError as error:
                failures.append({'form': f'{r["id"]:08X}', 'type': r['type'], 'fragment': i, 'error': str(error)})
    return {'schema': 1, 'fragments': total, 'envelope_validated': valid,
            'envelope_opcode_frequency': dict(sorted(counts.items())), 'failures': failures,
            'executed': 0, 'expression_payloads_validated': 0,
            'limits': ['Envelope boundaries are not native parameter, expression, branch-target or gameplay validation.',
                       'Dot operand is retained as a reference index; not interpreted as a byte count.',
                       'Counts exclude nested expression opcodes. No SCDA bytecode interpreter is implemented.']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('esm'); parser.add_argument('native_json'); parser.add_argument('--output', required=True)
    args = parser.parse_args()
    result = audit(args.esm, json.loads(Path(args.native_json).read_text()))
    Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    print(f'{result["envelope_validated"]}/{result["fragments"]} SCDA fragment envelopes validated')
