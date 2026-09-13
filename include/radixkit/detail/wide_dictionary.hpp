// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once

#include "wide_counting.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>

namespace radixkit::detail::wide {

// Called only after a sample suggests a small alphabet. Discovery and counting
// read the full input before changing any records. If the alphabet exceeds 64
// keys, return false with the input unchanged so the caller can use its kernel.
// Unlike range counting, dictionary keys may span the entire uint64_t domain.
template<unsigned HashVariant = 1, class Record, class Key>
bool try_dictionary(std::span<Record> values, Key key) {
    static_assert(HashVariant <= 1);
    static_assert(std::is_trivially_copyable_v<Record> &&
                  std::is_trivially_default_constructible_v<Record>);
    static_assert(std::is_same_v<
        std::remove_cvref_t<std::invoke_result_t<Key, const Record&>>,
        std::uint64_t>);
    const auto n = values.size();
    if (n < 2) return true;
    if (n > std::numeric_limits<std::uint32_t>::max()) return false;

    constexpr unsigned table_size = 256;
    constexpr unsigned maximum_keys = 64;
    std::array<std::uint64_t, table_size> keys{};
    std::array<bool, table_size> occupied{};
    std::array<std::uint32_t, 4 * table_size> counts{};
    std::array<std::uint8_t, maximum_keys> ordered_slots{};
    unsigned distinct = 0;
    const auto hash = [](std::uint64_t value) {
        value ^= value >> 33;
        // Variant 0 preserves the initial experiment. Its near-2^64 constant
        // clusters small consecutive keys in adjacent high-byte slots. Golden
        // ratio multiplication spreads those keys while keeping one multiply.
        constexpr std::uint64_t multiplier = HashVariant == 0
            ? 0xff51afd7ed558ccdULL : 0x9e3779b97f4a7c15ULL;
        value *= multiplier;
        return static_cast<unsigned>(value >> 56);
    };
    const auto discover = [&](std::uint64_t value) {
        auto slot = hash(value);
        while (occupied[slot] && keys[slot] != value)
            slot = (slot + 1) & (table_size - 1);
        if (!occupied[slot]) {
            if (distinct == maximum_keys) return table_size;
            keys[slot] = value;
            occupied[slot] = true;
            ordered_slots[distinct++] = static_cast<std::uint8_t>(slot);
        }
        return slot;
    };
    std::size_t i = 0;
    for (; n - i >= 4; i += 4) {
        const auto a = discover(key(values[i]));
        if (a == table_size) return false;
        ++counts[a];
        const auto b = discover(key(values[i + 1]));
        if (b == table_size) return false;
        ++counts[table_size + b];
        const auto c = discover(key(values[i + 2]));
        if (c == table_size) return false;
        ++counts[2 * table_size + c];
        const auto d = discover(key(values[i + 3]));
        if (d == table_size) return false;
        ++counts[3 * table_size + d];
    }
    for (; i < n; ++i) {
        const auto slot = discover(key(values[i]));
        if (slot == table_size) return false;
        ++counts[slot];
    }
    if (distinct == 1) return true;
    std::sort(ordered_slots.begin(), ordered_slots.begin() + distinct,
        [&](unsigned a, unsigned b) { return keys[a] < keys[b]; });
    for (unsigned entry = 0; entry < distinct; ++entry) {
        const auto slot = ordered_slots[entry];
        counts[slot] += counts[table_size + slot] + counts[2 * table_size + slot] +
                        counts[3 * table_size + slot];
    }
    if constexpr (std::is_same_v<Record, std::uint64_t> &&
                  std::is_same_v<std::remove_cvref_t<Key>, identity_key64>) {
        std::size_t output = 0;
        for (unsigned entry = 0; entry < distinct; ++entry) {
            const auto slot = ordered_slots[entry];
            std::fill_n(values.data() + output, counts[slot], keys[slot]);
            output += counts[slot];
        }
    } else {
        std::uint32_t offset = 0;
        for (unsigned entry = 0; entry < distinct; ++entry) {
            const auto slot = ordered_slots[entry];
            const auto frequency = counts[slot];
            counts[slot] = offset;
            offset += frequency;
        }
        const auto find = [&](std::uint64_t value) {
            auto slot = hash(value);
            // All keys were discovered before this loop; the deterministic
            // extractor and absence of deletions guarantee a matching slot.
            while (keys[slot] != value) slot = (slot + 1) & (table_size - 1);
            return slot;
        };
        auto scratch = std::make_unique_for_overwrite<Record[]>(n);
        i = 0;
        for (; n - i >= 4; i += 4) {
            const auto a = values[i], b = values[i + 1];
            const auto c = values[i + 2], d = values[i + 3];
            const auto pa = counts[find(key(a))]++;
            const auto pb = counts[find(key(b))]++;
            const auto pc = counts[find(key(c))]++;
            const auto pd = counts[find(key(d))]++;
            scratch[pa] = a; scratch[pb] = b;
            scratch[pc] = c; scratch[pd] = d;
        }
        for (; i < n; ++i) {
            const auto record = values[i];
            scratch[counts[find(key(record))]++] = record;
        }
        std::copy_n(scratch.get(), n, values.data());
    }
    return true;
}

template<unsigned HashVariant = 1>
inline bool try_dictionary_values(std::span<std::uint64_t> values) {
    return ::radixkit::detail::wide::try_dictionary<HashVariant>(values, identity_key64{});
}

} // namespace radixkit::detail::wide
