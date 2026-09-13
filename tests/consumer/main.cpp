#include <cstddef>
#include <cstdint>
#include <span>
#include <radixkit/radixkit.hpp>
#include <array>
#include <algorithm>

int main() {
    std::array<std::uint32_t, 5> values{9, 1, 7, 7, 3};
    radixkit::topk(values, 3);
    for (std::size_t i = 0; i < 3; ++i) if (values[i] < 7) return 1;
    radixkit::sort(values);
    if (values != std::array<std::uint32_t, 5>{1, 3, 7, 7, 9}) return 1;
    radixkit::sort(values, radixkit::sort_policy::adaptive);
    if (values != std::array<std::uint32_t, 5>{1, 3, 7, 7, 9}) return 1;
    std::array<std::uint64_t, 3> wide{UINT64_MAX, 7, 0};
    radixkit::sort(wide);
    if (wide != std::array<std::uint64_t, 3>{0, 7, UINT64_MAX}) return 1;
    std::array<std::uint64_t, 1024> sparse{};
    for (std::size_t i=0;i<sparse.size();++i) sparse[i]=std::uint64_t((i*71)%256)<<48;
    auto expected=sparse;
    std::sort(expected.begin(),expected.end());
    radixkit::sort(sparse,radixkit::sort_policy::adaptive);
    if(sparse!=expected)return 1;
    std::reverse(sparse.begin(),sparse.end());
    radixkit::sort(sparse,radixkit::sort_policy::radix);
    if(sparse!=expected)return 1;
    std::array<radixkit::key_value64, 3> rows{{{9, 100}, {2, 200}, {7, 300}}};
    radixkit::sort(rows);
    if (rows != std::array<radixkit::key_value64, 3>{{{2, 200}, {7, 300}, {9, 100}}}) return 1;
    radixkit::nth_element(values, 2);
    if (values[2] != 7) return 1;
    radixkit::partial_sort(values, 2);
    if (values[0] != 1 || values[1] != 3) return 1;
    radixkit::top_k(wide, 1);
    if (wide[0] != UINT64_MAX) return 1;
    std::array<radixkit::key_value32, 3> pairs{{{9, 100}, {2, 200}, {7, 300}}};
    radixkit::partial_sort(pairs, 2);
    if (pairs[0].key != 2 || pairs[0].value != 200 || pairs[1].key != 7) return 1;
    radixkit::nth_element_by_key(std::span(pairs), 1, [](const auto& r) { return r.key; });
    if (pairs[1].key != 7 || pairs[1].value != 300) return 1;
    radixkit::top_k_by_key(std::span(pairs), 1, [](const auto& r) { return r.key; });
    if (pairs[0].key != 9 || pairs[0].value != 100) return 1;
    struct row32 { std::uint32_t key; std::uint64_t payload; };
    std::array<row32, 3> narrow{{{9, 100}, {2, 200}, {7, 300}}};
    radixkit::sort_by_key(std::span(narrow), [](const row32& r) -> std::uint64_t { return r.key; });
    return narrow[0].key != 2 || narrow[0].payload != 200 ||
           narrow[1].key != 7 || narrow[1].payload != 300 ||
           narrow[2].key != 9 || narrow[2].payload != 100;
}
