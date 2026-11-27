#!/usr/bin/env python3
"""Inventory SCDA instruction envelopes or audit original fragment dependencies with a host probe.

FOSE documents control opcodes 0x10..0x1F. The dot opcode is a two-word
reference-prefix (opcode, SCRO/local reference index), not a length envelope.
Other observed instructions have (u16 opcode, u16 payload bytes, payload).
Expressions may contain nested native calls: those are not counted here.
"""
import argparse
import collections
import json
from pathlib import Path
import gzip
import hashlib
import io
import subprocess
import tempfile
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'dialogue'))
from audit_esm import records, values
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fragment_requirements import event_dispatch_sites, fragment_rows, instructions, read_probe, summarize


def decode_envelopes(data, known):
    return [op for _, op, _, _ in instructions(data, known)]


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
                       'Counts exclude nested expression opcodes. The envelope pass executes no bytecode.']}


def coverage(esm, native, probe, detail, result):
    # Run the probe on the same input every time; stale results cannot be supplied.
    with tempfile.TemporaryFile(mode='w+', encoding='utf-8', errors='replace') as log:
        subprocess.run([str(Path(probe).resolve()), str(esm)], stdout=log, check=True)
        log.seek(0)
        probes = read_probe(log)
    data = Path(esm).read_bytes()
    rows = list(fragment_rows(data, native['commands'], probes))
    if len(rows) != result['fragments']:
        raise ValueError('raw SCDA count differs from requirements row count')
    stats = summarize(rows)
    result['schema'] = 2
    result['limits'] = [
        'Top-level executed and expression_payloads_validated fields describe the envelope-only pass.',
        'The requirements section and index add operand decoding, native compilation, expression binding and isolated host previews.',
        'An entered preview proves one candidate path. Live event delivery, scene effects and alternate branches require gameplay verification.',
    ]
    result['requirements'] = {
        'esm_sha256': hashlib.sha256(data).hexdigest(),
        'count_unit': 'distinct nonempty SCDA fragments; each dependency counts once per fragment',
        **stats,
        'literal_event_dispatch_sites': event_dispatch_sites(Path(__file__).resolve().parents[2] / 'app/src/main/cpp', stats['events']),
    }
    # Write a deterministic metadata-only index, even when repeated builds are in different directories.
    with tempfile.NamedTemporaryFile(dir=detail.parent, delete=False) as tmp:
        temp_path = Path(tmp.name)
    try:
        with temp_path.open('wb') as raw, gzip.GzipFile(fileobj=raw, filename='', mode='wb', mtime=0) as zipped:
            with io.TextIOWrapper(zipped, encoding='utf-8', newline='\n') as out:
                for row in rows:
                    out.write(json.dumps(row, sort_keys=True, separators=(',', ':'), allow_nan=False) + '\n')
        result['requirements']['index_sha256'] = hashlib.sha256(temp_path.read_bytes()).hexdigest()
        temp_path.replace(detail)
    finally:
        temp_path.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('esm')
    parser.add_argument('native_json')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--probe', type=Path, help='script_fragment_probe host executable')
    parser.add_argument('--fragments', type=Path, help='write per-fragment JSON Lines index (.gz)')
    args = parser.parse_args()
    if bool(args.probe) != bool(args.fragments):
        parser.error('--probe and --fragments must be used together')
    if args.fragments and args.fragments.resolve() == args.output.resolve():
        parser.error('summary and fragment index must be distinct')
    native = json.loads(Path(args.native_json).read_text())
    result = audit(args.esm, native)
    if args.probe:
        coverage(args.esm, native, args.probe, args.fragments, result)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'{result["envelope_validated"]}/{result["fragments"]} SCDA fragment envelopes validated')
