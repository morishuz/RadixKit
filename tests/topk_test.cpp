#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <radixkit/topk.hpp>
#include <iostream>
#include <random>
#include <vector>

int main() {
    std::mt19937 rng(12345);
    std::size_t cases = 0;
    for (std::size_t n : {0, 1, 2, 3, 8, 31, 256, 1024}) {
        for (int trial = 0; trial < 40; ++trial) {
            std::vector<std::uint32_t> input(n);
            for (auto& v : input) v = trial % 3 == 0 ? rng() % 4 : rng();
            if (trial == 1) std::fill(input.begin(), input.end(), UINT32_MAX);
            if (trial == 2) std::sort(input.begin(), input.end());
            if (trial == 3) std::sort(input.begin(), input.end(), std::greater<>{});
            auto reference = input;
            std::sort(reference.begin(), reference.end(), std::greater<>{});
            for (std::size_t k = 0; k <= n; ++k) {
                for (auto fn : {radixkit::topk}) {
                    auto actual = input;
                    fn(actual, k);
                    if ((k == 0 || k == n) && actual != input) return 1;
                    std::sort(actual.begin(), actual.begin() + k, std::greater<>{});
                    std::sort(actual.begin() + k, actual.end(), std::greater<>{});
                    if (actual != reference) {
                        std::cerr << "Failure n=" << n << " k=" << k << '\n';
                        return 1;
                    }
                    ++cases;
                }
            }
        }
    }
    for (auto fn : {radixkit::topk}) {
        for (auto input : {std::vector<std::uint32_t>{}, std::vector<std::uint32_t>{3, 1, 2}}) {
            const auto before = input;
            try { fn(input, input.size() + 1); return 1; } catch (const std::out_of_range&) {}
            if (input != before) return 1;
        }
    }
    std::cout << "Passed " << cases << " differential checks and invalid-k checks\n";
}
