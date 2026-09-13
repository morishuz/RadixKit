// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace radixkit::detail::wide {

struct identity_key64 {
    std::uint64_t operator()(std::uint64_t value) const { return value; }
};

namespace adaptive_detail {

// Four independent reductions allow the compiler to vectorize scalar keys and
// avoid one loop-carried dependency for every record. No sample proves equality.
template<class Record, class Key>
bool all_equal(std::span<Record> values, Key& key, std::uint64_t pivot) {
    std::uint64_t a = 0, b = 0, c = 0, d = 0;
    std::size_t i = 0;
    for (; values.size() - i >= 4; i += 4) {
        a |= key(values[i]) ^ pivot;
        b |= key(values[i + 1]) ^ pivot;
        c |= key(values[i + 2]) ^ pivot;
        d |= key(values[i + 3]) ^ pivot;
    }
    for (; i < values.size(); ++i) a |= key(values[i]) ^ pivot;
    return (a | b | c | d) == 0;
}

// A sample proposes a range; the full read verifies it before any records are
// changed. A late outlier therefore safely abandons this path. Unsigned delta
// catches values below minimum, including when minimum is close to UINT64_MAX.
template<class Record, class Key>
bool count_range(std::span<Record> values, Key& key,
                 std::uint64_t minimum, std::uint64_t maximum) {
    const auto delta = maximum - minimum;
    if (delta >= 65536 || delta + 1 > values.size() / 4) return false;
    const auto range = static_cast<std::size_t>(delta + 1);
    constexpr std::size_t small_range = 256;
    std::array<std::uint32_t, 4 * small_range> small_counts{};
    std::unique_ptr<std::uint32_t[]> allocated;
    if (range > small_range) allocated = std::make_unique<std::uint32_t[]>(4 * range);
    auto* counts = range > small_range ? allocated.get() : small_counts.data();

    std::size_t i = 0;
    for (; values.size() - i >= 4; i += 4) {
        const auto a = key(values[i]) - minimum;
        const auto b = key(values[i + 1]) - minimum;
        const auto c = key(values[i + 2]) - minimum;
        const auto d = key(values[i + 3]) - minimum;
        if (a > delta || b > delta || c > delta || d > delta) return false;
        ++counts[a];
        ++counts[range + b];
        ++counts[2 * range + c];
        ++counts[3 * range + d];
    }
    for (; i < values.size(); ++i) {
        const auto bucket = key(values[i]) - minimum;
        if (bucket > delta) return false;
        ++counts[bucket];
    }
    for (std::size_t bucket = 0; bucket < range; ++bucket)
        counts[bucket] += counts[range + bucket] + counts[2 * range + bucket] +
                          counts[3 * range + bucket];

    if constexpr (std::is_same_v<Record, std::uint64_t> &&
                  std::is_same_v<std::remove_cvref_t<Key>, identity_key64>) {
        // Reconstructing keys is valid only for the explicit identity-key API.
        // A uint64_t with some other extractor can itself contain payload bits.
        std::size_t output = 0;
        for (std::size_t bucket = 0; bucket < range; ++bucket) {
            std::fill_n(values.data() + output, counts[bucket], minimum + bucket);
            output += counts[bucket];
        }
    } else {
        std::uint32_t offset = 0;
        for (std::size_t bucket = 0; bucket < range; ++bucket) {
            const auto frequency = counts[bucket];
            counts[bucket] = offset;
            offset += frequency;
        }
        auto scratch = std::make_unique_for_overwrite<Record[]>(values.size());
        i = 0;
        for (; values.size() - i >= 4; i += 4) {
            const auto a = values[i], b = values[i + 1];
            const auto c = values[i + 2], d = values[i + 3];
            const auto pa = counts[key(a) - minimum]++;
            const auto pb = counts[key(b) - minimum]++;
            const auto pc = counts[key(c) - minimum]++;
            const auto pd = counts[key(d) - minimum]++;
            scratch[pa] = a; scratch[pb] = b;
            scratch[pc] = c; scratch[pd] = d;
        }
        for (; i < values.size(); ++i) {
            const auto record = values[i];
            scratch[counts[key(record) - minimum]++] = record;
        }
        std::copy_n(scratch.get(), values.size(), values.data());
    }
    return true;
}

} // namespace adaptive_detail


} // namespace radixkit::detail::wide
