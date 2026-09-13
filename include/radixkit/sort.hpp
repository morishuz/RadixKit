// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "detail/sort_dispatch.hpp"

namespace radixkit {
// Concrete overloads preserve implicit vector/array-to-span conversion.
inline void sort(std::span<std::uint32_t> values) {
    detail::sort_projected(values, detail::default_key{});
}
inline void sort(std::span<std::uint64_t> values) {
    detail::sort_projected(values, detail::default_key{});
}
inline void sort(std::span<key_value32> values) {
    detail::sort_projected(values, detail::default_key{});
}
inline void sort(std::span<key_value64> values) {
    detail::sort_projected(values, detail::default_key{});
}
inline void sort(std::span<std::uint32_t> values, sort_policy policy) {
    detail::sort_with_policy(values, policy);
}
inline void sort(std::span<std::uint64_t> values, sort_policy policy) {
    detail::sort_with_policy(values, policy);
}

// The projection must be deterministic, non-mutating and non-throwing.
template<class Record, class Key, std::size_t Extent>
    requires detail::key_projection<Record, Key>
inline void sort_by_key(std::span<Record, Extent> values, Key key) {
    detail::sort_projected(std::span<Record>(values), key);
}
} // namespace radixkit
