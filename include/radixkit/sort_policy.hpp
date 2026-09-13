// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once

namespace radixkit {
enum class sort_policy {
    radix,   // Use the radix kernel without the optional adaptive front end.
    adaptive // Exact shortcuts for order, duplicates and few varying bits.
};
} // namespace radixkit
