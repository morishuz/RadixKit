# API

Include `<radixkit/radixkit.hpp>`. Requires C++20. Inputs are mutable, non-volatile contiguous
storage. Supported keys are exactly `uint32_t` and `uint64_t`; all ordering is
unsigned. All four operations support keys alone and records carrying payloads.

| Operation | Argument | Successful result |
|---|---|---|
| `sort(values)` | — | Entire input ascending |
| `nth_element(values, nth)` | Zero-based rank | Element at `nth` equals the sorted element at that rank; prefix keys ≤ that key and suffix keys ≥ it |
| `partial_sort(values, k)` | Count | Smallest `k` elements sorted ascending in the prefix; remainder unordered |
| `top_k(values, k)` | Count | Largest `k` elements in the prefix; both partitions unordered |

Values, duplicates and whole records are preserved. Output stays in the input
storage. No operation guarantees stability or which payload wins a tie at a
selection boundary. With repeated keys, any records satisfying the key ordering
are valid. No internal multithreading is used.

## Boundaries

- Empty input is valid for `sort`; other operations accept only position/count 0.
- `nth == size()` is a no-op, matching `std::nth_element` with `nth == end()`.
- `partial_sort(values, 0)` is a no-op; `k == size()` sorts the entire input.
- `top_k` with `k == 0` or `k == size()` is a no-op.
- A rank/count greater than `size()` throws `std::out_of_range` before mutation.
- `topk(span<uint32_t>, k)` preserves the original spelling and exact signature;
  new code should use `top_k`, which supports both widths and records.

## Records and payloads

`key_value32` and `key_value64` contain `.key` of the named width and `.value` of
type `uint64_t`. They work with every operation directly, including vectors,
arrays and spans. Key width is independent of payload width: custom records use
an extractor and can contain multiple payload fields.

These are naturally aligned structs, not packed wire formats. On the tested
64-bit ABI both occupy 16 bytes; `key_value32` includes padding for its aligned
64-bit payload. Padding is not part of the sorting or equality contract: compare
fields, not raw bytes. Packing is not used because it can introduce unaligned
access and change ABI and aggregate initialization.

```cpp
struct row { std::uint32_t key; std::uint64_t id; std::uint64_t extra; };
std::vector<row> rows{{9, 10, 20}, {2, 30, 40}, {7, 50, 60}};
auto key = [](const row& r) { return r.key; }; // Retains the 32-bit key width.
radixkit::sort_by_key(std::span(rows), key);
radixkit::nth_element_by_key(std::span(rows), 1, key);
radixkit::partial_sort_by_key(std::span(rows), 2, key);
radixkit::top_k_by_key(std::span(rows), 2, key);
```

The `_by_key` forms accept dynamic or fixed-extent spans. Records must be
copyable, trivially copyable and trivially default-constructible (including the
assignment and swapping required by the kernels). These requirements and key
return types are checked at the public API boundary.
Extractors must be copyable function objects that deterministically return `uint32_t` or `uint64_t`, must not mutate
records and must not throw. Native 32-bit record sorting processes only 32 key
bits. Widening an extractor to uint64_t explicitly chooses the 64-bit path.

Determinism and non-mutation are semantic preconditions, including across copies
of an extractor. The concept checks callable shape and key type, not those
properties. `const` and `noexcept` cannot prove purity; an explicit `noexcept`
annotation is not required, so ordinary non-throwing lambdas remain accepted.
Violating these preconditions is unsupported and can invalidate memory safety.

## Memory and failures

`nth_element` and `top_k` use no dynamic allocation: one 256-entry `size_t`
histogram plus bounded local state. They partition only the bucket containing
the requested rank. Extreme ranks use a minimum/maximum scan.

For `nth_element`, a near-end rank may instead use a heap stored in the input.
The heap contains at most 64 records and must be no larger than 1/1024 of the
input. Excessive heap replacements trigger the ordinary radix selector on the
still-complete permutation. This path adds no allocation and may order more of
the selected end than the contract requires. Prefix-selection APIs retain their
existing algorithms.

`sort` uses O(n) temporary heap storage in the worst case. `partial_sort` uses
standard heap selection for tiny prefixes; otherwise it composes selection with
sorting its prefix, requiring O(k) temporary storage plus bounded
histogram state. Some sort paths avoid the element buffer; adaptive counting can
use up to 1 MiB of counters. Caller-owned scratch is not exposed yet.

Full-sort radix paths also use substantial fixed stack tables: the uint32
four-lane table is 80 KiB and the uint64 two-lane LSD table is 84 KiB, before
other locals and caller frames. These are table sizes, not bounds on total stack
usage. Small-stack threads and embedded devices have not been validated.

Above `UINT32_MAX` elements, plain uint32 radix sorting uses `size_t` counters;
uint64 and projected-record paths fall back to `std::sort` (O(n log n)
comparisons). O(n) scratch is an upper bound, not a promise that every path
allocates an element buffer. Such huge inputs have not been tested; do not
extrapolate the published timings across this boundary.

Allocation failures propagate as `std::bad_alloc`; rollback is not promised.
For example, partial sorting can complete selection before the prefix sort fails
to allocate. No shared mutable library state is used; independent ranges can be
processed concurrently with independently safe extractor state.

Debug builds use standard `assert` checks for internal selection invariants.
Keep `NDEBUG` consistent across translation units using radixkit in the same
program: mixing assertion settings can produce different definitions of the
same header-defined template and violate C++'s one-definition rule.

## Sort policies

Plain keys also accept `sort(values, sort_policy::adaptive)` and
`sort(values, sort_policy::radix)`. The uint32 default is the radix champion;
the uint64 default retains its existing distribution-aware dispatcher. Explicit
uint64 `radix` bypasses that front end, but its kernel still samples to choose
ordinary LSD, run-batched LSD or MSD partitioning followed by LSD. It does not
force a single fixed LSD algorithm. Explicit `adaptive` uses verified order,
dominant-key and varying-bit shortcuts. Samples choose strategies, never justify
an approximate result. Policies are not exposed for record or selection APIs.
See [benchmarking](BENCHMARKING.md) for reproducible comparisons.

Stable sorting, `partial_sort_copy`, signed/floating/string keys, arbitrary
comparators and caller-owned scratch remain outside this API. Internal `detail/`
headers are not supported entry points.
