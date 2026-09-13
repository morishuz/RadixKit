// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace radixkit::detail {
// Stable LSD radix sort. Ascending uint32_t only; O(n) temporary storage.
// Count every digit in one traversal; omit digits constant across the input.
template<unsigned Bits = 11>
inline void radix_sort(std::span<std::uint32_t> values) {
    static_assert(Bits == 8 || Bits == 11);
    if (values.size() < 128) {
        std::sort(values.begin(), values.end());
        return;
    }
    constexpr unsigned passes = (32 + Bits - 1) / Bits;
    constexpr std::size_t buckets = std::size_t{1} << Bits;
    constexpr auto mask = static_cast<std::uint32_t>(buckets - 1);
    std::array<std::array<std::size_t, buckets>, passes> counts{};
    std::uint32_t differences = 0;
    const auto initial = values.front();
    for (auto v : values) {
        differences |= v ^ initial;
        for (unsigned pass = 0; pass < passes; ++pass)
            ++counts[pass][(v >> (pass * Bits)) & mask];
    }
    if (!differences) return;
    // Each active scatter writes every destination before it becomes a source.
    auto scratch = std::make_unique_for_overwrite<std::uint32_t[]>(values.size());
    auto* source = values.data();
    auto* destination = scratch.get();
    for (unsigned pass = 0; pass < passes; ++pass) {
        const auto shift = pass * Bits;
        if (((differences >> shift) & mask) == 0) continue;
        std::size_t offset = 0;
        for (auto& count : counts[pass]) {
            auto frequency = count;
            count = offset;
            offset += frequency;
        }
        for (std::size_t i = 0; i < values.size(); ++i) {
            auto v = source[i];
            destination[counts[pass][(v >> shift) & mask]++] = v;
        }
        std::swap(source, destination);
    }
    if (source != values.data()) std::copy_n(source, values.size(), values.data());
}
}
