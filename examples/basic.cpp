#include <radixkit/radixkit.hpp>
#include <iostream>
#include <vector>

int main() {
    std::vector<std::uint32_t> values{9, 1, 7, 7, 3};
    radixkit::nth_element(values, 2); // values[2] holds the third-smallest value.
    radixkit::partial_sort(values, 3); // The smallest three form a sorted prefix.
    radixkit::top_k(values, 3); // The first three values are 9, 7, 7, unordered.
    radixkit::sort(values);   // The complete array becomes 1, 3, 7, 7, 9.
    for (auto value : values) std::cout << value << ' ';
    std::cout << '\n';

    std::vector<std::uint64_t> keys{UINT64_MAX, 7, 0};
    radixkit::sort(keys);
    // Optional keys-only policy for repetitive, ordered or sparse-bit inputs.
    // Correct for every input, but performance depends on the distribution.
    radixkit::sort(keys, radixkit::sort_policy::adaptive);
    std::vector<radixkit::key_value64> rows{{9, 100}, {2, 200}, {9, 300}};
    radixkit::sort(rows);
    for (const auto& row : rows) std::cout << row.key << ':' << row.value << ' ';
    std::cout << '\n';
}
