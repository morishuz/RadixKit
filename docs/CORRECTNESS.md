# Correctness validation

Run the ordinary and stress tests with:

```sh
cmake -S . -B build-check -DCMAKE_BUILD_TYPE=Release -DRADIXKIT_BUILD_STRESS_TESTS=ON
cmake --build build-check --parallel 2
ctest --test-dir build-check --output-on-failure
./build-check/radixkit_adaptive_stress --seed 2026091104 --cases 2000 --max-size 65537
```

Tests compare against full `std::sort` references, check both nth-element
partitions, verify selected prefixes, and check the complete input permutation.
Records have unique payload identifiers so a sorted key array alone cannot hide
lost or mismatched payloads. Projected integer records retain their payload bits.

Coverage includes exhaustive tiny arrays, empty/no-op/out-of-range boundaries,
all active-digit combinations, sparse keys, duplicates, adversarial samples,
radix/heap cutoff boundaries and heap-budget fallback. Allocation injection
checks that selection stays allocation-free and that sort failures preserve
records for retry. Rollback to the original order is not promised.

For Clang or GCC sanitizers, use a separate build:

```sh
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DRADIXKIT_BUILD_STRESS_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-O1 -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all"
cmake --build build-san --parallel 2
ctest --test-dir build-san --output-on-failure
```

`python3 scripts/check_adaptive_mutations.py` optionally checks that the stress
oracle catches three deliberately broken copies in an ignored build directory.
It does not edit the installed/library sources. `scripts/check_package.py`
checks both relocated installation and add_subdirectory consumers.

Tests reduce risk; they are not a proof of correctness. Current executed
validation is on Apple M1/Apple Clang. Windows, GCC/libstdc++, x86, MSan,
small-stack workers and arrays above UINT32_MAX elements remain unvalidated.
Extractor determinism and non-mutation are caller preconditions, as explained in
[the API contract](API.md).
