// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>

namespace radixkit::detail {
// Exact unsigned 32/64-bit LSD sorting. Each scatter moves the complete record;
// equal-key records must never be reconstructed from frequencies alone.
template <unsigned Bits, unsigned Lanes, bool BatchRuns, class Record, class Key>
inline void radix64_sort_impl(std::span<Record> values, Key key) {
    static_assert(Bits == 8 || Bits == 11 || Bits == 16);
    static_assert(Lanes == 1 || Lanes == 2 || Lanes == 4);
    static_assert(std::is_trivially_copyable_v<Record> &&
                  std::is_trivially_default_constructible_v<Record>);
    using KeyType = std::remove_cvref_t<std::invoke_result_t<Key, const Record&>>;
    static_assert(std::is_same_v<KeyType, std::uint32_t> || std::is_same_v<KeyType, std::uint64_t>,
                  "Key must return uint32_t or uint64_t");
    constexpr unsigned key_bits = sizeof(KeyType) * 8;
    const auto n = values.size();
    if (n < 128 || n > std::numeric_limits<std::uint32_t>::max()) {
        std::sort(values.begin(), values.end(),
                  [&](const Record& a, const Record& b) { return key(a) < key(b); });
        return;
    }
    constexpr unsigned passes = (key_bits + Bits - 1) / Bits;
    constexpr unsigned full_bins = 1u << Bits;
    constexpr unsigned last_bits = key_bits - (passes - 1) * Bits;
    constexpr unsigned bins = (passes - 1) * full_bins + (1u << last_bits);
    // The 16-bit experiment has a large table; keep it off the thread stack.
    constexpr bool heap_counts = bins * Lanes * sizeof(std::uint32_t) > 256 * 1024;
    std::array<std::uint32_t, heap_counts ? 0 : bins * Lanes> stack_counts{};
    std::unique_ptr<std::uint32_t[]> allocated_counts;
    if constexpr (heap_counts) allocated_counts = std::make_unique<std::uint32_t[]>(bins * Lanes);
    auto* counts = heap_counts ? allocated_counts.get() : stack_counts.data();
    const auto first = key(values.front());
    auto count_value = [&](unsigned lane, std::uint64_t value) {
        for (unsigned pass = 0; pass < passes; ++pass) {
            const auto mask = pass + 1 == passes ? (1u << last_bits) - 1 : full_bins - 1;
            ++counts[lane * bins + pass * full_bins + ((value >> (pass * Bits)) & mask)];
        }
    };
    std::size_t i = 0;
    for (; n - i >= Lanes; i += Lanes)
        for (unsigned lane = 0; lane < Lanes; ++lane)
            count_value(lane, key(values[i + lane]));
    for (; i < n; ++i)
        count_value(0, key(values[i]));
    std::array<bool, passes> active{};
    bool any = false;
    for (unsigned pass = 0; pass < passes; ++pass) {
        const auto mask = pass + 1 == passes ? (1u << last_bits) - 1 : full_bins - 1;
        const auto bucket = pass * full_bins + ((first >> (pass * Bits)) & mask);
        std::uint32_t matching = 0;
        for (unsigned lane = 0; lane < Lanes; ++lane)
            matching += counts[lane * bins + bucket];
        any |= active[pass] = matching != n;
    }
    if (!any) return; // Every digit is constant: preserve the input as-is.
    for (unsigned lane = 1; lane < Lanes; ++lane)
        for (unsigned b = 0; b < bins; ++b)
            counts[b] += counts[lane * bins + b];
    // Prefix sums turn frequencies into stable scatter destinations.
    std::array<bool, passes> dominant{};
    for (unsigned pass = 0; pass < passes; ++pass) {
        if (!active[pass]) continue;
        const unsigned start = pass * full_bins;
        const unsigned end = start + (pass + 1 == passes ? (1u << last_bits) : full_bins);
        std::uint32_t offset = 0;
        for (unsigned b = start; b < end; ++b) {
            const auto frequency = counts[b];
            if constexpr (BatchRuns) dominant[pass] |= frequency > n / 2;
            counts[b] = offset;
            offset += frequency;
        }
    }
    // Alternate buffers only for active passes; an odd pass count needs copy-back.
    auto scratch = std::make_unique_for_overwrite<Record[]>(n);
    auto* source = values.data();
    auto* destination = scratch.get();
    for (unsigned pass = 0; pass < passes; ++pass) {
        if (!active[pass]) continue;
        const auto shift = pass * Bits;
        const auto mask = pass + 1 == passes ? (1u << last_bits) - 1 : full_bins - 1;
        auto* offsets = counts + pass * full_bins;
        i = 0;
        if constexpr (BatchRuns) {
            if (dominant[pass])
                for (; n - i >= 8; i += 8) {
                    const auto bucket = (key(source[i]) >> shift) & mask;
                    std::uint64_t differences = 0;
                    for (unsigned j = 1; j < 8; ++j)
                        differences |= key(source[i]) ^ key(source[i + j]);
                    if (((differences >> shift) & mask) == 0) {
                        const auto pos = offsets[bucket];
                        offsets[bucket] += 8;
                        std::copy_n(source + i, 8, destination + pos);
                    } else
                        for (unsigned j = 0; j < 8; ++j) {
                            const auto value = source[i + j];
                            destination[offsets[(key(value) >> shift) & mask]++] = value;
                        }
                }
        }
        for (; n - i >= 4; i += 4) {
            const auto a = source[i], b = source[i + 1], c = source[i + 2], d = source[i + 3];
            const auto pa = offsets[(key(a) >> shift) & mask]++;
            const auto pb = offsets[(key(b) >> shift) & mask]++;
            const auto pc = offsets[(key(c) >> shift) & mask]++;
            const auto pd = offsets[(key(d) >> shift) & mask]++;
            destination[pa] = a;
            destination[pb] = b;
            destination[pc] = c;
            destination[pd] = d;
        }
        for (; i < n; ++i) {
            const auto value = source[i];
            destination[offsets[(key(value) >> shift) & mask]++] = value;
        }
        std::swap(source, destination);
    }
    if (source != values.data()) std::copy_n(source, n, values.data());
}
// A compile-time switch keeps histogram construction and scatter invariants in
// one place while eliminating run detection from the ordinary radix kernel.
template <unsigned Bits, unsigned Lanes, class Record, class Key>
inline void radix64_sort(std::span<Record> values, Key key) {
    radix64_sort_impl<Bits, Lanes, false>(values, key);
}
} // namespace radixkit::detail
