#!/usr/bin/env python3
"""Extract metadata from the supplied 32-bit Fallout 3 executable, without running it.

Layout corroboration: FOSE CommandTable.h, CommandInfo (0x28), ParamInfo (0x0C).
Anchor validation uses GetDisabled (0x1023), plus the complete contiguous opcode
sequence starting at 0x1000. Emits metadata only; not executable bytes or code.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def extract(data):
    def u16(o): return struct.unpack_from('<H', data, o)[0]
    def u32(o): return struct.unpack_from('<I', data, o)[0]
    if data[:2] != b'MZ':
        raise ValueError('not PE')
    pe = u32(60)
    if data[pe:pe + 4] != b'PE\0\0' or u16(pe + 4) != 0x14c:
        raise ValueError('expected x86 PE')
    opt = pe + 24
    if u16(opt) != 0x10b:
        raise ValueError('expected PE32')
    image = u32(opt + 28)
    sections = []
    for i in range(u16(pe + 6)):
        o = opt + u16(pe + 20) + i * 40
        rva, size, raw = struct.unpack_from('<III', data, o + 12)
        if raw + size > len(data):
            raise ValueError('section exceeds file')
        sections.append((rva, size, raw))

    def offset(va, size=1):
        for rva, n, raw in sections:
            d = va - image - rva
            if 0 <= d and d + size <= n:
                return raw + d
        raise ValueError('unmapped address')

    def string(va):
        o = offset(va)
        end = data.find(b'\0', o, min(o + 2048, len(data)))
        if end < 0:
            raise ValueError('unterminated metadata string')
        return data[o:end].decode('ascii')

    name = data.index(b'GetDisabled\0')
    name_va = next(image + rva + name - raw for rva, n, raw in sections if raw <= name < raw + n)
    anchor, at = [], 0
    while True:
        at = data.find(struct.pack('<I', name_va), at)
        if at < 0:
            break
        if at + 40 <= len(data) and u32(at + 8) == 0x1023:
            anchor.append(at)
        at += 1
    if len(anchor) != 1:
        raise ValueError('ambiguous native table anchor')
    start = anchor[0] - 35 * 40
    commands = []
    for i in range(4096):
        o = start + i * 40
        if o + 40 > len(data):
            raise ValueError('unterminated command table')
        longname, alias, opcode, helptext, parent, n, params, execute, parse, evaluate, flags = struct.unpack_from('<IIIIHHIIIII', data, o)
        if opcode != 0x1000 + i:
            break
        if parent > 1 or n > 32:
            raise ValueError('invalid command metadata')
        arguments = []
        if n:
            p = offset(params, n * 12)
            for k in range(n):
                label, type_id, optional = struct.unpack_from('<III', data, p + k * 12)
                arguments.append({'label': string(label), 'type_id': type_id, 'optional_raw': optional})
        commands.append({'name': string(longname), 'alias': string(alias), 'opcode': opcode,
                         'condition_id': opcode - 0x1000 if evaluate else None,
                         'needs_parent': bool(parent), 'parameters': arguments,
                         'execute_address': f'{execute:08X}', 'parse_address': f'{parse:08X}',
                         'evaluate_address': f'{evaluate:08X}', 'flags': flags, 'file_offset': f'{o:X}'})
    if len(commands) < 36 or commands[35]['name'] != 'GetDisabled':
        raise ValueError('table validation failed')
    return {'schema': 1, 'exe_sha256': hashlib.sha256(data).hexdigest(),
            'layout': {'command_size': 40, 'parameter_size': 12, 'image_base': f'{image:08X}', 'table_file_offset': f'{start:X}'},
            'command_count': len(commands), 'commands': commands,
            'limits': ['Metadata addresses are evidence, never called by FalloutQuest.',
                       'Function return types and complete gameplay semantics are not encoded by these signatures.',
                       'Non-native control-flow opcodes and SCDA parameter serialization require separate M5 decoding.']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    report = extract(Path(args.exe).read_bytes())
    Path(args.output).write_text(json.dumps(report, indent=2) + '\n')
    print(f'Validated {report["command_count"]} contiguous native commands')
