#!/usr/bin/env python3
"""Summarize full-sort CSV trials using the median of per-seed medians."""
import argparse
import csv
import math
import statistics
import sys
from collections import defaultdict


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trials', nargs='+', help='CSV files from radixkit_sort_bench')
    args = parser.parse_args()
    groups = defaultdict(dict)
    for filename in args.trials:
        with open(filename, newline='') as source:
            for row in csv.DictReader(source):
                case = (row['type'], int(row['n']), row['distribution'], row['method'])
                trial = (int(row['seed']), int(row['repeat']))
                ms = float(row['ms'])
                if trial in groups[case] or not math.isfinite(ms) or ms <= 0:
                    raise ValueError(f'Duplicate or invalid trial: {case}, {trial}')
                groups[case][trial] = ms
    if not groups:
        raise ValueError('No trials found')
    writer = csv.writer(sys.stdout)
    writer.writerow(('type', 'n', 'distribution', 'method', 'seeds', 'trials',
                     'median_ms', 'min_seed_median_ms', 'max_seed_median_ms'))
    reference_trials = {}
    for case, trials in sorted(groups.items()):
        workload = case[:3]
        if set(trials) != reference_trials.setdefault(workload, set(trials)):
            raise ValueError(f'Methods have different seeds/repetitions: {workload}')
        by_seed = defaultdict(list)
        for (seed, _), ms in trials.items():
            by_seed[seed].append(ms)
        medians = [statistics.median(values) for values in by_seed.values()]
        writer.writerow((*case, len(medians), len(trials),
                         *(f'{value:.9f}' for value in
                           (statistics.median(medians), min(medians), max(medians)))))


if __name__ == '__main__':
    main()
