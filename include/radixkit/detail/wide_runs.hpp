// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "radix64.hpp"

namespace radixkit::detail::wide {
template<unsigned Bits, unsigned Lanes, class Record, class Key>
inline void radix64_runs_sort(std::span<Record> values, Key key) {
    radix64_sort_impl<Bits, Lanes, true>(values, key);
}
} // namespace radixkit::detail::wide
