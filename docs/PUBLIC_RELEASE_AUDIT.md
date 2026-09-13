# Initial public release audit

This audit covers the lean RadixKit 0.1.0 source snapshot prepared on 2026-09-13.
It does not publish a repository or change repository visibility.

## Release scope

The tree contains the library, examples, correctness/stress tests, current
documentation, packaging tools and two optional benchmark runners. Historical
experiments, raw results, environment logs and database integration are excluded.
The complete development history was preserved separately before squashing.
No sorting algorithm or threshold was changed during this release cleanup.

The installed library is header-only C++20 and depends only on the standard
library. Optional benchmarks vendor one ska_sort revision; they do not fetch
network dependencies and are not installed with the library.

## Code and contracts

Reviewed the public entry points, key/record constraints, full/prefix dispatch,
near-end heap fallback and radix selection invariants. No new sorting defect was
identified. Differential tests validate ordering, both selection partitions and
complete permutations, including unique payload IDs and projected payload bits.

The documented limits remain material: operations are unstable; allocation failure
need not restore original order; extractors must be deterministic and non-mutating;
large stack tables and arrays above UINT32_MAX require care. Tiny/middle-rank
and adverse distributions can favor standard-library algorithms. No universal
performance claim or formal correctness proof is made.

## Licensing and privacy

- Project code has the MIT license and installed headers carry MIT SPDX notices.
  Original fast32 contributor attribution is preserved.
- The vendored ska_sort source retains its Boost Software License, copyright,
  upstream revision and documented C++20 substitutions. It is excluded from
  RadixKit's MIT grant; see [third-party notices](../THIRD_PARTY_NOTICES.md).
- Private remote addresses, local home/volume paths, historical environment logs
  and personal output files are excluded from the release tree.
- `scripts/audit_release.py` checks local Markdown links, header notices and
  selected credential/private-path patterns. It is a targeted check, not a
  guarantee that every possible secret or provenance issue can be detected.
- Squashing removes earlier commits from main's ancestry, not from the separately
  preserved backup. Git author metadata still uses the configured Git identity.
  A future public repository should receive main only, not a mirror of private refs.

## Validation

- All ten Release CTest suites passed with stress checks enabled.
- All ten ASan/UBSan suites passed.
- Full-sort benchmark smoke validation covered all four formats, eight
  distributions and both ska variants; selection benchmark outputs and its
  complete matrix validator passed. These smoke timings are not speed claims.
- Relocated find_package and add_subdirectory consumers passed, including from
  the extracted source archive. The archive built independently with optional
  benchmarks enabled and all seven default test suites passed.
- Local documentation links, installed SPDX notices and targeted privacy-pattern
  checks passed. Public headers are unchanged from the validated renamed kernels.

Hardware scope is Apple M1 with Apple Clang 21/libc++.
Windows, GCC/libstdc++, x86, MSan, small-stack workers and multi-billion-element
inputs remain unvalidated. These are limitations, not implicit release guarantees.
