#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <radixkit/radixkit.hpp>
#include <radixkit/sort64.hpp>
#include <radixkit/topk.hpp>
#include <array>
#include <span>
#include <string>
#include <vector>

template<class R> concept selectable = requires(R&& rows) {
    radixkit::nth_element(rows, 0);
    radixkit::partial_sort(rows, 0);
    radixkit::top_k(rows, 0);
};
template<class T, class K> concept projectable = requires(std::span<T> rows, K key) {
    radixkit::sort_by_key(rows, key);
    radixkit::nth_element_by_key(rows, 0, key);
    radixkit::partial_sort_by_key(rows, 0, key);
    radixkit::top_k_by_key(rows, 0, key);
};
struct row { std::uint32_t key; std::uint64_t payload; };
struct narrow_key { std::uint32_t operator()(const row& r) const { return r.key; } };
struct wide_key { std::uint64_t operator()(const row& r) const { return r.key; } };
struct bad_key { std::uint16_t operator()(const row&) const { return 0; } };
struct mutable_key { std::uint32_t operator()(row&) const { return 0; } };
struct owning_row { std::uint64_t key; std::string payload; };
struct owning_key { std::uint64_t operator()(const owning_row& r) const { return r.key; } };
struct move_only_row {
    std::uint32_t key;
    move_only_row() = default;
    move_only_row(const move_only_row&) = delete;
    move_only_row(move_only_row&&) = default;
    move_only_row& operator=(const move_only_row&) = delete;
    move_only_row& operator=(move_only_row&&) = default;
};
struct move_only_key {
    std::uint32_t operator()(const move_only_row& r) const { return r.key; }
};
static_assert(std::is_trivially_copyable_v<move_only_row>);
static_assert(!projectable<move_only_row, move_only_key>);
struct convertible { operator std::uint32_t() const { return 0; } };
static_assert(selectable<std::vector<std::uint32_t>&>);
static_assert(selectable<std::array<radixkit::key_value32, 4>&>);
static_assert(selectable<std::span<radixkit::key_value64>>);
static_assert(!selectable<const std::vector<std::uint32_t>&>);
static_assert(!selectable<std::span<const std::uint64_t>>);
static_assert(!selectable<std::span<volatile std::uint64_t>>);
static_assert(!selectable<std::vector<std::int32_t>&>);
static_assert(!selectable<std::vector<float>&>);
static_assert(!selectable<std::vector<convertible>&>);
static_assert(projectable<row, narrow_key> && projectable<row, wide_key>);
static_assert(!projectable<const row, narrow_key>);
static_assert(!projectable<row, bad_key> && !projectable<row, mutable_key>);
static_assert(!projectable<owning_row, owning_key>);

template<class K> bool projected_payloads(std::size_t selected) {
    std::vector<std::uint64_t> input(8193);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = (std::uint64_t(i) << 32) | ((i * 71) % 16);
    auto project = [](std::uint64_t value) { return K(value & 255); };
    auto expected = input;
    std::sort(expected.begin(), expected.end(), [&](auto a, auto b) { return project(a) < project(b); });
    for (int operation = 0; operation < 4; ++operation) {
        auto values = input;
        if (operation == 0) radixkit::sort_by_key(std::span(values), project);
        if (operation == 1) radixkit::partial_sort_by_key(std::span(values), selected, project);
        if (operation == 2) radixkit::nth_element_by_key(std::span(values), selected, project);
        if (operation == 3) radixkit::top_k_by_key(std::span(values), selected, project);
        if (operation <= 1) {
            for (std::size_t i = 0; i < (operation == 0 ? values.size() : selected); ++i)
                if (project(values[i]) != project(expected[i])) return false;
        } else if (operation == 2) {
            if (project(values[selected]) != project(expected[selected])) return false;
            for (std::size_t i = 0; i < selected; ++i)
                if (project(values[i]) > project(values[selected])) return false;
            for (std::size_t i = selected + 1; i < values.size(); ++i)
                if (project(values[i]) < project(values[selected])) return false;
        } else {
            std::sort(values.begin(), values.begin() + selected,
                [&](auto a, auto b) { return project(a) < project(b); });
            for (std::size_t i = 0; i < selected; ++i)
                if (project(values[i]) != project(expected[values.size() - selected + i])) return false;
        }
        std::sort(values.begin(), values.end());
        if (values != input) return false; // Includes the payload bits above the key.
    }
    return true;
}

int main() {
    for (std::size_t selected : {1, 4096, 8191})
        if (!projected_payloads<std::uint32_t>(selected) ||
            !projected_payloads<std::uint64_t>(selected)) return 1;
    // Keep the original concrete function-address expression source-compatible.
    auto old_topk = &radixkit::topk;
    std::vector<std::uint32_t> values{3, 1, 2};
    old_topk(values, 1);
    if (values[0] != 3) return 1;
    // Projecting a scalar must move original values, not reconstruct projected keys.
    std::array<std::uint64_t, 4> projected{0x102, 0x200, 0x301, 0x400};
    radixkit::sort_by_key(std::span(projected), [](std::uint64_t x) {
        return std::uint32_t(x & 255);
    });
    auto original = projected;
    std::sort(original.begin(), original.end());
    if (original != std::array<std::uint64_t, 4>{0x102, 0x200, 0x301, 0x400}) return 1;
    for (std::size_t i = 1; i < projected.size(); ++i)
        if ((projected[i - 1] & 255) > (projected[i] & 255)) return 1;
}
