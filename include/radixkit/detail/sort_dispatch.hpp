// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "key.hpp"
#include <span>
#include <utility>
#include "adaptive_keys.hpp"
#include "../sort_policy.hpp"

namespace radixkit::detail {
// Full sorting and prefix sorting enter through the same dispatcher. Only plain
// keys with our identity extractor may use the key-only champion/reconstruction.
template<class Record, class Key>
    requires key_projection<Record, std::remove_reference_t<Key>>
inline void sort_projected(std::span<Record> values, Key&& key) {
    if constexpr (unsigned_key<Record> && std::same_as<std::remove_cvref_t<Key>, default_key>) {
        if constexpr (std::same_as<Record, std::uint32_t>)
            scatter_radix_sort(values);
        else
            wide::adaptive_sort(values, wide::identity_key64{});
    } else {
        using KeyType = std::remove_cvref_t<decltype(key(std::declval<const Record&>()))>;
        if constexpr (std::same_as<KeyType, std::uint32_t>)
            radix64_sort<11, 2>(values, key);
        else
            wide::adaptive_sort(values, key);
    }
}

template<unsigned_key T>
inline void sort_with_policy(std::span<T> values, sort_policy policy) {
    if (policy == sort_policy::adaptive) {
        adaptive_keys::sort(values);
    } else if constexpr (std::same_as<T, std::uint32_t>) {
        scatter_radix_sort(values);
    } else {
        wide::sort_kernel(values, wide::identity_key64{});
    }
}
} // namespace radixkit::detail
