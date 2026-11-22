#!/usr/bin/env python3
"""Summarize FalloutQuest captures, separating active gameplay, waiting and GPU work."""
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


def gameplay(row):
    return row.get('loading') == '0' and row.get('blocked') == '0' and row.get('submitted') == '1' and row.get('focused', '1') == '1'


def diagnostic_summary(rows):
    selected = [r for r in rows if gameplay(r)]
    if not selected:
        print('No active gameplay frames. Keep the game focused and headset awake during capture.')
        return
    print('\nActive gameplay diagnosis (inclusive scopes; never add parent and child timings):')
    cpu_pairs = [(float(r['workUs']), float(r['threadCpuUs'])) for r in selected
                 if float(r.get('threadCpuUs', -1)) >= 0 and float(r['workUs']) > 0]
    if cpu_pairs:
        work = sum(w for w, _ in cpu_pairs)
        cpu = sum(c for _, c in cpu_pairs)
        print(f'Render-thread running time / elapsed work: {100 * cpu / work:.1f}%; '
              'remaining time includes blocking and OS scheduling, not measured GPU execution. '
              'Worker-thread CPU is not included.')
    excluded = {'workUs', 'threadCpuUs', 'cadenceUs', 'elapsedUs', 'periodUs', 'slowObjectUs'}
    phases = []
    for key in rows[0]:
        if not key.endswith('Us') or key.endswith('MaxUs') or key in excluded or key.startswith(('gpu', 'vk')):
            continue
        values = [float(r[key]) for r in selected if float(r[key]) >= 0]
        if values and max(values) > 0:
            phases.append((percentile(values, .95), key, values))
    print('Largest CPU scope durations: metric | median ms | p95 ms | max ms | mean calls/frame | longest single call ms')
    for p95, key, values in sorted(phases, reverse=True)[:15]:
        calls = [int(r[key + 'Calls']) for r in selected] if key + 'Calls' in rows[0] else []
        longest = max(float(r.get(key + 'MaxUs', -1)) for r in selected)
        print(f'{key} | {statistics.median(values)/1000:.3f} | {p95/1000:.3f} | '
              f'{max(values)/1000:.3f} | {statistics.mean(calls) if calls and max(calls) else "unavailable"} | '
              f'{longest/1000 if longest >= 0 else "unavailable"}')
    if 'vkSubmissions' in rows[0]:
        expected = sum(int(r['vkSubmissions']) for r in selected)
        completed = sum(int(r['vkCompleted']) for r in selected)
        print(f'Vulkan GPU timestamp coverage: {completed}/{expected} submissions. '
              'Incomplete app frames export -1. GPU pass samples require all submissions and no dropped timestamps.')
    print('Slowest gameplay frames: serial | elapsed s | cell/world | work ms | thread CPU ms | residents/draws | slow reference | slow object ms')
    for r in sorted(selected, key=lambda row: float(row['workUs']), reverse=True)[:10]:
        print(f'{r["serial"]} | {float(r.get("elapsedUs", 0))/1e6:.3f} | '
              f'{int(r["cell"]):08X}/{int(r["world"]):08X} | {float(r["workUs"])/1000:.3f} | '
              f'{float(r.get("threadCpuUs", -1))/1000:.3f} | {r["residents"]}/{r["draws"]} | '
              f'{int(r.get("slowObjectRef", 0)):08X} | {float(r.get("slowObjectUs", 0))/1000:.3f}')


def report(path):
    with path.open(encoding='utf-8-sig') as f:
        rows = list(csv.DictReader(line for line in f if not line.startswith('#')))
    if not rows:
        raise ValueError(f'{path}: no captured frames')
    print(path.read_text().splitlines()[0])
    diagnostic_summary(rows)
    invalid_gpu = sum(not valid_gpu_us(float(r[k])) and float(r[k]) >= 0
                      for r in rows for k in ('gpuEye0Us', 'gpuEye1Us', 'gpuEyesUs'))
    print(f'\n{path} ({len(rows)} frames; GPU excludes preparation; overlapping scopes are not additive)')
    if invalid_gpu:
        print(f'WARNING: excluded {invalid_gpu} invalid/wrapped GPU values; GPU bottleneck cannot be inferred from them.')
    for group, predicate in (
        ('loading', lambda r: r['loading'] == '1'),
        ('gameplay', gameplay),
        ('npc-light', lambda r: gameplay(r) and int(r['residents']) == 0),
        ('npc-present', lambda r: gameplay(r) and int(r['residents']) > 0),
        ('gpu-sampled-v232+', lambda r: gameplay(r) and 'gpu0_terrainUs' in r and int(r['serial'])%8==0),
        ('gpu-unsampled-v232+', lambda r: gameplay(r) and 'gpu0_terrainUs' in r and int(r['serial'])%8!=0),
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
              'GPU scopes overlap: post includes bloom, reflection includes mirrorTerrain; '
              'v233 HDR segments include world/water/resolve work. Individual draw queries are unreliable on tilers.')
        print('metric,n,mean_ms,median_ms,p95_ms,p99_ms,max_ms,over72_percent,over90_percent')
        for key in rows[0]:
            if not key.endswith('Us') or key in ('periodUs', 'elapsedUs'):
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
