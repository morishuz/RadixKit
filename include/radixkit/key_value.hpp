// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include <cstdint>

namespace radixkit {
struct key_value32 {
    std::uint32_t key;
    std::uint64_t value;
    bool operator==(const key_value32&) const = default;
};
struct key_value64 {
    std::uint64_t key;
    std::uint64_t value;
    bool operator==(const key_value64&) const = default;
};
} // namespace radixkit
