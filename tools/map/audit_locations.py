#!/usr/bin/env python3
"""Read-only FO3 map/discovery metadata audit. Emits metadata, never game assets."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'dialogue'))
from audit_esm import records, first, text

SETTINGS = ('fSayOncePerDayInfoTimer', 'iMapMarkerRevealDistance', 'iMapMarkerVisibleDistance',
            'iXPRewardDiscoverMapMarker', 'fWorldMapMinZoom', 'fWorldMapMaxZoom',
            'fLocalMapMinZoom', 'fLocalMapMaxZoom', 'fMoveBaseSpeed',
            'fHostileActorExteriorDistance', 'fHostileActorInteriorDistance',
            'sDiscoveredText', 'sTravelQuestion', 'sTravel', 'sYes', 'sNo')

def defaults(data):
    u16 = lambda at: struct.unpack_from('<H', data, at)[0]
    u32 = lambda at: struct.unpack_from('<I', data, at)[0]
    if data[:2] != b'MZ':
        raise ValueError('expected supplied PE executable')
    pe = u32(60)
    if data[pe:pe+4] != b'PE\0\0' or u16(pe+4) != 0x14c:
        raise ValueError('expected x86 PE')
    opt = pe + 24
    if u16(opt) != 0x10b:
        raise ValueError('expected PE32')
    image = u32(opt+28)
    sections = []
    for i in range(u16(pe+6)):
        at = opt + u16(pe+20) + i*40
        rva, size, raw = struct.unpack_from('<III', data, at+12)
        if raw + size > len(data):
            raise ValueError('truncated section')
        sections.append((data[at:at+8].rstrip(b'\0'), image+rva, raw, size))
    def va(offset):
        return next(base+offset-raw for _, base, raw, size in sections if raw <= offset < raw+size)
    def offset(address):
        return next(raw+address-base for _, base, raw, size in sections if base <= address < base+size)
    out = {}
    for name in SETTINGS:
        at = data.index(name.encode('ascii')+b'\0')
        key = va(at)
        objects = []
        for section, base, raw, size in sections:
            if section != b'.data':
                continue
            at = raw
            while True:
                at = data.find(struct.pack('<I', key), at, raw+size)
                if at < 0:
                    break
                if at >= raw+8:
                    objects.append((at-8, u32(at-4)))
                at += 1
        if len(objects) != 1:
            raise ValueError('ambiguous Setting object: '+name)
        at, bits = objects[0]
        if name[0] == 's':
            start = offset(bits)
            value = data[start:data.index(0, start)].decode('cp1252')
        else:
            value = struct.unpack('<i' if name[0] == 'i' else '<f', struct.pack('<I', bits))[0]
        out[name] = {'value': value, 'setting_va': f'{va(at):08X}'}
    return out

def audit(esm, exe):
    master, executable = Path(esm).read_bytes(), Path(exe).read_bytes()
    overrides, markers = {}, []
    for r in records(master):
        editor = text(first(r, 'EDID'))
        if r['type'] == 'GMST' and editor in SETTINGS:
            data = first(r, 'DATA')
            overrides[editor] = (text(data) if editor.startswith('s') else
                struct.unpack('<i' if editor.startswith('i') else '<f', data)[0])
        if r['type'] == 'REFR' and first(r, 'XMRK', None) is not None:
            flags, icon = first(r, 'FNAM'), first(r, 'TNAM')
            markers.append({'form': f'{r["id"]:08X}', 'name': text(first(r, 'FULL')),
                            'flags': flags[0] if flags else 0,
                            'type': icon[0] if icon else 0,
                            'disabled': bool(r['flags'] & 0x800)})
    return {'esm_sha256': hashlib.sha256(master).hexdigest(),
            'exe_sha256': hashlib.sha256(executable).hexdigest(),
            'executable_defaults': defaults(executable), 'esm_overrides': overrides,
            'marker_count': len(markers),
            'marker_types': dict(sorted(collections.Counter(m['type'] for m in markers).items())),
            'initially_disabled': sum(m['disabled'] for m in markers),
            'show_all_hidden': sum(bool(m['flags'] & 4) for m in markers),
            'examples': [m for m in markers if m['form'] in ('0000321F', '000032D4', '0001D4E3', '00062743')]}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('esm'); parser.add_argument('exe'); parser.add_argument('--output', required=True)
    args = parser.parse_args()
    Path(args.output).write_text(json.dumps(audit(args.esm, args.exe), indent=2)+'\n')
