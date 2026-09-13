// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "sort.hpp"
#include "detail/partial_sort.hpp"
#include <stdexcept>

namespace radixkit {
namespace detail {
inline void check_position(std::size_t position, std::size_t size) {
    if (position > size) throw std::out_of_range("position exceeds input size");
}
} // namespace detail

// nth is a zero-based rank. nth == size is a no-op, like std::nth_element(end).
template<class Record, std::size_t Extent, class Key>
    requires detail::key_projection<Record, Key>
void nth_element_by_key(std::span<Record, Extent> values, std::size_t nth, Key key) {
    detail::check_position(nth, values.size());
    if (nth != values.size() &&
        !detail::select_near_end(std::span<Record>(values), nth, key))
        detail::radix_select<detail::selection_mode::nth>(std::span<Record>(values), nth, key);
}

template<class Record, std::size_t Extent, class Key>
    requires detail::key_projection<Record, Key>
void top_k_by_key(std::span<Record, Extent> values, std::size_t k, Key key) {
    detail::check_position(k, values.size());
    if (k && k != values.size())
        detail::radix_select<detail::selection_mode::largest_prefix>(
            std::span<Record>(values), k - 1, key);
}

template<class Record, std::size_t Extent, class Key>
    requires detail::key_projection<Record, Key>
void partial_sort_by_key(std::span<Record, Extent> values, std::size_t k, Key key) {
    detail::check_position(k, values.size());
    detail::partial_sort_projected(std::span<Record>(values), k, key);
}

template<detail::keyed_range R>
void nth_element(R&& values, std::size_t nth) {
    radixkit::nth_element_by_key(std::span(values), nth, detail::default_key{});
}
template<detail::keyed_range R>
void partial_sort(R&& values, std::size_t k) {
    radixkit::partial_sort_by_key(std::span(values), k, detail::default_key{});
}
template<detail::keyed_range R>
void top_k(R&& values, std::size_t k) {
    radixkit::top_k_by_key(std::span(values), k, detail::default_key{});
}
// Preserve the original concrete overload as well as its spelling.
inline void topk(std::span<std::uint32_t> values, std::size_t k) {
    radixkit::top_k(values, k);
}
} // namespace radixkit
