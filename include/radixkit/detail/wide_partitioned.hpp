// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once

#include "radix64.hpp"

namespace radixkit::detail::wide {

// One global MSD partition followed by cache-local LSD scatters. The
// top-bit partition fixes each bucket's final range in the caller's array;
// lower-bit sorting can then use matching ranges in the single scratch array.
// TopBits=10 and TopBits=11 with LocalBits=11 retain six total scatters. For
// uniformly distributed 1M-row, 16-byte records these create about 1024- or
// 512-row runs. LocalBits=9 trades an extra local pass and copy-back for a
// smaller histogram.
template<unsigned TopBits, unsigned LocalBits = 11, unsigned LocalLanes = 2,
         class Record, class Key>
inline void partitioned_radix64_sort(std::span<Record> values, Key key) {
    static_assert(TopBits == 10 || TopBits == 11);
    static_assert(LocalBits == 9 || LocalBits == 11);
    static_assert(LocalLanes == 1 || LocalLanes == 2);
    static_assert(std::is_trivially_copyable_v<Record> &&
                  std::is_trivially_default_constructible_v<Record>);
    static_assert(std::is_same_v<
        std::remove_cvref_t<std::invoke_result_t<Key, const Record&>>,
        std::uint64_t>);

    const auto n = values.size();
    // Avoid thousands of mostly empty buckets on very small inputs. Keep the
    // reference's explicit guard on the 32-bit histogram counters.
    if (n < 32768 || n > std::numeric_limits<std::uint32_t>::max()) {
        ::radixkit::detail::radix64_sort<11, 2>(values, key);
        return;
    }

    constexpr unsigned top_bins = 1u << TopBits;
    constexpr unsigned top_shift = 64 - TopBits;
    std::array<std::uint32_t, 2 * top_bins> top_counts{};
    std::size_t i = 0;
    for (; n - i >= 2; i += 2) {
        ++top_counts[key(values[i]) >> top_shift];
        ++top_counts[top_bins + (key(values[i + 1]) >> top_shift)];
    }
    for (; i < n; ++i) ++top_counts[key(values[i]) >> top_shift];
    for (unsigned b = 0; b < top_bins; ++b)
        top_counts[b] += top_counts[top_bins + b];

    // A common high prefix would leave one huge bucket and provide no cache
    // benefit. The ordinary sorter already skips its constant high digits.
    if (top_counts[key(values.front()) >> top_shift] == n) {
        ::radixkit::detail::radix64_sort<11, 2>(values, key);
        return;
    }

    std::array<std::uint32_t, top_bins + 1> boundaries;
    boundaries[0] = 0;
    for (unsigned b = 0; b < top_bins; ++b) {
        boundaries[b + 1] = boundaries[b] + top_counts[b];
        top_counts[b] = boundaries[b];
    }

    auto scratch = std::make_unique_for_overwrite<Record[]>(n);
    i = 0;
    for (; n - i >= 4; i += 4) {
        const auto a = values[i], b = values[i + 1];
        const auto c = values[i + 2], d = values[i + 3];
        const auto pa = top_counts[key(a) >> top_shift]++;
        const auto pb = top_counts[key(b) >> top_shift]++;
        const auto pc = top_counts[key(c) >> top_shift]++;
        const auto pd = top_counts[key(d) >> top_shift]++;
        scratch[pa] = a; scratch[pb] = b;
        scratch[pc] = c; scratch[pd] = d;
    }
    for (; i < n; ++i) {
        const auto value = values[i];
        scratch[top_counts[key(value) >> top_shift]++] = value;
    }

    constexpr unsigned bits = LocalBits;
    constexpr unsigned full_bins = 1u << bits;
    constexpr unsigned passes = (top_shift + bits - 1) / bits;
    constexpr unsigned last_bits = top_shift - (passes - 1) * bits;
    constexpr unsigned histogram_bins =
        (passes - 1) * full_bins + (1u << last_bits);
    // Two independent lanes match the reference. Their table is 68/72 KiB
    // for 11-bit digits or 22/24 KiB for 9-bit digits. One lane halves these
    // footprints and avoids a folding pass. Storage is reused per MSD bucket.
    std::array<std::uint32_t, LocalLanes * histogram_bins> counts;
    const auto less = [&](const Record& a, const Record& b) {
        return key(a) < key(b);
    };

    for (unsigned bucket = 0; bucket < top_bins; ++bucket) {
        const auto start = boundaries[bucket];
        const auto size = static_cast<std::size_t>(boundaries[bucket + 1] - start);
        auto* source = scratch.get() + start;
        auto* destination = values.data() + start;
        if (size < 128) {
            std::copy_n(source, size, destination);
            std::sort(destination, destination + size, less);
            continue;
        }

        counts.fill(0);
        const auto first = key(source[0]);
        auto count_value = [&](unsigned lane, std::uint64_t value) {
            for (unsigned pass = 0; pass < passes; ++pass) {
                const auto mask = pass + 1 == passes
                    ? (1u << last_bits) - 1 : full_bins - 1;
                ++counts[lane * histogram_bins + pass * full_bins +
                         ((value >> (pass * bits)) & mask)];
            }
        };
        i = 0;
        for (; size - i >= 2; i += 2) {
            count_value(0, key(source[i]));
            count_value(LocalLanes - 1, key(source[i + 1]));
        }
        for (; i < size; ++i) count_value(0, key(source[i]));

        std::array<bool, passes> active;
        for (unsigned pass = 0; pass < passes; ++pass) {
            const auto mask = pass + 1 == passes
                ? (1u << last_bits) - 1 : full_bins - 1;
            const auto b = pass * full_bins + ((first >> (pass * bits)) & mask);
            auto matching = counts[b];
            if constexpr (LocalLanes == 2) matching += counts[histogram_bins + b];
            active[pass] = matching != size;
        }
        if constexpr (LocalLanes == 2)
            for (unsigned b = 0; b < histogram_bins; ++b)
                counts[b] += counts[histogram_bins + b];

        for (unsigned pass = 0; pass < passes; ++pass) {
            if (!active[pass]) continue;
            const auto mask = pass + 1 == passes
                ? (1u << last_bits) - 1 : full_bins - 1;
            auto* offsets = counts.data() + pass * full_bins;
            std::uint32_t offset = 0;
            for (unsigned b = 0; b <= mask; ++b) {
                const auto frequency = offsets[b];
                offsets[b] = offset;
                offset += frequency;
            }
            const auto shift = pass * bits;
            i = 0;
            for (; size - i >= 4; i += 4) {
                const auto a = source[i], b = source[i + 1];
                const auto c = source[i + 2], d = source[i + 3];
                const auto pa = offsets[(key(a) >> shift) & mask]++;
                const auto pb = offsets[(key(b) >> shift) & mask]++;
                const auto pc = offsets[(key(c) >> shift) & mask]++;
                const auto pd = offsets[(key(d) >> shift) & mask]++;
                destination[pa] = a; destination[pb] = b;
                destination[pc] = c; destination[pd] = d;
            }
            for (; i < size; ++i) {
                const auto value = source[i];
                destination[offsets[(key(value) >> shift) & mask]++] = value;
            }
            std::swap(source, destination);
        }
        if (source != values.data() + start)
            std::copy_n(source, size, values.data() + start);
    }
}

} // namespace radixkit::detail::wide
