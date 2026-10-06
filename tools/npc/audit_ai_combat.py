#!/usr/bin/env python3
"""Audit user-supplied FO3 records without copying proprietary assets.

Enums/offsets: TES5Edit dev-4.1.6 Core/wbCommon.pas and wbDefinitionsFO3.pas.
"""
import argparse
import collections
import json
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'dialogue'))
from audit_esm import records, first, values, text
TYPES = ['Find', 'Follow', 'Escort', 'Eat', 'Sleep', 'Wander', 'Travel',
         'Accompany', 'Use Item At', 'Ambush', 'Flee Not Combat', 'Unused',
         'Sandbox', 'Patrol', 'Guard', 'Dialogue', 'Use Weapon']
def audit(path):
    all_records = list(records(Path(path).read_bytes()))
    counts = collections.Counter(r['type'] for r in all_records)
    print('Records', json.dumps({t: counts[t] for t in
          ['NPC_', 'PACK', 'CSTY', 'FACT', 'WEAP', 'AMMO', 'PROJ', 'ARMO',
           'IDLE', 'NAVM', 'NAVI', 'LVLN', 'LVLC', 'LVLI']}))
    packages = collections.Counter()
    styles = collections.Counter()
    aidt = collections.Counter()
    scripts = collections.Counter()
    for r in all_records:
        if r['type'] == 'PACK':
            data = first(r, 'PKDT')
            if len(data) >= 8:
                kind = data[4]
                packages[kind] += 1
                if first(r, 'SCDA') or first(r, 'SCTX').rstrip(b'\0'):
                    scripts[kind] += 1
        elif r['type'] == 'CSTY':
            styles[tuple(len(first(r, t)) for t in ['CSTD', 'CSAD', 'CSSD'])] += 1
        elif r['type'] == 'NPC_':
            ai = first(r, 'AIDT')
            aidt[len(ai)] += 1
    for kind, count in sorted(packages.items()):
        print('PACK', kind, TYPES[kind] if kind < len(TYPES) else 'Unsupported',
              'count', count, 'scripts', scripts[kind])
    print('CSTY lengths CSTD/CSAD/CSSD', dict(styles))
    print('AIDT lengths', dict(aidt))
    for r in all_records:
        if r['type'] == 'GMST' and text(first(r, 'EDID')) in [
                'fSneakMaxDistance', 'fMaxArmorRating', 'fDamageSkillBase',
                'fDamageSkillMult', 'fDamageGunWeapCondBase',
                'fDamageGunWeapCondMult']:
            print('GMST', text(first(r, 'EDID')), struct.unpack('<f', first(r, 'DATA'))[0])
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('esm')
    audit(parser.parse_args().esm)
