# Changelog

## 0.1.0

- Header-only C++20 sorting and selection for uint32/uint64 keys and records.
- `sort`, `nth_element`, `partial_sort` and `top_k`, with custom-key projections.
- Radix kernels with adaptive order, duplicate and key-range strategies, plus
  bounded heap selection for eligible near-end ranks.
- MIT licensing, CMake installation, examples and reproducible source packaging.
- Differential, exhaustive, payload-preservation, allocation-failure and stress
  tests, plus optional standard-library and ska_sort benchmarks.

Developed under the working name fast32; the released namespace is `radixkit`.
