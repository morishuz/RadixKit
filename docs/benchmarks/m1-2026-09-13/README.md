# Provisional full-sort benchmarks: Apple M1

Measured 2026-09-13 on commit `8174d895cf7f89d15ed7be364b85ff58f1b4dc1f`.
These results describe this machine and these synthetic inputs, not a portable
performance guarantee. No library or benchmark algorithms were changed for this run.

## Environment and measurement

- MacBook Pro (MacBookPro17,1), Apple M1, 16 GB RAM; macOS 26.6.2 (25G83).
- Apple Clang 21.0.0 (clang-2100.1.1.101), libc++, arm64; CMake 3.30.
- Release: `-O3 -DNDEBUG -std=gnu++20 -march=native`, macOS 26.5 SDK.
  All competitors compiled into the same executable with the same flags.
- One sorting thread. The user closed other applications; runs executed serially
  with no concurrent builds. CPU affinity, clock frequency and thermal state were
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
| 32-bit keys | Uniform random | 2.645 | 12.452 | 4.236 | 20.861 | 1.60× |
| 32-bit keys | 16-bit range | 2.122 | 8.610 | 8.265 | 18.114 | 3.90× |
| 32-bit keys | 99% zeros | 2.794 | 12.933 | 11.202 | 0.877 | 4.01× |
| 32-bit keys | All equal | 1.182 | 8.576 | 11.092 | 1.276 | 7.26× |
| 32-bit keys | Ascending | 6.428 | 13.702 | 12.661 | 0.965 | 1.97× |
| 32-bit keys | Descending | 6.441 | 14.815 | 12.650 | 1.655 | 1.96× |
| 32-bit keys | 8 sparse bits | 3.265 | 9.634 | 6.531 | 8.073 | 2.00× |
| 32-bit keys | Nearly sorted | 6.448 | 15.912 | 12.533 | 2.750 | 1.94× |
| 64-bit keys | Uniform random | 7.231 | 12.643 | 12.752 | 21.637 | 1.75× |
| 64-bit keys | 16-bit range | 4.082 | 17.467 | 17.459 | 18.911 | 4.28× |
| 64-bit keys | 99% zeros | 0.396 | 21.410 | 21.475 | 0.955 | 54.11× |
| 64-bit keys | All equal | 0.167 | 17.266 | 17.189 | 1.301 | 103.08× |
| 64-bit keys | Ascending | 0.639 | 22.559 | 22.405 | 0.957 | 35.05× |
| 64-bit keys | Descending | 0.867 | 23.421 | 23.414 | 1.681 | 27.01× |
| 64-bit keys | 8 sparse bits | 12.606 | 30.451 | 30.452 | 8.342 | 2.42× |
| 64-bit keys | Nearly sorted | 9.591 | 24.725 | 24.833 | 2.963 | 2.58× |
| 32-bit keys + 64-bit payloads | Uniform random | 9.040 | 13.466 | 7.444 | 56.855 | 0.82× |
| 32-bit keys + 64-bit payloads | 16-bit range | 5.400 | 9.712 | 9.782 | 49.700 | 1.80× |
| 32-bit keys + 64-bit payloads | 99% zeros | 8.895 | 13.475 | 11.159 | 67.185 | 1.25× |
| 32-bit keys + 64-bit payloads | All equal | 1.254 | 8.790 | 11.197 | 0.809 | 7.01× |
| 32-bit keys + 64-bit payloads | Ascending | 13.141 | 14.117 | 15.011 | 0.928 | 1.07× |
| 32-bit keys + 64-bit payloads | Descending | 13.222 | 15.716 | 14.930 | 1.287 | 1.13× |
| 32-bit keys + 64-bit payloads | 8 sparse bits | 6.282 | 10.672 | 7.438 | 23.207 | 1.18× |
| 32-bit keys + 64-bit payloads | Nearly sorted | 13.115 | 16.725 | 14.830 | 2.718 | 1.13× |
| 64-bit keys + 64-bit payloads | Uniform random | 12.400 | 13.621 | 13.590 | 56.333 | 1.10× |
| 64-bit keys + 64-bit payloads | 16-bit range | 5.606 | 18.485 | 18.522 | 49.987 | 3.30× |
| 64-bit keys + 64-bit payloads | 99% zeros | 0.459 | 21.837 | 22.025 | 65.596 | 47.60× |
| 64-bit keys + 64-bit payloads | All equal | 0.348 | 17.334 | 17.294 | 1.054 | 49.74× |
| 64-bit keys + 64-bit payloads | Ascending | 0.626 | 22.785 | 22.789 | 1.074 | 36.40× |
| 64-bit keys + 64-bit payloads | Descending | 1.041 | 24.392 | 24.384 | 1.522 | 23.42× |
| 64-bit keys + 64-bit payloads | 8 sparse bits | 13.768 | 31.632 | 31.714 | 23.252 | 2.30× |
| 64-bit keys + 64-bit payloads | Nearly sorted | 10.254 | 25.514 | 25.403 | 2.957 | 2.48× |

### 100,000 elements

| Type | Distribution | RadixKit | `ska_sort` | `ska_sort_copy` | `std::sort` | vs best ska |
|---|---|---:|---:|---:|---:|---:|
| 32-bit keys | Uniform random | 0.253 | 1.416 | 0.378 | 1.882 | 1.49× |
| 32-bit keys | 16-bit range | 0.186 | 1.210 | 0.783 | 1.859 | 4.20× |
| 32-bit keys | 99% zeros | 0.274 | 1.271 | 1.077 | 0.081 | 3.93× |
| 32-bit keys | All equal | 0.115 | 0.873 | 1.084 | 0.128 | 7.60× |
| 32-bit keys | Ascending | 0.654 | 1.368 | 0.747 | 0.094 | 1.14× |
| 32-bit keys | Descending | 0.659 | 1.414 | 0.750 | 0.166 | 1.14× |
| 32-bit keys | 8 sparse bits | 0.319 | 0.971 | 0.644 | 0.809 | 2.02× |
| 32-bit keys | Nearly sorted | 0.655 | 1.668 | 0.736 | 0.275 | 1.12× |
| 64-bit keys | Uniform random | 0.527 | 1.407 | 1.391 | 1.917 | 2.64× |
| 64-bit keys | 16-bit range | 0.375 | 2.099 | 2.106 | 1.907 | 5.59× |
| 64-bit keys | 99% zeros | 0.044 | 2.145 | 2.139 | 0.092 | 48.48× |
| 64-bit keys | All equal | 0.016 | 1.741 | 1.737 | 0.128 | 109.40× |
| 64-bit keys | Ascending | 0.063 | 2.280 | 2.263 | 0.094 | 35.90× |
| 64-bit keys | Descending | 0.083 | 2.307 | 2.280 | 0.164 | 27.63× |
| 64-bit keys | 8 sparse bits | 1.225 | 3.390 | 3.415 | 0.848 | 2.77× |
| 64-bit keys | Nearly sorted | 0.893 | 2.534 | 2.539 | 0.292 | 2.84× |
| 32-bit keys + 64-bit payloads | Uniform random | 0.487 | 1.533 | 0.424 | 4.592 | 0.87× |
| 32-bit keys + 64-bit payloads | 16-bit range | 0.373 | 1.297 | 0.826 | 4.602 | 2.21× |
| 32-bit keys + 64-bit payloads | 99% zeros | 0.826 | 1.277 | 1.062 | 3.832 | 1.29× |
| 32-bit keys + 64-bit payloads | All equal | 0.120 | 0.866 | 1.060 | 0.067 | 7.22× |
| 32-bit keys + 64-bit payloads | Ascending | 1.150 | 1.382 | 1.110 | 0.067 | 0.97× |
| 32-bit keys + 64-bit payloads | Descending | 1.157 | 1.476 | 1.089 | 0.114 | 0.94× |
| 32-bit keys + 64-bit payloads | 8 sparse bits | 0.550 | 1.011 | 0.703 | 2.378 | 1.28× |
| 32-bit keys + 64-bit payloads | Nearly sorted | 1.163 | 1.761 | 1.104 | 0.259 | 0.95× |
| 64-bit keys + 64-bit payloads | Uniform random | 0.679 | 1.506 | 1.507 | 4.673 | 2.22× |
| 64-bit keys + 64-bit payloads | 16-bit range | 0.449 | 2.202 | 2.166 | 4.612 | 4.82× |
| 64-bit keys + 64-bit payloads | 99% zeros | 0.045 | 2.163 | 2.154 | 3.764 | 48.04× |
| 64-bit keys + 64-bit payloads | All equal | 0.019 | 1.735 | 1.730 | 0.094 | 92.28× |
| 64-bit keys + 64-bit payloads | Ascending | 0.061 | 2.287 | 2.233 | 0.097 | 36.56× |
| 64-bit keys + 64-bit payloads | Descending | 0.094 | 2.336 | 2.375 | 0.142 | 24.89× |
| 64-bit keys + 64-bit payloads | 8 sparse bits | 1.288 | 3.480 | 3.477 | 2.407 | 2.70× |
| 64-bit keys + 64-bit payloads | Nearly sorted | 0.948 | 2.612 | 2.611 | 0.286 | 2.75× |

### 10,000 elements

| Type | Distribution | RadixKit | `ska_sort` | `ska_sort_copy` | `std::sort` | vs best ska |
|---|---|---:|---:|---:|---:|---:|
| 32-bit keys | Uniform random | 0.026 | 0.128 | 0.035 | 0.161 | 1.34× |
| 32-bit keys | 16-bit range | 0.023 | 0.189 | 0.079 | 0.169 | 3.45× |
| 32-bit keys | 99% zeros | 0.084 | 0.124 | 0.110 | 0.008 | 1.31× |
| 32-bit keys | All equal | 0.013 | 0.092 | 0.115 | 0.013 | 7.18× |
| 32-bit keys | Ascending | 0.023 | 0.115 | 0.073 | 0.010 | 3.18× |
| 32-bit keys | Descending | 0.023 | 0.125 | 0.073 | 0.017 | 3.17× |
| 32-bit keys | 8 sparse bits | 0.037 | 0.194 | 0.066 | 0.106 | 1.75× |
| 32-bit keys | Nearly sorted | 0.023 | 0.129 | 0.072 | 0.033 | 3.18× |
| 64-bit keys | Uniform random | 0.051 | 0.124 | 0.120 | 0.172 | 2.35× |
| 64-bit keys | 16-bit range | 0.038 | 0.263 | 0.264 | 0.171 | 6.95× |
| 64-bit keys | 99% zeros | 0.004 | 0.216 | 0.219 | 0.009 | 50.44× |
| 64-bit keys | All equal | 0.002 | 0.180 | 0.178 | 0.013 | 103.98× |
| 64-bit keys | Ascending | 0.006 | 0.207 | 0.206 | 0.010 | 32.31× |
| 64-bit keys | Descending | 0.008 | 0.208 | 0.207 | 0.017 | 26.21× |
| 64-bit keys | 8 sparse bits | 0.130 | 0.410 | 0.410 | 0.108 | 3.16× |
| 64-bit keys | Nearly sorted | 0.038 | 0.215 | 0.215 | 0.035 | 5.69× |
| 32-bit keys + 64-bit payloads | Uniform random | 0.051 | 0.138 | 0.037 | 0.341 | 0.73× |
| 32-bit keys + 64-bit payloads | 16-bit range | 0.038 | 0.184 | 0.077 | 0.339 | 2.04× |
| 32-bit keys + 64-bit payloads | 99% zeros | 0.083 | 0.127 | 0.106 | 0.289 | 1.28× |
| 32-bit keys + 64-bit payloads | All equal | 0.012 | 0.087 | 0.109 | 0.007 | 6.99× |
| 32-bit keys + 64-bit payloads | Ascending | 0.040 | 0.115 | 0.085 | 0.007 | 2.12× |
| 32-bit keys + 64-bit payloads | Descending | 0.041 | 0.127 | 0.085 | 0.011 | 2.10× |
| 32-bit keys + 64-bit payloads | 8 sparse bits | 0.057 | 0.196 | 0.069 | 0.229 | 1.21× |
| 32-bit keys + 64-bit payloads | Nearly sorted | 0.041 | 0.136 | 0.088 | 0.034 | 2.13× |
| 64-bit keys + 64-bit payloads | Uniform random | 0.067 | 0.129 | 0.129 | 0.345 | 1.92× |
| 64-bit keys + 64-bit payloads | 16-bit range | 0.042 | 0.269 | 0.262 | 0.345 | 6.21× |
| 64-bit keys + 64-bit payloads | 99% zeros | 0.004 | 0.192 | 0.191 | 0.281 | 42.89× |
| 64-bit keys + 64-bit payloads | All equal | 0.002 | 0.170 | 0.175 | 0.009 | 85.06× |
| 64-bit keys + 64-bit payloads | Ascending | 0.006 | 0.199 | 0.199 | 0.010 | 32.03× |
| 64-bit keys + 64-bit payloads | Descending | 0.009 | 0.217 | 0.213 | 0.014 | 22.42× |
| 64-bit keys + 64-bit payloads | 8 sparse bits | 0.131 | 0.443 | 0.442 | 0.240 | 3.37× |
| 64-bit keys + 64-bit payloads | Nearly sorted | 0.047 | 0.218 | 0.216 | 0.036 | 4.60× |

## Reproduce

From the repository root:

```sh
cmake -S . -B build-m1-public -DCMAKE_BUILD_TYPE=Release \
  -DRADIXKIT_BUILD_BENCHMARKS=ON -DRADIXKIT_BUILD_TESTS=OFF \
  -DRADIXKIT_BUILD_EXAMPLES=OFF -DRADIXKIT_NATIVE_BENCHMARKS=ON
cmake --build build-m1-public --target radixkit_sort_bench --parallel 2
mkdir -p build-m1-public/results
```

Run the following Python snippet after the build has finished. It preserves the
measurement order and stops on a benchmark or validation error:

```python
import subprocess
from pathlib import Path

out = Path("build-m1-public/results")
for seed, sizes in [(13579, [10000, 100000, 1000000]),
                    (24680, [1000000, 100000, 10000]),
                    (97531, [100000, 10000, 1000000])]:
    for n in sizes:
        with (out / f"sort-{n}-{seed}.csv").open("w") as output:
            subprocess.run(["./build-m1-public/radixkit_sort_bench",
                            str(n), "9", str(seed)], stdout=output, check=True)
```

```sh
python3 scripts/summarize_sort.py build-m1-public/results/sort-*.csv > build-m1-public/summary.csv
```

Archived [raw trials](trials.csv) and [per-case summary](summary.csv) include all
formats, sizes and policies. Ratios use unrounded medians. See the general
[benchmark guide](../../BENCHMARKING.md) for fairness and interpretation.
