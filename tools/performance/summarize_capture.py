#!/usr/bin/env python3
"""Summarize bounded v223 ADB captures; CPU, cadence and delayed GPU are separate."""
import argparse
import csv
import math
import statistics
from pathlib import Path


def percentile(values, p):
    v = sorted(values)
    pos = (len(v) - 1) * p
    lo = math.floor(pos)
    hi = math.ceil(pos)
    return v[lo] + (v[hi] - v[lo]) * (pos - lo)


def valid_gpu_us(value):
    # v223 Quest elapsed queries sometimes returned uint64-wrapped negatives.
    return math.isfinite(value) and 0 <= value <= ((1 << 63) - 1) / 1000


def stats(values):
    return dict(n=len(values), mean=statistics.mean(values), median=statistics.median(values),
                p95=percentile(values, .95), p99=percentile(values, .99), maximum=max(values),
                over72=100 * sum(v > 1e6 / 72 for v in values) / len(values),
                over90=100 * sum(v > 1e6 / 90 for v in values) / len(values))


def cadence_values(rows, predicate):
    # Cadence on row N spans starts N-1 -> N. Exclude phase/focus boundaries:
    # the first gameplay row can otherwise inherit a many-second loading gap.
    return [float(current['cadenceUs']) for previous, current in zip(rows, rows[1:])
            if predicate(previous) and predicate(current)
            and int(current['serial']) == int(previous['serial']) + 1
            and float(current['cadenceUs']) > 0]


def report(path):
    with path.open() as f:
        rows = list(csv.DictReader(line for line in f if not line.startswith('#')))
    if not rows:
        raise ValueError(f'{path}: no captured frames')
    print(path.read_text().splitlines()[0])
    invalid_gpu = sum(not valid_gpu_us(float(r[k])) and float(r[k]) >= 0
                      for r in rows for k in ('gpuEye0Us', 'gpuEye1Us', 'gpuEyesUs'))
    print(f'\n{path} ({len(rows)} frames; GPU excludes preparation; overlapping scopes are not additive)')
    if invalid_gpu:
        print(f'WARNING: excluded {invalid_gpu} invalid/wrapped GPU values; GPU bottleneck cannot be inferred from them.')
    for group, predicate in (
        ('loading', lambda r: r['loading'] == '1'),
        ('gameplay', lambda r: r['loading'] == '0' and r['blocked'] == '0' and r['submitted'] == '1'),
        ('npc-light', lambda r: r['loading'] == '0' and r['blocked'] == '0' and r['submitted'] == '1' and int(r['residents']) == 0),
        ('npc-present', lambda r: r['loading'] == '0' and r['blocked'] == '0' and r['submitted'] == '1' and int(r['residents']) > 0),
        ('gpu-sampled-v232', lambda r: r['loading']=='0' and r['blocked']=='0' and r['submitted']=='1' and 'gpu0_terrainUs' in r and int(r['serial'])%8==0),
        ('gpu-unsampled-v232', lambda r: r['loading']=='0' and r['blocked']=='0' and r['submitted']=='1' and 'gpu0_terrainUs' in r and int(r['serial'])%8!=0),
        ('combat', lambda r: r['loading'] == '0' and r['submitted'] == '1' and int(r['combat']) > 0),
        ('streaming', lambda r: r['loading'] == '0' and r['streaming'] == '1'),
    ):
        selected = [r for r in rows if predicate(r)]
        if not selected:
            print(f'{group}: no samples')
            continue
        print(f'\n{group}: {len(selected)} frames; mean residents={statistics.mean(int(r["residents"]) for r in selected):.1f}')
        pairs = sum(valid_gpu_us(float(r['gpuEyesUs'])) for r in selected)
        print(f'GPU eye pairs: {pairs} valid, {len(selected)-pairs} unavailable/invalid; '
              'cadence uses adjacent frames within this group; v232 GPU is sampled every 8 frames, missing=-1; '
              'GPU scopes overlap: post includes bloom, reflection includes mirrorTerrain')
        print('metric,n,mean_ms,median_ms,p95_ms,p99_ms,max_ms,over72_percent,over90_percent')
        for key in rows[0]:
            if not key.endswith('Us') or key == 'periodUs':
                continue
            # -1 is absent/disjoint/pending. Zero cadence is the initial frame.
            vals = cadence_values(rows, predicate) if key == 'cadenceUs' else [
                float(r[key]) for r in selected if float(r[key]) >= 0
                and (not key.startswith('gpu') or valid_gpu_us(float(r[key])))]
            if vals:
                v = stats(vals)
                print(f'{key},{v["n"]},' + ','.join(f'{v[k]/1000:.3f}' for k in ('mean', 'median', 'p95', 'p99', 'maximum')) + f',{v["over72"]:.2f},{v["over90"]:.2f}')
        for key in ('npcDraws', 'paletteSkipped', 'draws', 'states', 'uniforms', 'textures', 'visible', 'animated', 'rigParts', 'parts', 'paletteBytes', 'vertices', 'paths', 'expanded'):
            print(f'{key}: mean={statistics.mean(int(r[key]) for r in selected):.1f} max={max(int(r[key]) for r in selected)}')
        # This is an application cadence estimate, not a runtime/compositor counter.
        missed = sum(max(0, round(float(r['cadenceUs']) / float(r['periodUs'])) - 1)
                     for previous, r in zip(rows, rows[1:])
                     if predicate(previous) and predicate(r)
                     and int(r['serial']) == int(previous['serial']) + 1
                     and float(r['periodUs']) > 0 and float(r['cadenceUs']) > 0)
        print(f'estimated missed display intervals={missed}; compositor/reprojection counts unavailable')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path, nargs='+')
    args = parser.parse_args()
    for path in args.captures:
        report(path)


if __name__ == '__main__':
    main()
