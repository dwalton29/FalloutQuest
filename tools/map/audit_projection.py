#!/usr/bin/env python3
"""Verify the map constants/operations against the supplied original FO3 EXE.
Read-only original-data audit. No executable or original assets are exported.
"""
import struct
import sys
from pathlib import Path

def verify(path):
    data=Path(path).read_bytes()
    pe=struct.unpack_from('<I',data,60)[0]
    count=struct.unpack_from('<H',data,pe+6)[0]
    optional=struct.unpack_from('<H',data,pe+20)[0]
    sections=[]
    for index in range(count):
        offset=pe+24+optional+40*index
        size,virtual,raw_size,raw=struct.unpack_from('<4I',data,offset+8)
        sections.append((virtual,max(size,raw_size),raw))
    def read(address,size):
        for virtual,length,raw in sections:
            if 0x400000+virtual<=address and address+size<=0x400000+virtual+length:
                start=raw+address-0x400000-virtual
                return data[start:start+size]
        raise ValueError(f'unmapped address {address:08X}')
    for address,value in [(0xF9B8B0,104/1024),(0xF9B8BC,816/1024),(0xF3ED74,.5)]:
        assert struct.unpack('<f',read(address,4))[0]==value,hex(address)
    # Original world-map path multiplies both normalized coordinates by the
    # content extent, then adds the border. NW Y and SE X are incremented.
    for address,expected in [(0x946172,'f30f5915bcb8f900'),(0x946182,'f30f5815b0b8f900'),
                             (0x9505C0,'0fbf460a40c1e00c'),(0x9505E2,'0fbf460c40c1e00c'),
                             (0x7C8D7A,'f30f58b1a0000000'),(0x7C8DD9,'f30f108194000000')]:
        expected=bytes.fromhex(expected)
        assert read(address,len(expected))==expected,hex(address)
    print('Original FO3 map image border/content, whole-cell bounds and centred game-unit transform verified')

if __name__=='__main__':
    if len(sys.argv)!=2:
        print('usage: audit_projection.py /path/to/Fallout3.exe',file=sys.stderr)
        sys.exit(2)
    verify(sys.argv[1])
