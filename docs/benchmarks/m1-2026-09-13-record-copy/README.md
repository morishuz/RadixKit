# Provisional full-sort benchmarks: Apple M1, improved record copying

Measured 2026-09-13 after the uint32 record-copy optimization. The measured
library is commit `109042a4cfd54e69d28f00b5edefc0314c785e99` plus the
[source patch](source.patch). The optimization was uncommitted at measurement time;
[source SHA256 hashes](source-sha256.csv) identify all headers, the benchmark,
vendored ska_sort, CMake configuration and summary script. No kernel, benchmark or
compiler-option changes were made during these runs. To recreate the source from
the base commit, apply the patch in an isolated checkout; do not reapply it to a
checkout that already includes the optimization.

These results describe this machine and these synthetic inputs, not a portable
performance guarantee. The [previous measurement](../m1-2026-09-13/README.md)
is preserved with its original raw trials. Comparing separate sessions does not
isolate the code change from timing variation; this report measures all competitors
afresh in the same executable.

## Environment and measurement

- MacBook Pro (MacBookPro17,1), Apple M1, 16 GB RAM; macOS 26.6.2 (25G83).
- Apple Clang 21.0.0 (clang-2100.1.1.101), libc++, arm64; CMake 3.30.
- Release: `-O3 -DNDEBUG -std=gnu++20 -march=native`, macOS 26.5 SDK.
  All competitors compiled into the same executable with the same flags.
- One sorting thread. Runs executed serially with no concurrent builds or other
  agent benchmarks. Background application state was not independently verified. CPU affinity, clock frequency and thermal state were
  not controlled or measured. Background OS activity may remain.
- Sizes: 10,000, 100,000 and 1,000,000 elements. Seeds: 13579, 24680, 97531.
  Each process performs one warmup and nine measured repetitions per case/method.
  Method order rotates. Size order varies between seeds as shown below.
- Each reported time is the median of the three per-seed medians (27 trials).
  The summary CSV also retains the minimum/maximum seed medians, which are
  descriptive variability measures, not confidence intervals.
- Input copying and validation are outside timing; required scratch allocation,
  copy-back and deallocation are inside timing. Fresh copies favor warm inputs.
  Every result was checked against sorted reference keys; record payload identities
  and associations were checked in full. All 11,664 timed results passed.
- Tables consistently use default `radixkit::sort`. The optional adaptive policy
  was also measured for plain keys and is included in the CSVs; it is not substituted
  when it is faster. This is a full-sort benchmark, not a selection benchmark.
- ska_sort revision `2c14d4bf7b667032ed28a481065f721385e33849`, with the two
  documented C++20 compatibility substitutions. Its `ska_sort_copy` may internally
  fall back to `ska_sort`, notably for wide keys. Both variants are shown.

Times below are milliseconds; lower is better. **vs best ska** is the smaller ska
median divided by the RadixKit median. Below 1 means RadixKit loses. This ratio does
not imply RadixKit beats `std::sort`; compare that column separately.

## Inputs and memory

Uniform uses full-width `mt19937_64` values. The 16-bit range masks keys to 0–65,535.
99% zeros places a random key at every hundredth position (a periodic pattern,
not a random choice of nonzero positions). All equal uses key 7. Ascending and
descending use consecutive keys. Sparse inputs vary eight fixed, evenly spaced
bit positions, giving at most 256 distinct keys. Nearly sorted starts ascending
and swaps 32 pairs; this fixed swap count is proportionally smaller at larger N.
Seeds do not change the equal/ascending/descending inputs, so their three runs
measure repeated timings rather than three distinct datasets.

Record payloads are unique 64-bit IDs. Both record layouts occupy 16 bytes on this
ABI; plain keys occupy 4 or 8 bytes. At one million elements this is 4, 8 or 16 MB
of input respectively (decimal MB). RadixKit has O(N) worst-case heap scratch plus
fixed histograms; `ska_sort_copy`'s adapter always allocates one N-element buffer.
`std::sort` does not need an N-element scratch array. These are not equal-memory
algorithms. Peak RSS and allocation counts were not measured; see the
[API memory contract](../../API.md) for RadixKit's stack and heap qualifications.

## Results

### 1,000,000 elements

| Type | Distribution | RadixKit | `ska_sort` | `ska_sort_copy` | `std::sort` | vs best ska |
|---|---|---:|---:|---:|---:|---:|
| 32-bit keys | Uniform random | 2.637 | 12.279 | 4.271 | 20.823 | 1.62× |
| 32-bit keys | 16-bit range | 2.018 | 8.720 | 8.257 | 18.273 | 4.09× |
| 32-bit keys | 99% zeros | 2.757 | 12.814 | 11.098 | 0.884 | 4.03× |
| 32-bit keys | All equal | 1.163 | 8.596 | 11.094 | 1.277 | 7.39× |
| 32-bit keys | Ascending | 6.438 | 13.691 | 12.601 | 0.951 | 1.96× |
| 32-bit keys | Descending | 6.462 | 14.728 | 12.538 | 1.663 | 1.94× |
| 32-bit keys | 8 sparse bits | 3.280 | 9.718 | 6.491 | 8.040 | 1.98× |
| 32-bit keys | Nearly sorted | 6.463 | 15.941 | 12.577 | 2.788 | 1.95× |
| 64-bit keys | Uniform random | 6.552 | 12.614 | 12.477 | 21.921 | 1.90× |
| 64-bit keys | 16-bit range | 4.043 | 17.265 | 17.370 | 18.873 | 4.27× |
| 64-bit keys | 99% zeros | 0.388 | 21.363 | 21.391 | 0.950 | 55.12× |
| 64-bit keys | All equal | 0.166 | 17.111 | 17.084 | 1.283 | 103.05× |
| 64-bit keys | Ascending | 0.629 | 22.365 | 22.502 | 0.978 | 35.55× |
| 64-bit keys | Descending | 0.876 | 23.361 | 23.581 | 1.716 | 26.67× |
| 64-bit keys | 8 sparse bits | 12.720 | 30.418 | 30.319 | 8.325 | 2.38× |
| 64-bit keys | Nearly sorted | 9.601 | 24.658 | 24.662 | 2.942 | 2.57× |
| 32-bit keys + 64-bit payloads | Uniform random | 6.225 | 13.459 | 7.146 | 56.954 | 1.15× |
| 32-bit keys + 64-bit payloads | 16-bit range | 3.761 | 9.599 | 9.419 | 49.670 | 2.50× |
| 32-bit keys + 64-bit payloads | 99% zeros | 8.617 | 13.183 | 10.914 | 65.143 | 1.27× |
| 32-bit keys + 64-bit payloads | All equal | 1.228 | 8.760 | 10.870 | 0.734 | 7.13× |
| 32-bit keys + 64-bit payloads | Ascending | 8.967 | 14.173 | 14.883 | 0.802 | 1.58× |
| 32-bit keys + 64-bit payloads | Descending | 8.897 | 15.685 | 14.809 | 1.259 | 1.66× |
| 32-bit keys + 64-bit payloads | 8 sparse bits | 4.281 | 10.687 | 7.422 | 23.084 | 1.73× |
| 32-bit keys + 64-bit payloads | Nearly sorted | 9.064 | 17.299 | 14.818 | 2.742 | 1.63× |
| 64-bit keys + 64-bit payloads | Uniform random | 12.364 | 13.815 | 13.608 | 56.436 | 1.10× |
| 64-bit keys + 64-bit payloads | 16-bit range | 5.356 | 18.356 | 18.141 | 49.849 | 3.39× |
| 64-bit keys + 64-bit payloads | 99% zeros | 0.442 | 21.899 | 21.975 | 65.145 | 49.60× |
| 64-bit keys + 64-bit payloads | All equal | 0.304 | 17.413 | 17.453 | 1.067 | 57.33× |
| 64-bit keys + 64-bit payloads | Ascending | 0.635 | 22.819 | 22.669 | 1.047 | 35.71× |
| 64-bit keys + 64-bit payloads | Descending | 1.008 | 24.294 | 24.356 | 1.533 | 24.09× |
| 64-bit keys + 64-bit payloads | 8 sparse bits | 13.678 | 31.674 | 31.539 | 23.315 | 2.31× |
| 64-bit keys + 64-bit payloads | Nearly sorted | 10.227 | 25.304 | 25.270 | 3.000 | 2.47× |

### 100,000 elements

| Type | Distribution | RadixKit | `ska_sort` | `ska_sort_copy` | `std::sort` | vs best ska |
|---|---|---:|---:|---:|---:|---:|
| 32-bit keys | Uniform random | 0.255 | 1.425 | 0.379 | 1.873 | 1.49× |
| 32-bit keys | 16-bit range | 0.193 | 1.240 | 0.801 | 1.882 | 4.14× |
| 32-bit keys | 99% zeros | 0.276 | 1.281 | 1.111 | 0.081 | 4.03× |
| 32-bit keys | All equal | 0.115 | 0.877 | 1.083 | 0.125 | 7.63× |
| 32-bit keys | Ascending | 0.669 | 1.382 | 0.754 | 0.096 | 1.13× |
| 32-bit keys | Descending | 0.667 | 1.427 | 0.757 | 0.166 | 1.13× |
| 32-bit keys | 8 sparse bits | 0.330 | 0.975 | 0.650 | 0.823 | 1.97× |
| 32-bit keys | Nearly sorted | 0.664 | 1.665 | 0.757 | 0.278 | 1.14× |
| 64-bit keys | Uniform random | 0.519 | 1.420 | 1.402 | 1.937 | 2.70× |
| 64-bit keys | 16-bit range | 0.375 | 2.142 | 2.133 | 1.940 | 5.69× |
| 64-bit keys | 99% zeros | 0.042 | 2.156 | 2.179 | 0.090 | 51.13× |
| 64-bit keys | All equal | 0.016 | 1.746 | 1.747 | 0.128 | 108.83× |
| 64-bit keys | Ascending | 0.061 | 2.233 | 2.286 | 0.097 | 36.30× |
| 64-bit keys | Descending | 0.084 | 2.326 | 2.287 | 0.169 | 27.21× |
| 64-bit keys | 8 sparse bits | 1.238 | 3.352 | 3.380 | 0.838 | 2.71× |
| 64-bit keys | Nearly sorted | 0.929 | 2.545 | 2.534 | 0.293 | 2.73× |
| 32-bit keys + 64-bit payloads | Uniform random | 0.347 | 1.558 | 0.430 | 4.638 | 1.24× |
| 32-bit keys + 64-bit payloads | 16-bit range | 0.264 | 1.317 | 0.816 | 4.617 | 3.09× |
| 32-bit keys + 64-bit payloads | 99% zeros | 0.798 | 1.286 | 1.078 | 3.820 | 1.35× |
| 32-bit keys + 64-bit payloads | All equal | 0.120 | 0.878 | 1.066 | 0.068 | 7.28× |
| 32-bit keys + 64-bit payloads | Ascending | 0.807 | 1.405 | 1.082 | 0.069 | 1.34× |
| 32-bit keys + 64-bit payloads | Descending | 0.819 | 1.500 | 1.095 | 0.116 | 1.34× |
| 32-bit keys + 64-bit payloads | 8 sparse bits | 0.386 | 1.027 | 0.712 | 2.435 | 1.84× |
| 32-bit keys + 64-bit payloads | Nearly sorted | 0.797 | 1.729 | 1.098 | 0.259 | 1.38× |
| 64-bit keys + 64-bit payloads | Uniform random | 0.686 | 1.549 | 1.527 | 4.726 | 2.23× |
| 64-bit keys + 64-bit payloads | 16-bit range | 0.446 | 2.211 | 2.191 | 4.603 | 4.91× |
| 64-bit keys + 64-bit payloads | 99% zeros | 0.045 | 2.184 | 2.180 | 3.784 | 48.45× |
| 64-bit keys + 64-bit payloads | All equal | 0.019 | 1.743 | 1.735 | 0.094 | 92.31× |
| 64-bit keys + 64-bit payloads | Ascending | 0.062 | 2.243 | 2.274 | 0.097 | 35.91× |
| 64-bit keys + 64-bit payloads | Descending | 0.090 | 2.363 | 2.346 | 0.143 | 26.16× |
| 64-bit keys + 64-bit payloads | 8 sparse bits | 1.314 | 3.447 | 3.446 | 2.428 | 2.62× |
| 64-bit keys + 64-bit payloads | Nearly sorted | 0.962 | 2.584 | 2.599 | 0.287 | 2.69× |

### 10,000 elements

| Type | Distribution | RadixKit | `ska_sort` | `ska_sort_copy` | `std::sort` | vs best ska |
|---|---|---:|---:|---:|---:|---:|
| 32-bit keys | Uniform random | 0.026 | 0.136 | 0.035 | 0.167 | 1.35× |
| 32-bit keys | 16-bit range | 0.022 | 0.180 | 0.080 | 0.166 | 3.55× |
| 32-bit keys | 99% zeros | 0.088 | 0.126 | 0.115 | 0.008 | 1.32× |
| 32-bit keys | All equal | 0.012 | 0.088 | 0.111 | 0.013 | 7.16× |
| 32-bit keys | Ascending | 0.024 | 0.119 | 0.074 | 0.010 | 3.14× |
| 32-bit keys | Descending | 0.024 | 0.127 | 0.074 | 0.017 | 3.16× |
| 32-bit keys | 8 sparse bits | 0.035 | 0.185 | 0.064 | 0.092 | 1.80× |
| 32-bit keys | Nearly sorted | 0.023 | 0.132 | 0.073 | 0.034 | 3.18× |
| 64-bit keys | Uniform random | 0.051 | 0.133 | 0.128 | 0.173 | 2.50× |
| 64-bit keys | 16-bit range | 0.038 | 0.259 | 0.262 | 0.172 | 6.85× |
| 64-bit keys | 99% zeros | 0.004 | 0.219 | 0.219 | 0.009 | 54.08× |
| 64-bit keys | All equal | 0.002 | 0.183 | 0.183 | 0.013 | 104.31× |
| 64-bit keys | Ascending | 0.006 | 0.203 | 0.205 | 0.010 | 32.11× |
| 64-bit keys | Descending | 0.008 | 0.213 | 0.214 | 0.017 | 26.33× |
| 64-bit keys | 8 sparse bits | 0.130 | 0.415 | 0.403 | 0.106 | 3.10× |
| 64-bit keys | Nearly sorted | 0.039 | 0.219 | 0.219 | 0.036 | 5.66× |
| 32-bit keys + 64-bit payloads | Uniform random | 0.036 | 0.143 | 0.037 | 0.351 | 1.02× |
| 32-bit keys + 64-bit payloads | 16-bit range | 0.028 | 0.187 | 0.078 | 0.341 | 2.79× |
| 32-bit keys + 64-bit payloads | 99% zeros | 0.082 | 0.127 | 0.108 | 0.283 | 1.32× |
| 32-bit keys + 64-bit payloads | All equal | 0.012 | 0.085 | 0.105 | 0.007 | 6.96× |
| 32-bit keys + 64-bit payloads | Ascending | 0.033 | 0.116 | 0.086 | 0.007 | 2.60× |
| 32-bit keys + 64-bit payloads | Descending | 0.033 | 0.127 | 0.085 | 0.011 | 2.59× |
| 32-bit keys + 64-bit payloads | 8 sparse bits | 0.041 | 0.187 | 0.070 | 0.228 | 1.71× |
| 32-bit keys + 64-bit payloads | Nearly sorted | 0.033 | 0.136 | 0.086 | 0.032 | 2.58× |
| 64-bit keys + 64-bit payloads | Uniform random | 0.069 | 0.138 | 0.128 | 0.349 | 1.87× |
| 64-bit keys + 64-bit payloads | 16-bit range | 0.043 | 0.272 | 0.265 | 0.353 | 6.19× |
| 64-bit keys + 64-bit payloads | 99% zeros | 0.004 | 0.202 | 0.196 | 0.290 | 49.58× |
| 64-bit keys + 64-bit payloads | All equal | 0.002 | 0.169 | 0.170 | 0.009 | 86.46× |
| 64-bit keys + 64-bit payloads | Ascending | 0.006 | 0.199 | 0.199 | 0.010 | 31.77× |
| 64-bit keys + 64-bit payloads | Descending | 0.009 | 0.215 | 0.215 | 0.014 | 24.30× |
| 64-bit keys + 64-bit payloads | 8 sparse bits | 0.132 | 0.438 | 0.423 | 0.242 | 3.20× |
| 64-bit keys + 64-bit payloads | Nearly sorted | 0.048 | 0.222 | 0.224 | 0.035 | 4.67× |

## Reproduce

From the repository root:

```sh
cmake -S . -B build-m1-record-copy -DCMAKE_BUILD_TYPE=Release \
  -DRADIXKIT_BUILD_BENCHMARKS=ON -DRADIXKIT_BUILD_TESTS=OFF \
  -DRADIXKIT_BUILD_EXAMPLES=OFF -DRADIXKIT_NATIVE_BENCHMARKS=ON
cmake --build build-m1-record-copy --target radixkit_sort_bench --parallel 2
mkdir -p build-m1-record-copy/results
```

Run the following Python snippet after the build has finished. It preserves the
measurement order and stops on a benchmark or validation error:

```python
import subprocess
from pathlib import Path

out = Path("build-m1-record-copy/results")
for seed, sizes in [(13579, [10000, 100000, 1000000]),
                    (24680, [1000000, 100000, 10000]),
                    (97531, [100000, 10000, 1000000])]:
    for n in sizes:
        with (out / f"sort-{n}-{seed}.csv").open("w") as output:
            subprocess.run(["./build-m1-record-copy/radixkit_sort_bench",
                            str(n), "9", str(seed)], stdout=output, check=True)
```

```sh
python3 scripts/summarize_sort.py build-m1-record-copy/results/sort-*.csv > build-m1-record-copy/summary.csv
```

Archived [raw trials](trials.csv) and [per-case summary](summary.csv) include all
formats, sizes and policies. Ratios use unrounded medians. See the general
[benchmark guide](../../BENCHMARKING.md) for fairness and interpretation.
