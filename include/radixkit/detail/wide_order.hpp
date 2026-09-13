// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <span>
#include <type_traits>

namespace radixkit::detail::wide {

// Check the entire range in the requested direction, repairing short local
// inversions by insertion. True means the whole range is now ordered; false
// means the caller must run its general sorting fallback. Equal keys are in
// order in either direction. On false, earlier repairs may remain, but every
// complete record and its key/payload association is still present.
//
// move_budget counts writes to record-array elements: inserting across d
// records costs d+1 writes. Position searches happen before mutation, so an
// exhausted budget never leaves a displaced record or a hole in the array.
// An already ordered input needs no writes, even with a zero budget.
template<class Record, class Key>
inline bool bounded_order_repair(std::span<Record> values, Key key,
                                 bool descending = false,
                                 std::size_t move_budget = 64) {
    static_assert(std::is_trivially_copyable_v<Record>);
    using KeyValue = std::remove_cvref_t<
        std::invoke_result_t<Key, const Record&>>;
    const auto before = [descending](const KeyValue& a, const KeyValue& b) {
        return descending ? a > b : a < b;
    };
    if (values.size() < 2) return true;

    auto previous = key(values.front());
    for (std::size_t i = 1; i < values.size(); ++i) {
        const auto current = key(values[i]);
        if (!before(current, previous)) {
            previous = current;
            continue;
        }

        // The prefix [0,i) is ordered. Locate the insertion point while
        // bounding the eventual record writes, before opening a hole.
        std::size_t position = i;
        std::size_t distance = 0;
        do {
            if (distance >= move_budget || move_budget - distance <= 1)
                return false;
            --position;
            ++distance;
        } while (position && before(current, key(values[position - 1])));

        const auto held = values[i];
        std::copy_backward(values.data() + position, values.data() + i,
                           values.data() + i + 1);
        values[position] = held;
        move_budget -= distance + 1;
        // The old prefix's last key shifted into values[i], so `previous`
        // remains the correct key for the next adjacent-order comparison.
    }
    return true;
}

} // namespace radixkit::detail::wide
