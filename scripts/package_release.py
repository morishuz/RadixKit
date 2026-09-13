#!/usr/bin/env python3
"""Create a deterministic, dependency-free source archive from an explicit allowlist."""
import argparse
import gzip
import hashlib
from pathlib import Path
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build-dist/radixkit-source.tar.gz'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    files = {Path(name) for name in (
        'LICENSE', 'README.md', 'CHANGELOG.md', 'THIRD_PARTY_NOTICES.md',
        'CMakeLists.txt', 'cmake/radixkitConfig.cmake.in',
        'scripts/check_package.py', 'scripts/package_release.py',
        'scripts/check_adaptive_mutations.py', 'scripts/summarize_selection.py',
        'scripts/audit_release.py', '.gitignore', '.clang-format')}
    for directory, suffixes in [('include', {'.hpp'}), ('tests', {'.cpp', '.txt'}),
                                ('examples', {'.cpp'}), ('docs', {'.md'}), ('bench', {'.cpp'}),
                                ('third_party/ska_sort', {'.hpp', '.md', '.txt'})]:
        files.update(p.relative_to(root) for p in (root / directory).rglob('*')
                     if p.is_file() and p.suffix in suffixes)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('wb') as raw, gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=0) as zipped:
        with tarfile.open(fileobj=zipped, mode='w') as archive:
            for relative in sorted(files):
                path = root / relative
                if path.is_symlink():
                    raise RuntimeError(f'Refusing symlink: {relative}')
                info = archive.gettarinfo(str(path), arcname=str(Path('radixkit') / relative))
                info.uid = info.gid = info.mtime = 0
                info.uname = info.gname = ''
                info.mode = 0o644
                with path.open('rb') as contents:
                    archive.addfile(info, contents)
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    print(f'{len(files)} files, {args.output.stat().st_size} bytes: {args.output}')
    print(f'SHA256 {digest}')


if __name__ == '__main__':
    main()
