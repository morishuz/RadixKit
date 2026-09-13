// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "sort_dispatch.hpp"
#include "select.hpp"

namespace radixkit::detail {
// The public boundary has already checked k <= size. Keep the measured tiny-heap
// strategy here, beside selection + prefix-sort composition, rather than in APIs.
template<class Record, class Key>
    requires key_projection<Record, Key>
inline void partial_sort_projected(std::span<Record> values, std::size_t k, Key& key) {
    if (!k) return;
    // A tiny heap avoids radix histogram/partition overhead for small prefixes.
    if (k <= 32 && k != values.size()) {
        std::partial_sort(values.begin(), values.begin() + k, values.end(),
            [&](const Record& a, const Record& b) { return key(a) < key(b); });
        return;
    }
    auto prefix = values.first(k);
    if (k != values.size())
        radix_select<selection_mode::smallest_prefix>(values, k - 1, key);
    sort_projected(prefix, key);
}
} // namespace radixkit::detail
