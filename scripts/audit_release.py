#!/usr/bin/env python3
"""Check release files, local Markdown links, attribution and obvious secret/path leaks.

This is a targeted source-tree check, not a proof that no confidential data exists.
It does not inspect Git history or publish/change repository settings.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
roots = ('include', 'tests', 'examples', 'bench', 'cmake', 'docs', 'scripts', 'third_party/ska_sort')
files = {root / name for name in ('README.md', 'LICENSE', 'CHANGELOG.md',
                                 'THIRD_PARTY_NOTICES.md', 'CMakeLists.txt', '.gitignore', '.clang-format')}
for directory in roots:
    files.update(p for p in (root / directory).rglob('*') if p.is_file() and
                 '__pycache__' not in p.parts and p.suffix != '.pyc')
checks = {
    'private key': r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',
    'token': r'\b(?:ghp_|github_pat_|glpat-|sk-proj-)[A-Za-z0-9_-]{12,}|AKIA[0-9A-Z]{16}',
    'local home/volume path': r'/(?:Users|Volumes|home)/[A-Za-z0-9]',
    'repository address requiring review': r'git@[A-Za-z0-9.-]+:[A-Za-z0-9_./-]+|https?://gitlab\.com/',
}
errors = []
for path in sorted(files):
    relative = path.relative_to(root)
    if path.is_symlink():
        errors.append(f'{relative}: symlink is not allowed in the release sources')
        continue
    text = path.read_text()
    for label, pattern in checks.items():
        if re.search(pattern, text):
            errors.append(f'{relative}: matched {label} pattern')
    if path.suffix == '.md':
        prose = re.sub(r'^```[^\n]*\n.*?^```[ \t]*$', '', text, flags=re.M | re.S)
        for target in re.findall(r'\]\(([^)]+)\)', prose):
            target = target.strip('<>').split('#', 1)[0]
            if not target or re.match(r'[A-Za-z][A-Za-z0-9+.-]*:', target):
                continue
            if not (path.parent / target).exists():
                errors.append(f'{relative}: broken local link {target}')
    if 'include/radixkit/' in path.as_posix():
        if 'SPDX-License-Identifier: MIT' not in text:
            errors.append(f'{relative}: missing MIT SPDX notice')
        if re.search(r'namespace\s+fast32\b|#include\s*[<"]fast32/', text):
            errors.append(f'{relative}: old API name remains')
for required in ('LICENSE', 'third_party/ska_sort/LICENSE_1_0.txt'):
    if not (root / required).is_file():
        errors.append(f'Missing {required}')
if errors:
    print('\n'.join(errors), file=sys.stderr)
    sys.exit(1)
print(f'Checked {len(files)} release files: links, SPDX notices and targeted privacy patterns passed.')
