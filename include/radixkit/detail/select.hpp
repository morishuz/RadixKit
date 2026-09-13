// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <type_traits>

namespace radixkit::detail {
// MSD radix selection: only recurse into the bucket containing the requested
// zero-based rank. Entire records move together. Earlier digits already agree
// inside that bucket, so unsigned byte order determines the remaining order.
enum class selection_mode { nth, smallest_prefix, largest_prefix };

// Keep the smallest count values in a max-heap, then place its maximum at
// count-1. Reverse iterators and a reversed comparator handle the upper end.
// At most 16*count+32 replacements limit wasted work on adversarial inputs.
// On failure the input is still a permutation, ready for ordinary radix selection.
template<class Iterator, class Less>
bool select_heap_end(Iterator first, Iterator last, std::size_t count, Less less) {
    auto end = first + count;
    std::make_heap(first, end, less);
    std::size_t budget = 16 * count + 32;
    for (auto it = end; it != last; ++it) {
        if (!less(*it, *first)) continue;
        if (budget == 0) return false;
        --budget;
        std::pop_heap(first, end, less);
        std::iter_swap(end - 1, it);
        std::push_heap(first, end, less);
    }
    std::iter_swap(first, end - 1);
    return true;
}

// Keep the nth-only frontend separate so prefix selection and the radix kernel
// retain their existing control flow. The caller falls back on a false result.
template<class Record, class Key>
bool select_near_end(std::span<Record> values, std::size_t rank, Key& key) {
    const auto count = std::min(rank, values.size() - 1 - rank) + 1;
    if (count <= 1 || count > 64 || count > values.size() / 1024) return false;
    auto less = [&](const Record& a, const Record& b) { return key(a) < key(b); };
    return rank < values.size() / 2
        ? select_heap_end(values.begin(), values.end(), count, less)
        : select_heap_end(values.rbegin(), values.rend(), count,
            [&](const Record& a, const Record& b) { return less(b, a); });
}

template<selection_mode Mode, class Record, class Key>
void radix_select(std::span<Record> values, std::size_t rank, Key key) {
    constexpr bool largest = Mode == selection_mode::largest_prefix;
    constexpr bool prefix_only = Mode != selection_mode::nth;
    using K = std::remove_cvref_t<std::invoke_result_t<Key, const Record&>>;
    static_assert(std::is_same_v<K, std::uint32_t> || std::is_same_v<K, std::uint64_t>);
    static_assert(std::is_trivially_copyable_v<Record>);
    assert(values.empty() || rank < values.size());
    if (values.size() < 2) return;
    auto less = [&](const Record& a, const Record& b) {
        if constexpr (largest) return key(a) > key(b);
        else return key(a) < key(b);
    };
    if (rank == 0) {
        std::iter_swap(values.begin(), std::min_element(values.begin(), values.end(), less));
        return;
    }
    if (rank == values.size() - 1) {
        if constexpr (!prefix_only)
            std::iter_swap(values.end() - 1, std::max_element(values.begin(), values.end(), less));
        return;
    }
    for (int shift = sizeof(K) * 8 - 8; shift >= 0 && values.size() > 1; shift -= 8) {
        auto digit = [&](const Record& v) {
            unsigned d = (key(v) >> shift) & 255u;
            if constexpr (largest) d ^= 255u;
            return d;
        };
        std::array<std::size_t, 256> counts{};
        for (const auto& v : values) ++counts[digit(v)];
        unsigned bucket = 0;
        std::size_t preceding = 0;
        // Every record contributes exactly once, so rank < size implies a
        // matching bucket. Keep the array bound explicit as well.
        while (bucket + 1 < counts.size() && rank - preceding >= counts[bucket])
            preceding += counts[bucket++];
        assert(rank >= preceding && rank - preceding < counts[bucket]);
        // Constant digits need no partition, but still advance to the next byte.
        if (counts[bucket] == values.size()) {
            if constexpr (prefix_only) if (rank + 1 == values.size()) return;
            // Earlier digits agree throughout this range. An exact variation
            // mask lets narrow-domain inputs skip all remaining constant bytes.
            K variation = 0;
            const K first_key = key(values.front());
            for (const auto& v : values) variation |= key(v) ^ first_key;
            if (!variation) return;
            shift = static_cast<int>((std::bit_width(variation) - 1) / 8 * 8) + 8;
            continue; // The loop decrement advances to the highest varying byte.
        }
        auto first = values.begin();
        auto boundary = std::partition(first, values.end(),
            [&](const Record& v) { return digit(v) < bucket; });
        auto end = std::partition(boundary, values.end(),
            [&](const Record& v) { return digit(v) == bucket; });
        // Catch inconsistent projections in debug builds before narrowing the
        // span. These checks do not establish extractor purity in general.
        assert(static_cast<std::size_t>(boundary - first) == preceding);
        assert(static_cast<std::size_t>(end - boundary) == counts[bucket]);
        rank -= preceding;
        values = values.subspan(static_cast<std::size_t>(boundary - first),
                                static_cast<std::size_t>(end - boundary));
        if constexpr (prefix_only) if (rank + 1 == values.size()) return;
    }
}
} // namespace radixkit::detail
