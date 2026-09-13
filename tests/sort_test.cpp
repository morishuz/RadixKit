#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>
#include <radixkit/detail/radix.hpp>
#include <radixkit/sort.hpp>
#include <iostream>
#include <random>

namespace {
using Function = void (*)(std::span<std::uint32_t>);
void adaptive(std::span<std::uint32_t> v) { radixkit::sort(v, radixkit::sort_policy::adaptive); }
void radix(std::span<std::uint32_t> v) { radixkit::sort(v, radixkit::sort_policy::radix); }
// Exercise the size_t-counter fallback at affordable sizes; this does not
// simulate or validate allocations above UINT32_MAX elements.
void fallback8(std::span<std::uint32_t> v) { radixkit::detail::radix_sort<8>(v); }
void fallback11(std::span<std::uint32_t> v) { radixkit::detail::radix_sort<11>(v); }
struct Method { const char* name; Function function; };
constexpr Method methods[] = {{"sort", static_cast<Function>(radixkit::sort)},
                              {"adaptive", adaptive}, {"radix policy", radix},
                              {"size_t radix8", fallback8}, {"size_t radix11", fallback11}};
}

int main() {
    std::mt19937 rng(157);
    std::size_t checks = 0;
    for (std::size_t n : {0,1,2,3,4,5,31,127,128,129,130,131,255,1023,1024,1025,1026,1027,4095,4096,4097,65536}) {
        for (int trial = 0; trial < 30; ++trial) {
            std::vector<std::uint32_t> input(n);
            for (auto& v : input) {
                v = rng();
                switch (trial % 6) {
                case 0: v %= 16; break;
                case 1: v = UINT32_MAX; break;
                case 2: v = (v & 255u) << 24; break;
                case 3: v = (v & 255u) << 8; break;
                default: break;
                }
            }
            if (trial == 4) std::sort(input.begin(), input.end());
            if (trial == 5) std::sort(input.begin(), input.end(), std::greater<>{});
            auto expected = input;
            std::sort(expected.begin(), expected.end());
            for (auto method : methods) {
                auto actual = input;
                method.function(actual);
                if (actual != expected) {
                    std::cerr << method.name << " failed n=" << n << " trial=" << trial << '\n';
                    return 1;
                }
                ++checks;
            }
        }
    }
    // Sampling must never decide correctness. Exercise hidden outliers, a
    // misleading dominant sample, exceptions on both sides, and range cutoffs.
    for (int scenario=0;scenario<7;++scenario) {
        const std::size_t n=scenario<4?4097:262143+(scenario-4);
        std::vector<std::uint32_t> input(n);
        for(auto& v:input)v=scenario<4?100+(rng()%16):0xffff0000u+(rng()%65536);
        if(scenario==0)input.back()=UINT32_MAX;
        if(scenario==1)input.back()=0;
        if(scenario==2) {
            for(auto& v:input)v=rng();
            for(std::size_t i=0;i<32;++i)input[i*((n-1)/31)]=123;
        }
        if(scenario==3) {
            std::fill(input.begin(),input.end(),0x80000000u);
            input[1]=0; input.back()=UINT32_MAX;
        }
        if(scenario>=4) {input[0]=0xffff0000u;input.back()=UINT32_MAX;}
        auto expected=input;
        std::sort(expected.begin(),expected.end());
        for(auto method:methods) {
            auto actual=input;method.function(actual);
            if(actual!=expected) {std::cerr<<method.name<<" failed adversarial case "<<scenario<<'\n';return 1;}
            ++checks;
        }
    }
    // Large-kernel cutoff, active-digit parity, majority dispatch, and run tails.
    for (std::size_t n : {65535,65536,65537,65543}) {
        for (unsigned active=1;active<8;++active) {
            const std::uint32_t mask=((active&1)?2047u:0u)
                | ((active&2)?(2047u<<11):0u) | ((active&4)?(1023u<<22):0u);
            for (unsigned pattern=0;pattern<6;++pattern) {
                std::vector<std::uint32_t> input(n);
                for (std::size_t i=0;i<n;++i) {
                    auto bits=rng()&mask;
                    if (pattern==1 && i%8!=7) bits=0; // No first-pass run of eight.
                    if (pattern==2 && i<n/2) bits=0; // Majority threshold boundary.
                    if (pattern==3 && i<=n/2) bits=0;
                    if (pattern==4 && i+1<n) bits=0; // One wide exception.
                    if (pattern==5) bits=0; // All equal after the large-kernel cutoff.
                    input[i]=0x81234567u^bits;
                }
                auto expected=input;std::sort(expected.begin(),expected.end());
                for (auto method:methods) {
                    auto actual=input;method.function(actual);
                    if (actual!=expected) {
                        std::cerr<<method.name<<" failed large-kernel boundary n="<<n
                                 <<" active="<<active<<" pattern="<<pattern<<'\n';
                        return 1;
                    }
                    ++checks;
                }
            }
        }
    }
    std::cout << "Passed " << checks << " full-sort differential checks\n";
}
