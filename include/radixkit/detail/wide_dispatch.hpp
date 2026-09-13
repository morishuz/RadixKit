// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "wide_counting.hpp"
#include "wide_partitioned.hpp"
#include "wide_runs.hpp"
#include "wide_dictionary.hpp"
#include "wide_order.hpp"
namespace radixkit::detail::wide {
template <bool CountRange = true, std::size_t RangeAlign = 1, unsigned HashVariant = 1,
          class Record, class Key, class Fallback>
void adaptive_frontend(std::span<Record> values, Key key, Fallback fallback) {
    static_assert(RangeAlign > 0 && RangeAlign <= 65536 && (RangeAlign & (RangeAlign - 1)) == 0);
    static_assert(std::is_trivially_copyable_v<Record> &&
                  std::is_trivially_default_constructible_v<Record>);
    static_assert(std::is_same_v<std::remove_cvref_t<std::invoke_result_t<Key, const Record&>>,
                                 std::uint64_t>);
    const auto n = values.size();
    if (n < 2) return;
    if (n < 4096 || n > std::numeric_limits<std::uint32_t>::max()) {
        fallback(values);
        return;
    }
    constexpr std::size_t sample_size = 32;
    std::array<std::uint64_t, sample_size> sample;
    // n is bounded above, so these products cannot overflow size_t on a
    // supported 64-bit target; division includes both the first and last key.
    const auto step = (n - 1) / (sample_size - 1);
    const auto remainder = (n - 1) % (sample_size - 1);
    for (std::size_t i = 0; i < sample_size; ++i)
        sample[i] = key(values[i * step + i * remainder / (sample_size - 1)]);

    bool ascending = true, descending = true;
    for (std::size_t i = 1; i < sample_size; ++i) {
        ascending &= sample[i - 1] <= sample[i];
        descending &= sample[i - 1] >= sample[i];
    }
    if (ascending && descending && adaptive_detail::all_equal(values, key, sample[0])) return;
    if (ascending && ::radixkit::detail::wide::bounded_order_repair(values, key, false)) return;
    if (descending && ::radixkit::detail::wide::bounded_order_repair(values, key, true)) {
        std::reverse(values.begin(), values.end());
        return;
    }
    std::sort(sample.begin(), sample.end());
    const auto pivot = sample[sample_size / 2];
    if (std::count(sample.begin(), sample.end(), pivot) >= 28) {
        // Extreme pivots let us omit one whole partition pass. For all other
        // pivots, the two partitions establish < pivot, == pivot, > pivot.
        auto equal = values.begin();
        if (pivot != 0)
            equal = std::partition(values.begin(), values.end(),
                                   [&](const Record& record) { return key(record) < pivot; });
        auto upper = values.end();
        if (pivot != std::numeric_limits<std::uint64_t>::max())
            upper = std::partition(equal, values.end(),
                                   [&](const Record& record) { return key(record) == pivot; });
        const auto left = static_cast<std::size_t>(equal - values.begin());
        const auto right = static_cast<std::size_t>(upper - values.begin());
        if (left > 1) fallback(values.first(left));
        if (n - right > 1) fallback(values.subspan(right));
        return;
    }
    if constexpr (CountRange) {
        constexpr auto align_mask = static_cast<std::uint64_t>(RangeAlign - 1);
        if (adaptive_detail::count_range(values, key, sample.front() & ~align_mask,
                                         sample.back() | align_mask))
            return;
    }
    unsigned distinct = 1;
    for (unsigned i = 1; i < sample_size; ++i)
        distinct += sample[i] != sample[i - 1];
    if (distinct <= 24 && ::radixkit::detail::wide::try_dictionary<HashVariant>(values, key)) return;
    fallback(values);
}

template <class Record, class Key> void sort_kernel(std::span<Record> values, Key key) {
    const auto n = values.size();
    if (n < 32768 || n > UINT32_MAX) {
        ::radixkit::detail::radix64_sort<11, 2>(values, key);
        return;
    }
    std::array<std::uint64_t, 16> sample;
    for (unsigned i = 0; i < 16; ++i)
        sample[i] = key(values[(n - 1) / 15 * i]);
    auto ordered = sample;
    std::sort(ordered.begin(), ordered.end());
    // When a narrow sampled range failed exact counting, unseen outliers can
    // make otherwise constant digits active. Run batching handles those passes.
    bool repeated = ordered.back() - ordered.front() < 65536;
    for (unsigned i = 0; i + 8 < 16; ++i)
        repeated |= ordered[i] == ordered[i + 8];
    if (repeated) {
        ::radixkit::detail::wide::radix64_runs_sort<11, 2>(values, key);
        return;
    }
    unsigned distinct_top = 1;
    for (unsigned i = 1; i < 16; ++i)
        distinct_top += (ordered[i] >> 54) != (ordered[i - 1] >> 54);
    if (n >= 16777216 / sizeof(Record) && distinct_top >= 12) {
        ::radixkit::detail::wide::partitioned_radix64_sort<10, 9, 1>(values, key);
        return;
    }
    ::radixkit::detail::radix64_sort<11, 2>(values, key);
}
template <unsigned HashVariant = 1, class Record, class Key>
void adaptive_sort(std::span<Record> values, Key key) {
    ::radixkit::detail::wide::adaptive_frontend<true, 256, HashVariant>(
        values, key,
        [key](std::span<Record> part) { ::radixkit::detail::wide::sort_kernel(part, key); });
}
} // namespace radixkit::detail::wide
