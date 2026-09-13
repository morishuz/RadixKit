#!/usr/bin/env python3
"""Check that differential stress tests catch three deliberately broken copies.

Only build-stress-mutants/ is modified; installed/library sources stay intact.
"""
import os
from pathlib import Path
import shlex
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
work = root / 'build-stress-mutants'
logs = work / 'logs'
logs.mkdir(parents=True, exist_ok=True)
source = (root / 'include/radixkit/detail/adaptive_keys.hpp').read_text()
mutations = {
    'missing_mask_check': (
        'if (!valid_domain(values, first, mask)) return false;',
        '// Deliberate test mutation: omitted validation.'),
    'missing_repair_overlap': (
        'const auto start = index > budget ? index - budget : 0;',
        'const auto start = index; // Deliberately incorrect.'),
    'wrong_submask_order': (
        'deposited = (deposited - mask) & mask;',
        'deposited = (deposited + mask) & mask;'),
}
for name, (before, after) in mutations.items():
    if source.count(before) != 1:
        raise RuntimeError(f'Review mutation for changed source: {name}')
    directory = work / name
    headers = directory / 'include/radixkit'
    shutil.copytree(root / 'include/radixkit', headers, dirs_exist_ok=True)
    (headers / 'detail/adaptive_keys.hpp').write_text(source.replace(before, after))
    binary = directory / 'runner'
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + [
        '-std=c++20', '-O1', f'-I{directory / "include"}',
        str(root / 'tests/stress/adaptive_stress.cpp'), '-o', str(binary)], check=True)
    result = subprocess.run([
        str(binary), '--seed', '2026091104', '--cases', '2000', '--max-size', '65537',
        '--failure-prefix', str(directory / 'expected-failure')],
        cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (logs / f'mutant-{name}.log').write_text(result.stdout)
    if result.returncode != 1 or 'output differs from std::sort' not in result.stdout:
        raise RuntimeError(f'Mutant was not rejected by the differential oracle: {name}')
    print(f'Detected deliberate mutation: {name}')
