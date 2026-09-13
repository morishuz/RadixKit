// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "striped_radix.hpp"

namespace radixkit::detail {
namespace striped_detail {
// Stable eight-value run batching. Mixed groups retain sequential offsets.
template <unsigned Shift, unsigned Mask>
inline void scatter_runs8(const std::uint32_t* source, std::uint32_t* destination, std::size_t n,
                          std::uint32_t* offsets) {
    std::size_t i = 0;
    for (; n - i >= 8; i += 8) {
        const auto a = source[i], b = source[i + 1];
        const auto c = source[i + 2], d = source[i + 3];
        const auto e = source[i + 4], f = source[i + 5];
        const auto g = source[i + 6], h = source[i + 7];
        const auto differences =
            (a ^ b) | (a ^ c) | (a ^ d) | (a ^ e) | (a ^ f) | (a ^ g) | (a ^ h);
        if (((differences >> Shift) & Mask) == 0) {
            const auto bucket = (a >> Shift) & Mask;
            const auto position = offsets[bucket];
            offsets[bucket] = position + 8;
            destination[position] = a;
            destination[position + 1] = b;
            destination[position + 2] = c;
            destination[position + 3] = d;
            destination[position + 4] = e;
            destination[position + 5] = f;
            destination[position + 6] = g;
            destination[position + 7] = h;
        } else {
            const auto pa = offsets[(a >> Shift) & Mask]++;
            const auto pb = offsets[(b >> Shift) & Mask]++;
            const auto pc = offsets[(c >> Shift) & Mask]++;
            const auto pd = offsets[(d >> Shift) & Mask]++;
            destination[pa] = a;
            destination[pb] = b;
            destination[pc] = c;
            destination[pd] = d;
            const auto pe = offsets[(e >> Shift) & Mask]++;
            const auto pf = offsets[(f >> Shift) & Mask]++;
            const auto pg = offsets[(g >> Shift) & Mask]++;
            const auto ph = offsets[(h >> Shift) & Mask]++;
            destination[pe] = e;
            destination[pf] = f;
            destination[pg] = g;
            destination[ph] = h;
        }
    }
    for (; n - i >= 4; i += 4) {
        const auto a = source[i], b = source[i + 1];
        const auto c = source[i + 2], d = source[i + 3];
        const auto pa = offsets[(a >> Shift) & Mask]++;
        const auto pb = offsets[(b >> Shift) & Mask]++;
        const auto pc = offsets[(c >> Shift) & Mask]++;
        const auto pd = offsets[(d >> Shift) & Mask]++;
        destination[pa] = a;
        destination[pb] = b;
        destination[pc] = c;
        destination[pd] = d;
    }
    for (; i < n; ++i) {
        const auto value = source[i];
        destination[offsets[(value >> Shift) & Mask]++] = value;
    }
}
} // namespace striped_detail

// Use run batching only for passes with an exactly counted majority bucket.
// Smaller inputs use the frozen implementation to avoid extra prefix overhead.
inline void scatter_radix_sort(std::span<std::uint32_t> values) {
    constexpr unsigned Lanes = 4;
    const auto n = values.size();
    if (n < 65536) {
        striped_radix_sort<4>(values);
        return;
    }
    if (n > std::numeric_limits<std::uint32_t>::max()) {
        radix_sort<11>(values);
        return;
    }
    constexpr std::size_t bins = 2048 + 2048 + 1024;
    alignas(64) std::array<std::array<std::uint32_t, bins>, Lanes> counts{};
    const auto first = values.front();
    std::size_t i = 0;
    for (; n - i >= Lanes; i += Lanes) {
        for (unsigned lane = 0; lane < Lanes; ++lane) {
            const auto v = values[i + lane];
            ++counts[lane][v & 2047u];
            ++counts[lane][2048 + ((v >> 11) & 2047u)];
            ++counts[lane][4096 + (v >> 22)];
        }
    }
    for (; i < n; ++i) {
        const auto v = values[i];
        ++counts[0][v & 2047u];
        ++counts[0][2048 + ((v >> 11) & 2047u)];
        ++counts[0][4096 + (v >> 22)];
    }
    // A digit is constant exactly when all n values match the first digit.
    std::array<std::uint32_t, 3> matching{};
    for (unsigned lane = 0; lane < Lanes; ++lane) {
        matching[0] += counts[lane][first & 2047u];
        matching[1] += counts[lane][2048 + ((first >> 11) & 2047u)];
        matching[2] += counts[lane][4096 + (first >> 22)];
    }
    const std::uint32_t varying = (matching[0] != n ? 2047u : 0u) |
                                  (matching[1] != n ? (2047u << 11) : 0u) |
                                  (matching[2] != n ? (1023u << 22) : 0u);
    if (!varying) return;
    for (unsigned lane = 1; lane < Lanes; ++lane)
        for (std::size_t b = 0; b < bins; ++b)
            counts[0][b] += counts[lane][b];
    std::array<bool, 3> dominant{};
    for (unsigned pass = 0; pass < 3; ++pass) {
        const unsigned start = pass * 2048, end = pass == 2 ? 5120 : start + 2048;
        std::uint32_t offset = 0;
        for (unsigned b = start; b < end; ++b) {
            const auto frequency = counts[0][b];
            dominant[pass] |= frequency > n / 2;
            counts[0][b] = offset;
            offset += frequency;
        }
    }
    auto scratch = std::make_unique_for_overwrite<std::uint32_t[]>(n);
    auto* source = values.data();
    auto* destination = scratch.get();
    if (varying & 2047u) {
        if (dominant[0])
            striped_detail::scatter_runs8<0, 2047>(source, destination, n, counts[0].data());
        else
            striped_detail::scatter<0, 2047>(source, destination, n, counts[0].data());
        std::swap(source, destination);
    }
    if (varying & (2047u << 11)) {
        if (dominant[1])
            striped_detail::scatter_runs8<11, 2047>(source, destination, n,
                                                    counts[0].data() + 2048);
        else
            striped_detail::scatter<11, 2047>(source, destination, n, counts[0].data() + 2048);
        std::swap(source, destination);
    }
    if (varying >> 22) {
        if (dominant[2])
            striped_detail::scatter_runs8<22, 1023>(source, destination, n,
                                                    counts[0].data() + 4096);
        else
            striped_detail::scatter<22, 1023>(source, destination, n, counts[0].data() + 4096);
        std::swap(source, destination);
    }
    if (source != values.data()) std::copy_n(source, n, values.data());
}
} // namespace radixkit::detail
