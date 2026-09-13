# Reproducible benchmarks

Benchmarks are optional and never add dependencies to installed RadixKit headers.
The full-sort runner vendors ska_sort at a fixed revision; there are no downloads.

See the [provisional Apple M1 results](benchmarks/m1-2026-09-13/README.md) for
measured timings, raw trials and exact reproduction details for the released code.

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DRADIXKIT_BUILD_BENCHMARKS=ON
cmake --build build-bench --target radixkit_sort_bench radixkit_selection_bench --parallel 2
./build-bench/radixkit_sort_bench 1000000 7 13579 > build-bench/sort.csv
python3 scripts/summarize_sort.py build-bench/sort.csv > build-bench/sort-summary.csv
./build-bench/radixkit_selection_bench 1000000 > build-bench/selection.csv
python3 scripts/summarize_selection.py build-bench/selection.csv > build-bench/selection-summary.csv
```

Full-sort arguments are input size (at least 128), measured repetitions and random seed. It tests
uint32, uint64, key_value32 and key_value64 across uniform, 16-bit-range, 99%-zero,
equal, ascending, descending, sparse-bit and nearly sorted inputs. Methods are
RadixKit default, std::sort, ska_sort and ska_sort_copy. Plain keys also test the
explicit adaptive policy. Both ska variants are retained because their strengths
differ; compare against the faster one for the case being discussed.

The ska_sort_copy adapter includes uninitialized scratch allocation, any required
copy-back, and deallocation in the timing. The pinned upstream implementation can
itself fall back to ska_sort for wide keys or tiny inputs. This is not a separate
optimized copy algorithm for every format. Naturally aligned key_value32 and
key_value64 records are both 16 bytes on the tested ABI.

Selection uses uniform and 16-value inputs, counts/ranks 1, 1%, 50% and N, one
warmup and five repetitions. The CSV operation field is 0 for nth_element, 1 for
partial_sort and 2 for largest-k selection. `k` is a zero-based rank for operation
0 and a count otherwise. Rank 1 is the second-smallest element, not the one-element
top-k case. Full-count nth/top-k calls are no-ops and must not be ranked by speed.

## Fairness and interpretation

- Every method receives the same freshly copied input; copying and correctness
  validation are outside timing. Required algorithm allocations are inside timing.
- Method order rotates/alternates between repetitions. Every method gets a warmup.
- Full outputs are checked against std::sort references; records also check every
  unique payload identifier and its original key. Selection checks both partitions
  and complete permutations, not just the selected value.
- The compiler, optimization flags, input layout and process are shared. Plain-key
  std::sort retains its arithmetic fast path. No method uses internal threads here.
- Use per-case medians and retain the raw trials. Test several sizes, seeds, both
  key widths and distributions. Include regressions and memory requirements.
- Run benchmarks serially, without builds or other heavy work. Freshly copied
  inputs tend to be warm; these tests do not establish cold-cache or database speedups.

`RADIXKIT_NATIVE_BENCHMARKS=ON` applies `-march=native` to all methods on supported
compilers. Record this setting and compiler/CPU details with results. The published
M1 table is provisional and specific to its measured workloads. Earlier research
informed the algorithms, but each user should reproduce their workload.
