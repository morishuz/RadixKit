// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "../key_value.hpp"
#include <concepts>
#include <ranges>
#include <type_traits>

namespace radixkit::detail {
template<class T>
concept unsigned_key = std::same_as<std::remove_cvref_t<T>, std::uint32_t> ||
                       std::same_as<std::remove_cvref_t<T>, std::uint64_t>;

template<class T>
concept sortable_record = std::same_as<T, std::remove_cv_t<T>> &&
                          std::is_trivially_copyable_v<T> &&
                          std::is_trivially_default_constructible_v<T> && std::copyable<T>;

template<class Record, class Key>
// This checks callable shape, not semantic purity. Repeated calls (including
// copies of the extractor) must agree and must not mutate records or throw.
// Requiring const/noexcept syntax cannot prove those properties and would
// reject ordinary valid lambdas without an explicit noexcept annotation.
concept key_projection = sortable_record<Record> && std::is_copy_constructible_v<Key> &&
    requires(Key key, const Record& record) {
        { key(record) } -> unsigned_key;
    };

struct default_key {
    std::uint32_t operator()(std::uint32_t value) const { return value; }
    std::uint64_t operator()(std::uint64_t value) const { return value; }
    std::uint32_t operator()(const key_value32& row) const { return row.key; }
    std::uint64_t operator()(const key_value64& row) const { return row.key; }
};

template<class T>
concept default_record = unsigned_key<T> || std::same_as<T, key_value32> ||
                        std::same_as<T, key_value64>;

template<class R>
concept keyed_range = std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
    default_record<std::ranges::range_value_t<R>> &&
    sortable_record<std::remove_reference_t<std::ranges::range_reference_t<R>>>;
} // namespace radixkit::detail
