#!/usr/bin/env python3
"""Print an ordered stage timeline from an adb logcat capture; no estimates."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
args = parser.parse_args()
patterns = ('SCENE LOAD BEGIN:', 'SCENE FIRST PLAYABLE FRAME:', 'SCENE WORKER TIMELINE:', 'SCENE CPU READY:',
            'SCENE TEXTURE CPU:', 'SCENE UPLOAD TIMELINE:', 'SCENE COLLISION READY:', 'SCENE LOAD READY:',
            'LOADING ASSET CPU READY:', 'LOADING BASE READY:', 'LOADING FIRST FRAME:', 'LOADING FIRST ART FRAME:',
            'LOADING FIRST ANIMATED FRAME:', 'LOADING FRAME STATS:',
            'LOADING LONG OP:', 'LOADING PRESENTATION COMPLETE:',
            'EXTERIOR WARMUP', 'TERRAIN CPU START:', 'TERRAIN COMMIT:')
for line_number, line in enumerate(args.log.read_text(errors='replace').splitlines(), 1):
    if any(event in line for event in patterns):
        print(f'{line_number:6d} {line.strip()}')
