#!/usr/bin/env python3
"""Validate selection benchmark matrices and report medians, including regressions."""
import argparse
import csv
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('files', nargs='+', type=Path)
args = parser.parse_args()
writer = csv.writer(sys.stdout, lineterminator='\n')
writer.writerow(['type', 'n', 'distribution', 'k', 'operation', 'radixkit_ms', 'std_ms', 'speedup', 'status'])
for path in args.files:
    groups = defaultdict(dict)
    sizes = set()
    for row in csv.DictReader(path.open()):
        if row['method'] == 'fast32':
            row['method'] = 'radixkit' # Historical CSV compatibility.
        n, k, op, repeat = (int(row[c]) for c in ('n', 'k', 'operation', 'repeat'))
        ms = float(row['ms'])
        if n < 100 or not math.isfinite(ms) or ms < 0:
            raise ValueError(f'Invalid measurement in {path}')
        sizes.add(n)
        key = (row['type'], n, row['distribution'], k, op, row['method'])
        if repeat in groups[key]:
            raise ValueError(f'Duplicate trial in {path}: {key}, {repeat}')
        groups[key][repeat] = ms
    if len(sizes) != 1:
        raise ValueError(f'Expected one input size in {path}')
    n = sizes.pop()
    expected = {(t, n, d, k, op, method)
                for t in ('u32', 'u64', 'kv32', 'kv64')
                for d in ('uniform', 'duplicates') for k in (1, n//100, n//2, n)
                for op in range(3) for method in ('radixkit', 'std')}
    if set(groups) != expected or any(set(v) != set(range(5)) for v in groups.values()):
        raise ValueError(f'Incomplete matrix in {path}')
    for key in sorted(expected):
        if key[-1] != 'radixkit':
            continue
        t, n, d, k, op, _ = key
        ours = statistics.median(groups[key].values())
        baseline = statistics.median(groups[key[:-1] + ('std',)].values())
        noop = k == n and op != 1
        ratio = baseline / ours if ours and not noop else ''
        status = 'no-op' if noop else ('regression >10%' if ours > baseline * 1.1 else '')
        writer.writerow([t, n, d, k, ('nth_element', 'partial_sort', 'top_k')[op],
                         ours, baseline, ratio, status])
