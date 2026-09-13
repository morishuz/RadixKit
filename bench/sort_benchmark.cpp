// SPDX-License-Identifier: MIT
#include <radixkit/radixkit.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <ska_sort.hpp>

template<class T> auto key(const T& row) {
    if constexpr (std::is_integral_v<T>) return row;
    else return row.key;
}

template<class T> void run(const char* type, std::size_t n, int repeats, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    const char* distributions[] = {"uniform", "uniform16", "zeros99", "equal",
                                   "ascending", "descending", "sparse8", "nearly_sorted"};
    const char* methods[] = {"std_sort", "ska_sort", "ska_sort_copy", "radixkit", "adaptive"};
    const unsigned method_count = std::is_integral_v<T> ? 5 : 4;
    auto extract = [](const T& row) { return key(row); };
    auto less = [&](const T& a, const T& b) { return key(a) < key(b); };
    for (unsigned distribution = 0; distribution < 8; ++distribution) {
        using K = decltype(key(T{}));
        std::vector<T> input(n);
        for (std::size_t i = 0; i < n; ++i) {
            K value = static_cast<K>(rng());
            if (distribution == 1) value &= 65535;
            if (distribution == 2 && i % 100) value = 0;
            if (distribution == 3) value = 7;
            if (distribution == 4 || distribution == 7) value = static_cast<K>(i);
            if (distribution == 5) value = static_cast<K>(n - i);
            if (distribution == 6) {
                K sparse = 0;
                for (unsigned bit = 0; bit < 8; ++bit)
                    sparse |= K((value >> bit) & 1) << (bit * sizeof(K));
                value = sparse;
            }
            if constexpr (std::is_integral_v<T>) input[i] = value;
            else input[i] = {value, i};
        }
        if (distribution == 7 && n) for (unsigned i = 0; i < 32; ++i) {
            auto a = rng() % n, b = rng() % n;
            if constexpr (std::is_integral_v<T>) std::swap(input[a], input[b]);
            else std::swap(input[a].key, input[b].key);
        }
        auto expected = input;
        std::sort(expected.begin(), expected.end(), less);
        for (int repeat = -1; repeat < repeats; ++repeat) {
            for (unsigned slot = 0; slot < method_count; ++slot) {
                const auto method = (slot + repeat + 1) % method_count;
                auto values = input; // Same data; input copying is outside timing.
                const auto start = std::chrono::steady_clock::now();
                if (method == 0) {
                    if constexpr (std::is_integral_v<T>) std::sort(values.begin(), values.end());
                    else std::sort(values.begin(), values.end(), less);
                }
                if (method == 1) ska_sort(values.begin(), values.end(), extract);
                if (method == 2) {
                    auto scratch = std::make_unique_for_overwrite<T[]>(n);
                    if (ska_sort_copy(values.begin(), values.end(), scratch.get(), extract))
                        std::copy_n(scratch.get(), n, values.begin());
                }
                if (method == 3) radixkit::sort(values);
                if constexpr (std::is_integral_v<T>)
                    if (method == 4) radixkit::sort(values, radixkit::sort_policy::adaptive);
                const auto stop = std::chrono::steady_clock::now();
                // Validate sorted keys and the entire permutation outside timing.
                for (std::size_t i = 0; i < n; ++i)
                    if (key(values[i]) != key(expected[i])) throw std::runtime_error("incorrect keys");
                if constexpr (!std::is_integral_v<T>) {
                    std::vector<bool> seen(n);
                    for (const auto& row : values) {
                        if (row.value >= n || seen[row.value] || row.key != input[row.value].key)
                            throw std::runtime_error("incorrect payload permutation");
                        seen[row.value] = true;
                    }
                }
                if (repeat >= 0)
                    std::cout << type << ',' << n << ',' << seed << ',' << distributions[distribution]
                              << ',' << repeat << ',' << methods[method] << ','
                              << std::chrono::duration<double, std::milli>(stop - start).count() << '\n';
            }
        }
    }
}

int main(int argc, char** argv) {
    try {
        const std::size_t n = argc > 1 ? std::stoull(argv[1]) : 1000000;
        const int repeats = argc > 2 ? std::stoi(argv[2]) : 7;
        const auto seed = argc > 3 ? std::stoull(argv[3]) : 13579;
        if (n < 128) throw std::invalid_argument("benchmark size must be at least 128");
        if (repeats < 1) throw std::invalid_argument("repeats must be positive");
        std::cout << std::setprecision(9) << "type,n,seed,distribution,repeat,method,ms\n";
        run<std::uint32_t>("u32", n, repeats, seed);
        run<std::uint64_t>("u64", n, repeats, seed);
        run<radixkit::key_value32>("kv32", n, repeats, seed);
        run<radixkit::key_value64>("kv64", n, repeats, seed);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
