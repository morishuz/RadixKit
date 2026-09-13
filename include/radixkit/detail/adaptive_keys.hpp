// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "scatter_radix.hpp"
#include "wide_dispatch.hpp"
#include <bit>

namespace radixkit::detail::adaptive_keys {
// Preserve the published defaults as the fallback, including uint64 dictionary
// handling. These never call the optional policy recursively.
template<class T>
void fallback(std::span<T> values) {
    if constexpr (sizeof(T) == 4) scatter_radix_sort(values);
    else wide::adaptive_sort(values, wide::identity_key64{});
}

// Exact, read-only validation. A rejection must precede counter allocation.
template<class T>
bool valid_domain(std::span<T> values, T first, T mask) {
    for (std::size_t begin = 0; begin < values.size();) {
        const auto end = begin + std::min(std::size_t(256), values.size() - begin);
        T invalid = 0;
        for (auto i = begin; i < end; ++i) invalid |= (values[i] ^ first) & ~mask;
        if (invalid) return false;
        begin = end;
    }
    return true;
}
// Small inputs fuse validation/counting. Large inputs are validated first;
// only the verified path may omit checks during counting.
template<bool Validate, class Counter, class T, class Pack>
bool count_and_reconstruct(std::span<T> values, T first, T mask,
                           Counter* counts, std::size_t domain, Pack pack) {
    for (std::size_t begin = 0; begin < values.size();) {
        const auto end = begin + std::min(std::size_t(256), values.size() - begin);
        T invalid = 0;
        auto i = begin;
        for (; end - i >= 4; i += 4) {
            for (unsigned lane = 0; lane < 4; ++lane) {
                const auto x = values[i + lane];
                if constexpr (Validate) invalid |= (x ^ first) & ~mask;
                ++counts[lane * domain + pack(x)];
            }
        }
        for (; i < end; ++i) {
            const auto x = values[i];
            if constexpr (Validate) invalid |= (x ^ first) & ~mask;
            ++counts[pack(x)];
        }
        if (invalid) return false;
        begin = end;
    }
    const T base = first & ~mask;
    std::size_t out = 0;
    // Deposit successive dense indices into mask positions. Updating only the
    // varying bits enumerates the domain in numeric order without per-bin loops.
    T deposited = 0;
    for (std::size_t b = 0; b < domain; ++b) {
        const std::size_t frequency = std::size_t(counts[b]) + counts[domain + b] +
                                     counts[2 * domain + b] + counts[3 * domain + b];
        std::fill_n(values.data() + out, frequency, base | deposited);
        out += frequency;
        deposited = (deposited - mask) & mask; // unsigned arithmetic, including wrap
    }
    return true;
}

template<bool Validate, class T>
bool histogram(std::span<T> values, T first, T mask) {
    const unsigned bits = std::popcount(mask);
    if (!bits || bits > 16) return false;
    const auto domain = std::size_t(1) << bits;

    const auto shift = std::countr_zero(mask);
    const bool contiguous = (mask >> shift) == T(domain - 1);
    // Small domains need no heap allocation. Use the same counters for all
    // sizes: each lane <=N, and N is bounded by UINT32_MAX at the entry point.
    std::array<std::uint32_t, 1024> local{};
    std::unique_ptr<std::uint32_t[]> allocated;
    if (domain > 256) allocated = std::make_unique<std::uint32_t[]>(4 * domain);
    auto* counts = domain <= 256 ? local.data() : allocated.get();
    if (contiguous) {
        return count_and_reconstruct<Validate>(values, first, mask, counts, domain,
            [shift, domain](T x) { return std::size_t((x >> shift) & (domain - 1)); });
    }
    if (values.size() <= 4096 && bits <= 8) {
        std::array<unsigned, 8> positions{};
        unsigned j = 0;
        for (T m = mask; m; m &= m - 1) positions[j++] = std::countr_zero(m);
        return count_and_reconstruct<Validate>(values, first, mask, counts, domain, [&](T x) {
            unsigned packed = 0;
            for (unsigned k = 0; k < bits; ++k) packed |= unsigned((x >> positions[k]) & 1) << k;
            return packed;
        });
    }
    std::array<std::array<std::uint16_t, 256>, sizeof(T)> lookup{};
    unsigned dense_bit = 0;
    // Recurrence replaces the earlier O(256 * key_bits * sizeof(T)) table setup.
    for (unsigned byte = 0; byte < sizeof(T); ++byte) {
        std::array<std::uint16_t, 8> weights{};
        for (unsigned bit = 0; bit < 8; ++bit)
            if ((mask >> (8 * byte + bit)) & 1) weights[bit] = std::uint16_t(1u << dense_bit++);
        for (unsigned x = 1; x < 256; ++x)
            lookup[byte][x] = lookup[byte][x & (x - 1)] | weights[std::countr_zero(x)];
    }
    return count_and_reconstruct<Validate>(values, first, mask, counts, domain, [&](T x) {
        std::uint16_t packed = 0;
        for (unsigned byte = 0; byte < sizeof(T); ++byte) packed |= lookup[byte][(x >> (byte * 8)) & 255];
        return packed;
    });
}

// Large domains need enough input to amortize zeroing and reconstruction.
// This is a performance heuristic, not a promise about all CPUs/distributions.
template<class T>
bool try_histogram(std::span<T> values, T first, T mask) {
    const unsigned bits = std::popcount(mask);
    if (!bits || bits > 16 || values.size() > UINT32_MAX) return false;
    const auto domain = std::size_t(1) << bits;
    const auto density = bits <= 8 ? 2u : (sizeof(T) == 4 ? 16u : 8u);
    if (domain > values.size() / density) return false;
    if (values.size() <= 4096) return histogram<true>(values, first, mask);
    // On large inputs validate first: a late outlier then costs a read-only
    // scan, without allocating/initializing counters or doing random writes.
    if (!valid_domain(values, first, mask)) return false;
    return histogram<false>(values, first, mask);
}

template<class T>
bool all_equal(std::span<T> values, T first) {
    for (std::size_t begin = 0; begin < values.size();) {
        T differences = 0;
        const auto end = begin + std::min(std::size_t(256), values.size() - begin);
        for (auto i = begin; i < end; ++i) differences |= values[i] ^ first;
        if (differences) return false;
        begin = end;
    }
    return true;
}

template<bool Descending, class T>
bool check_and_repair(std::span<T> values) {
    const auto before = [](T a, T b) {
        if constexpr (Descending) return a > b;
        else return a < b;
    };
    const auto inversion = std::is_sorted_until(values.begin(), values.end(), before);
    if (inversion == values.end()) return true;
    const auto index = static_cast<std::size_t>(inversion - values.begin());
    constexpr std::size_t budget = 64;
    // The prefix is proved ordered. Keep budget preceding elements so that an
    // insertion crossing the checked boundary would require at least budget+1
    // writes and MUST fail. No successful repair can invalidate that prefix.
    // This avoids rescanning a potentially huge prefix before the first repair.
    const auto start = index > budget ? index - budget : 0;
    return wide::bounded_order_repair(values.subspan(start), [](T x) { return x; },
                                      Descending, budget);
}

template<class T>
void sort(std::span<T> values) {
    static_assert(std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>);
    const auto n = values.size();
    if (n < 512) {
        // Preserve the previous uint32 policy's cheap tiny-input order paths;
        // do not allocate a radix buffer merely because this is below the
        // histogram cutoff. The same checks also benefit small uint64 inputs.
        if (std::is_sorted(values.begin(), values.end())) return;
        if (std::is_sorted(values.begin(), values.end(), std::greater<>{})) {
            std::reverse(values.begin(), values.end()); return;
        }
        fallback(values); return;
    }
    if (n > UINT32_MAX) { fallback(values); return; }
    const T first = values[0];
    std::array<T, 32> sample{};
    T mask = 0;
    bool ascending = true, descending = true;
    const auto step = (n - 1) / 31, remainder = (n - 1) % 31;
    for (std::size_t i = 0; i < sample.size(); ++i) {
        sample[i] = values[i * step + i * remainder / 31];
        mask |= sample[i] ^ first;
        if (i) { ascending &= sample[i - 1] <= sample[i]; descending &= sample[i - 1] >= sample[i]; }
    }
    if (!mask && all_equal(values, first)) return;
    T majority = first;
    unsigned votes = 0;
    for (auto x : sample) {
        if (!votes) majority = x;
        if (x == majority) ++votes; else --votes;
    }
    bool dominated = std::count(sample.begin(), sample.end(), majority) >= 28;
    if (ascending || descending || dominated) {
        bool local_up = true, local_down = true;
        unsigned matches = values[0] == majority;
        for (std::size_t i = 1; i < 64; ++i) {
            local_up &= values[i - 1] <= values[i]; local_down &= values[i - 1] >= values[i];
            matches += values[i] == majority;
        }
        ascending &= local_up; descending &= local_down; dominated &= matches >= 56;
    }
    const auto key = [](T x) { return x; };
    if (sizeof(T) == 4 || n <= 4096) {
        // Simple order checks avoid the repair-loop overhead for narrow keys
        // and small wide-key arrays. Repair only after finding an inversion.
        if (ascending && check_and_repair<false>(values)) return;
        if (descending && check_and_repair<true>(values)) {
            std::reverse(values.begin(), values.end()); return;
        }
    } else {
        if (ascending && wide::bounded_order_repair(values, key)) return;
        if (descending && wide::bounded_order_repair(values, key, true)) {
            std::reverse(values.begin(), values.end()); return;
        }
    }
    if (dominated) {
        auto equal = values.begin();
        if (majority != 0) equal = std::partition(values.begin(), values.end(), [majority](T x) { return x < majority; });
        auto upper = values.end();
        if (majority != std::numeric_limits<T>::max()) upper = std::partition(equal, values.end(), [majority](T x) { return x == majority; });
        fallback(values.first(static_cast<std::size_t>(equal - values.begin())));
        fallback(values.subspan(static_cast<std::size_t>(upper - values.begin())));
        return;
    }
    if (try_histogram(values, first, mask)) return;
    fallback(values);
}
} // namespace radixkit::detail::adaptive_keys
