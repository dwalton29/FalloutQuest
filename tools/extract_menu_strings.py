#!/usr/bin/env python3
"""Reproduce/check menu string defaults from a user-supplied Fallout3.exe.

No executable or menu assets are distributed. --check verifies the compiled
header against the PE GameSetting records, independent of C++ formatting.
To regenerate, supply --menus pointing at the original extracted menus tree.
"""
import argparse
import json
from pathlib import Path
import re
import struct

NATIVE_KEYS = '''sInventoryWeapons sInventoryApparel sInventoryAid sInventoryMisc
sInventoryAmmoTab sLocalMapTabText sWorldMapTabText sQuestsTabText sNotesTabText
sCommsTabText sTravelQuestion sYes sNo sInventoryEquip sInventoryUnequip
sInventoryUse'''.split()


def recover(path):
    data = path.read_bytes()
    if data[:2] != b'MZ':
        raise ValueError('Expected PE32 Fallout3.exe')
    pe = struct.unpack_from('<I', data, 60)[0]
    if data[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', data, pe + 24)[0] != 0x10b:
        raise ValueError('Expected PE32 Fallout3.exe')
    base = struct.unpack_from('<I', data, pe + 52)[0]
    count, size = struct.unpack_from('<H', data, pe + 6)[0], struct.unpack_from('<H', data, pe + 20)[0]
    sections = []
    for i in range(count):
        _, _, va, raw_size, raw = struct.unpack_from('<8sIIII', data, pe + 24 + size + 40 * i)
        sections.append((va, raw, raw_size))

    def address(offset):
        for va, raw, length in sections:
            if raw <= offset < raw + length:
                return base + va + offset - raw
        return 0

    def string(pointer):
        for va, raw, length in sections:
            if base + va <= pointer < base + va + length:
                offset = raw + pointer - base - va
                end = data.find(b'\0', offset, min(raw + length, offset + 1000))
                if end >= offset and all(c >= 32 or c in (9, 10, 13) for c in data[offset:end]):
                    return data[offset:end].decode('cp1252')
        return None

    values, proof = {}, {}
    for match in re.finditer(rb'\x00(s[A-Z][A-Za-z0-9_]+)\x00', data):
        name = match[1].decode('ascii')
        pointer = address(match.start() + 1)
        if not pointer:
            continue
        at = 0
        while True:
            at = data.find(struct.pack('<I', pointer), at)
            if at < 0:
                break
            if at >= 8 and at % 4 == 0:
                value = string(struct.unpack_from('<I', data, at - 4)[0])
                if value is not None:
                    values[name] = value
                    proof[name] = {'record_file_offset': hex(at - 8), 'name_va': hex(pointer)}
                    break
            at += 1
    return values, proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--menus', type=Path)
    parser.add_argument('--check', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--proof', type=Path)
    args = parser.parse_args()
    values, proof = recover(args.exe)
    if args.check:
        quoted = r'"(?:\\.|[^"\\])*"'
        pairs = re.findall(r'\{\s*(' + quoted + r')\s*,\s*(' + quoted + r')\s*\}', args.check.read_text())
        assert pairs, 'No compiled defaults found'
        for key, value in pairs:
            key, value = json.loads(key), json.loads(value)
            assert key in values and values[key] == value, f'Mismatched executable default: {key}'
        print(f'Checked {len(pairs)} compiled UI strings against original executable records')
    if args.output:
        if not args.menus:
            parser.error('--output requires --menus')
        keys = set(NATIVE_KEYS)
        for path in args.menus.rglob('*.xml'):
            keys.update(re.findall(r'&-([^;]+);', path.read_text(encoding='cp1252')))
        selected = {key: values[key] for key in sorted(keys) if key in values}
        header = '''#pragma once
#include <string>
#include <unordered_map>
namespace fo3menu {
// Original Fallout3.exe GameSetting string defaults. ESM/localized values win.
inline const std::unordered_map<std::string,std::string>& StringDefaults(){static const std::unordered_map<std::string,std::string> values{
'''
        for key, value in selected.items():
            header += ' {' + json.dumps(key) + ',' + json.dumps(value, ensure_ascii=True) + '},\n'
        header += '};return values;}\n}\n'
        args.output.write_text(header)
        if args.proof:
            args.proof.write_text(json.dumps({key: proof[key] for key in selected}, indent=2) + '\n')
        print(f'Recovered {len(selected)} menu defaults; ESM overrides retain priority')


if __name__ == '__main__':
    main()
