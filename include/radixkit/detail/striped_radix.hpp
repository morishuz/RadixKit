// SPDX-License-Identifier: MIT
// Copyright (c) 2026 fast32 contributors
#pragma once
#include "radix.hpp"
#include <limits>
#include <memory>

namespace radixkit::detail {
namespace striped_detail {
template<unsigned Shift, unsigned Mask>
inline void scatter(const std::uint32_t* source, std::uint32_t* destination,
                    std::size_t n, std::uint32_t* offsets) {
    std::size_t i=0;
    for (; n-i>=4; i+=4) {
        // Update offsets sequentially: equal keys must retain their input order.
        const auto a=source[i], b=source[i+1], c=source[i+2], d=source[i+3];
        const auto pa=offsets[(a>>Shift)&Mask]++;
        const auto pb=offsets[(b>>Shift)&Mask]++;
        const auto pc=offsets[(c>>Shift)&Mask]++;
        const auto pd=offsets[(d>>Shift)&Mask]++;
        destination[pa]=a; destination[pb]=b; destination[pc]=c; destination[pd]=d;
    }
    for (; i<n; ++i) {
        const auto v=source[i];
        destination[offsets[(v>>Shift)&Mask]++]=v;
    }
}
}

// Three stable LSD scatters, using independent histogram chains to expose
// instruction-level parallelism. 32-bit offsets halve histogram traffic.
template<unsigned Lanes=4>
inline void striped_radix_sort(std::span<std::uint32_t> values) {
    static_assert(Lanes==2 || Lanes==4);
    const auto n=values.size();
    if (n<128) { std::sort(values.begin(),values.end()); return; }
    if (n>std::numeric_limits<std::uint32_t>::max()) { radix_sort<11>(values); return; }
    constexpr std::size_t bins=2048+2048+1024;
    alignas(64) std::array<std::array<std::uint32_t,bins>,Lanes> counts{};
    std::array<std::uint32_t,Lanes> differences{};
    const auto first=values.front();
    std::size_t i=0;
    for (; n-i>=Lanes; i+=Lanes) {
        for (unsigned lane=0;lane<Lanes;++lane) {
            const auto v=values[i+lane];
            ++counts[lane][v&2047u];
            ++counts[lane][2048+((v>>11)&2047u)];
            ++counts[lane][4096+(v>>22)];
            differences[lane]|=v^first;
        }
    }
    for (; i<n; ++i) {
        const auto v=values[i];
        ++counts[0][v&2047u];
        ++counts[0][2048+((v>>11)&2047u)];
        ++counts[0][4096+(v>>22)];
        differences[0]|=v^first;
    }
    std::uint32_t varying=0;
    for (auto d:differences) varying|=d;
    if (!varying) return;
    for (unsigned lane=1;lane<Lanes;++lane)
        for (std::size_t b=0;b<bins;++b) counts[0][b]+=counts[lane][b];
    for (unsigned pass=0;pass<3;++pass) {
        const unsigned start=pass*2048, end=pass==2?5120:start+2048;
        std::uint32_t offset=0;
        for (unsigned b=start;b<end;++b) {
            const auto frequency=counts[0][b];
            counts[0][b]=offset;
            offset+=frequency;
        }
    }
    auto scratch=std::make_unique_for_overwrite<std::uint32_t[]>(n);
    auto* source=values.data();
    auto* destination=scratch.get();
    if (varying&2047u) {
        striped_detail::scatter<0,2047>(source,destination,n,counts[0].data());
        std::swap(source,destination);
    }
    if (varying&(2047u<<11)) {
        striped_detail::scatter<11,2047>(source,destination,n,counts[0].data()+2048);
        std::swap(source,destination);
    }
    if (varying>>22) {
        striped_detail::scatter<22,1023>(source,destination,n,counts[0].data()+4096);
        std::swap(source,destination);
    }
    if (source!=values.data()) std::copy_n(source,n,values.data());
}
}
