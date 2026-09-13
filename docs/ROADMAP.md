# Library roadmap

The initial four-operation API is implemented: `sort`, `nth_element`,
`partial_sort` and `top_k` support uint32/uint64 keys and records. MIT licensing,
packaging and reproducible correctness tests are in place. See [API](API.md).
The legacy uint32 `topk` entry point is preserved.

Next priorities:

- Validate on x86 and additional compilers; current evidence focuses on Apple M1.
- Broaden selection benchmarks across sizes, distributions and payload widths.
- Consider reusable caller-owned scratch with precise size/alignment/overlap and
  exception contracts, if allocation measurements justify the added API surface.
- Consider `stable_sort` and `partial_sort_copy` as separate additions with their
  own correctness and performance validation.

Keep strings, floating-point keys, arbitrary comparators, internal threading and
integration with databases outside the core. Preserve research candidates outside
installed headers and require measured advantages before changing defaults.

See the [release audit](PUBLIC_RELEASE_AUDIT.md) for current validation limits.
