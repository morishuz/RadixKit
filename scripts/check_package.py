#!/usr/bin/env python3
"""Verify relocated find_package and add_subdirectory consumers of radixkit."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    source = args.source.resolve()
    with tempfile.TemporaryDirectory(prefix='radixkit-package-') as temporary:
        work = Path(temporary)
        build, prefix = work / 'library-build', work / 'original-prefix'
        run('cmake', '-S', source, '-B', build, '-DCMAKE_BUILD_TYPE=Release',
            '-DRADIXKIT_BUILD_TESTS=OFF', '-DRADIXKIT_BUILD_EXAMPLES=OFF')
        run('cmake', '--install', build, '--prefix', prefix)
        relocated = work / 'relocated-prefix'
        prefix.rename(relocated)
        license_file = relocated / "share/licenses/radixkit/LICENSE"
        if license_file.read_bytes() != (source / "LICENSE").read_bytes():
            raise RuntimeError("Installed MIT license differs")
        for header in (source / 'include/radixkit').rglob('*.hpp'):
            installed = relocated / header.relative_to(source)
            if header.read_bytes() != installed.read_bytes():
                raise RuntimeError(f'Installed header differs: {header.name}')
        for name, option in [('installed', f'-DCMAKE_PREFIX_PATH={relocated}'),
                             ('embedded', f'-DRADIXKIT_SOURCE_DIR={source}')]:
            consumer = work / name
            run('cmake', '-S', source / 'tests/consumer', '-B', consumer,
                '-DCMAKE_BUILD_TYPE=Release', option)
            run('cmake', '--build', consumer, '--config', 'Release', '--parallel', '2')
            run('ctest', '--test-dir', consumer, '--build-config', 'Release', '--output-on-failure')
    print('Relocated install and embedded consumers passed.')


if __name__ == '__main__':
    main()
