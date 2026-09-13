# Library structure

The API exposes sorting and three selection operations, with the same contracts
for unsigned 32/64-bit keys and records. Implementations share key projections
and whole-record movement rather than reconstructing projected records.

| Module | Responsibility |
|---|---|
| `sort.hpp` | Full-sort overloads and `sort_by_key` |
| `selection.hpp` | Rank/count validation and selection entry points |
| `detail/key.hpp` | Supported key/record constraints and default projection |
| `detail/sort_dispatch.hpp` | Shared dispatch for full and prefix sorting |
| `detail/partial_sort.hpp` | Tiny heap or selection followed by prefix sorting |
| `detail/select.hpp` | Bounded near-end heap frontend and MSD radix selection |
| Other `detail/` headers | Radix passes, counting, dictionaries and order repair |

`radixkit.hpp` includes both API headers. `sort64.hpp` and `topk.hpp` are
compatibility includes; `topk` retains the original uint32 function signature.
The supported API is documented in [API.md](API.md); detail headers are private.

Full sorting and prefix sorting share the dispatcher. Only a built-in identity
projection on plain keys may use key-only reconstruction. Custom projections
preserve every original record, including integer records with payload bits.
Ordinary and run-batched record radix sorting share their histogram/scatter core.
Specialized 32-bit and partitioned 64-bit layouts remain separate.

Selection follows only the bucket containing the rank. Near-end nth-element
ranks may use a small heap in the input, with a bounded number of replacements
before falling back to radix. Samples elsewhere choose strategies; full checks,
not samples, establish correctness of order, range or duplicate shortcuts.

Tests check public constraints, complete permutations, key/payload associations,
partition boundaries, allocation failures and adversarial distributions. See
[correctness validation](CORRECTNESS.md).
