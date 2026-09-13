#include <radixkit/sort64.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using U64 = std::uint64_t;
constexpr U64 maximum = UINT64_MAX;
constexpr U64 payload_mask = 0xd1b54a32d192ed03ULL;
std::size_t cases = 0;
std::size_t checks = 0;

void require(bool condition, const std::string& label) {
    if (!condition) throw std::runtime_error(label);
}

// Include both endpoints, matching the documented 32-position sampling grid.
// These positions construct adversarial inputs; only public sorting APIs run.
std::size_t sample_index(std::size_t n, std::size_t i) {
    return i * ((n - 1) / 31) + i * ((n - 1) % 31) / 31;
}

void check(const std::vector<U64>& input, const std::string& label) {
    auto expected = input;
    std::sort(expected.begin(), expected.end());
    auto actual = input;
    radixkit::sort(std::span(actual));
    require(actual == expected, label + ": scalar values");
    ++checks;

    std::vector<radixkit::key_value64> records(input.size());
    for (std::size_t i = 0; i < input.size(); ++i)
        records[i] = {input[i], U64(i) ^ payload_mask};
    radixkit::sort(std::span(records));
    std::vector<unsigned char> seen(input.size());
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto id = records[i].value ^ payload_mask;
        require(records[i].key == expected[i] && id < input.size(),
                label + ": pair key order or payload bounds");
        require(!seen[id] && records[i].key == input[id],
                label + ": pair association or duplicate payload");
        seen[id] = 1;
    }
    ++checks;
    ++cases;
}

template<class Key>
void check_packed(std::vector<U64> input, Key key, const std::string& label) {
    auto expected = input;
    std::sort(expected.begin(), expected.end());
    radixkit::sort_by_key(std::span(input), key);
    require(std::is_sorted(input.begin(), input.end(),
                [&](U64 a, U64 b) { return key(a) < key(b); }),
            label + ": packed key order");
    // Canonicalize only after checking key order. Equal-key order is unspecified.
    std::sort(input.begin(), input.end());
    require(input == expected, label + ": packed payload preservation");
    ++checks;
    ++cases;
}

void check_large_opaque(std::mt19937_64& random) {
    struct alignas(32) Record {
        U64 payload, key, other, id;
        bool operator==(const Record&) const = default;
    };
    // Just above the 16 MiB dispatch threshold for a 32-byte record.
    constexpr std::size_t n = 524289;
    std::vector<Record> records(n);
    for (std::size_t i = 0; i < n; ++i)
        records[i] = {random(), random(), random(), i};
    // Ensure a diverse high prefix at the secondary sample positions.
    for (std::size_t i = 0; i < 16; ++i) {
        auto& record = records[(n - 1) / 15 * i];
        record.key = (U64(i * 67) << 54) | (record.key & ((U64(1) << 54) - 1));
    }
    const auto original = records;
    std::vector<U64> expected;
    expected.reserve(n);
    for (const auto& record : records) expected.push_back(record.key);
    std::sort(expected.begin(), expected.end());
    radixkit::sort_by_key(std::span(records), [](const Record& record) { return record.key; });
    std::vector<unsigned char> seen(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto id = records[i].id;
        require(records[i].key == expected[i] && id < n,
                "large opaque record: key order or ID bounds");
        require(!seen[id] && records[i] == original[id],
                "large opaque record: complete record preservation");
        seen[id] = 1;
    }
    ++checks;
    ++cases;
}
} // namespace

int main() {
    try {
        std::mt19937_64 random(20260912);
        for (std::size_t n : {0, 1, 2, 7, 127, 128, 129, 4095, 4096, 4097}) {
            std::vector<U64> values(n);
            for (auto& value : values) value = random();
            check(values, "size boundary " + std::to_string(n));
        }

        constexpr std::size_t n = 4097;
        for (unsigned pattern = 0; pattern < 5; ++pattern) {
            std::vector<U64> values(n);
            for (std::size_t i = 0; i < n; ++i) {
                if (pattern == 0) values[i] = 0;
                if (pattern == 1) values[i] = maximum;
                if (pattern == 2) values[i] = i / 3;
                if (pattern == 3) values[i] = (n - 1 - i) / 3;
                if (pattern == 4) values[i] = i % 2 ? maximum : 0;
            }
            check(values, "ordered/equal pattern " + std::to_string(pattern));
        }

        for (unsigned pattern = 0; pattern < 4; ++pattern) {
            std::vector<U64> values(n);
            for (std::size_t i = 0; i < n; ++i) values[i] = i / 3;
            if (pattern == 1) std::reverse(values.begin(), values.end());
            if (pattern < 2) std::swap(values[n - 6], values[n - 2]);
            if (pattern == 2) std::swap(values[2], values[3]);
            if (pattern == 3) {
                std::swap(values[2], values[3]); // A repair can succeed first.
                std::swap(values[n - 100], values[n - 2]); // Exceeds the write budget.
            }
            check(values, "local inversion " + std::to_string(pattern));
        }

        for (unsigned pattern = 0; pattern < 6; ++pattern) {
            std::vector<U64> values(n);
            for (auto& value : values) value = random();
            if (pattern < 4) {
                for (std::size_t i = 0; i < 32; ++i) {
                    U64 sample = U64(1) << 63;
                    if (pattern == 1) sample = i * (maximum / 32);
                    if (pattern == 2) sample = (31 - i) * (maximum / 32);
                    if (pattern == 3) sample = 100 + (i * 7) % 16;
                    values[sample_index(n, i)] = sample;
                }
            } else if (pattern == 4) {
                std::fill(values.begin(), values.end(), U64(1) << 63);
                values[1] = 0;
                values[n - 2] = maximum;
            } else {
                for (auto& value : values) value = maximum - (random() & 15);
                values[n - 2] = 0;
            }
            check(values, "sampling trap " + std::to_string(pattern));
        }

        for (unsigned pattern = 0; pattern < 5; ++pattern) {
            U64 minimum = maximum - 15;
            U64 width = 16;
            if (pattern == 1) { minimum = (U64(1) << 32) - 127; width = 256; }
            if (pattern == 2) { minimum = (U64(1) << 63) - 127; width = 256; }
            if (pattern == 3) { minimum = maximum - 1023; width = 1024; }
            if (pattern == 4) { minimum = maximum - 1024; width = 1025; }
            std::vector<U64> values(n);
            for (auto& value : values) value = minimum + random() % width;
            values.front() = minimum;
            values.back() = minimum + width - 1;
            check(values, "shifted range " + std::to_string(pattern));
        }
        for (std::size_t count : {262143, 262144, 262145}) {
            std::vector<U64> values(count);
            for (auto& value : values) value = maximum - (random() & 65535);
            values.front() = maximum - 65535;
            values.back() = maximum;
            check(values, "maximum counting range cutoff " + std::to_string(count));
        }

        for (unsigned cardinality : {16, 24, 32, 64, 65}) {
            constexpr U64 multiplier = 0x9e3779b97f4a7c15ULL;
            std::vector<U64> values(n);
            for (std::size_t i = 0; i < n; ++i)
                values[i] = U64(i % std::min(cardinality, 64u)) * multiplier;
            // A small sampled alphabet must not hide the actual dictionary size.
            for (std::size_t i = 0; i < 32; ++i)
                values[sample_index(n, i)] = U64((i * 7) % 16) * multiplier;
            if (cardinality == 65) values[n - 2] = U64(64) * multiplier;
            check(values, "dictionary capacity " + std::to_string(cardinality));
        }

        for (U64 pivot : {U64(0), U64(1) << 63, maximum}) {
            for (bool clustered : {false, true}) {
                std::vector<U64> values(n, pivot);
                for (std::size_t i = 0; i < n / 100; ++i) {
                    const auto index = clustered ? i : 1 + i * 100;
                    values[index] = i % 2 ? maximum : 0;
                }
                check(values, "dominant pivot " + std::to_string(pivot) +
                    (clustered ? " clustered" : " interleaved"));
            }
        }

        const auto small_key = [](U64 value) -> U64 { return value >> 60; };
        for (unsigned pattern = 0; pattern < 4; ++pattern) {
            std::vector<U64> values(n);
            for (std::size_t i = 0; i < n; ++i) {
                U64 key = random() % 16;
                if (pattern == 1) key = 0;
                if (pattern == 2) key = std::min<std::size_t>(i / 256, 15);
                values[i] = (key << 60) | (random() & ((U64(1) << 60) - 1));
            }
            if (pattern == 2) {
                std::reverse(values.begin(), values.end());
                std::swap(values[n - 258], values[n - 256]);
            }
            if (pattern == 3)
                for (std::size_t i = 0; i < 32; ++i)
                    values[sample_index(n, i)] &= (U64(1) << 60) - 1;
            check_packed(values, small_key, "packed narrow key " + std::to_string(pattern));
        }
        {
            constexpr U64 low_mask = (U64(1) << 58) - 1;
            std::vector<U64> values(n);
            for (std::size_t i = 0; i < n; ++i)
                values[i] = (U64(i % 64) << 58) | (random() & low_mask);
            for (std::size_t i = 0; i < 32; ++i)
                values[sample_index(n, i)] = (U64((i * 7) % 16) << 58) | (random() & low_mask);
            check_packed(values, [](U64 value) -> U64 { return value & ~low_mask; },
                         "packed full-width dictionary keys");
        }
        check_large_opaque(random);

        std::cout << "Passed " << checks << " public wide-sort checks across "
                  << cases << " adaptive input cases\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
