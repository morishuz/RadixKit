#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <span>
#include <radixkit/radixkit.hpp>
#include <iostream>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
std::size_t checks=0;
using Pair=radixkit::key_value64;
bool less_pair(const Pair& a,const Pair& b) {
    return a.key<b.key || (a.key==b.key && a.value<b.value);
}
void check(const std::vector<std::uint64_t>& input) {
    auto expected=input;std::sort(expected.begin(),expected.end());
    for(unsigned method=0;method<5;++method) {
        auto v=input;auto key=[](std::uint64_t x){return x;};
        if(method==0)radixkit::sort(v);
        if(method==1)radixkit::detail::radix64_sort<8,4>(std::span(v),key);
        if(method==2)radixkit::detail::radix64_sort<11,2>(std::span(v),key);
        if(method==3)radixkit::detail::radix64_sort<11,4>(std::span(v),key);
        if(method==4)radixkit::detail::radix64_sort<16,1>(std::span(v),key);
        if(v!=expected)throw std::runtime_error("uint64 mismatch");
        ++checks;
        std::vector<Pair> records;records.reserve(input.size());
        for(std::size_t i=0;i<input.size();++i)records.push_back({input[i],std::uint64_t(i)^0xd1b54a32d192ed03ull});
        auto reference=records;std::sort(reference.begin(),reference.end(),less_pair);
        auto get=[](const Pair& r){return r.key;};
        if(method==0)radixkit::sort(records);
        if(method==1)radixkit::detail::radix64_sort<8,4>(std::span(records),get);
        if(method==2)radixkit::detail::radix64_sort<11,2>(std::span(records),get);
        if(method==3)radixkit::detail::radix64_sort<11,4>(std::span(records),get);
        if(method==4)radixkit::detail::radix64_sort<16,1>(std::span(records),get);
        if(!std::is_sorted(records.begin(),records.end(),[&](const Pair& a,const Pair& b){return get(a)<get(b);}))
            throw std::runtime_error("pair key order");
        // Canonicalize only after checking key order: ties need not be stable.
        std::sort(records.begin(),records.end(),less_pair);
        if(records!=reference)throw std::runtime_error("key/payload association");
        ++checks;
    }
}

// Exercise copies from source references with padding, keys after payloads,
// reference-returning extractors, every active 11-bit digit subset and tail.
template<class Record, class MakeRecord>
void check_record32(MakeRecord make_record) {
    std::mt19937_64 rng(314159);
    auto get = [](const Record& row) -> const std::uint32_t& { return row.key; };
    auto less = [&](const Record& a, const Record& b) { return get(a) < get(b); };
    for (unsigned active = 0; active < 8; ++active) {
        std::uint32_t mask = 0;
        for (unsigned digit = 0; digit < 3; ++digit)
            if (active & (1u << digit))
                mask |= std::uint32_t(digit == 2 ? 1023 : 2047) << (digit * 11);
        for (std::size_t tail = 0; tail < 4; ++tail) {
            const std::size_t n = 132 + tail;
            std::vector<Record> input;
            for (std::size_t i = 0; i < n; ++i)
                input.push_back(make_record(0x81234567u ^ (std::uint32_t(rng()) & mask), i, rng()));
            auto expected = input;
            std::sort(expected.begin(), expected.end(), less);
            for (bool partial : {false, true}) {
                auto values = input;
                const auto sorted = partial ? n - 4 : n;
                if constexpr (std::is_same_v<Record, radixkit::key_value32>) {
                    if (partial) radixkit::partial_sort(values, sorted);
                    else radixkit::sort(values);
                } else {
                    if (partial) radixkit::partial_sort_by_key(std::span(values), sorted, get);
                    else radixkit::sort_by_key(std::span(values), get);
                }
                for (std::size_t i = 0; i < sorted; ++i)
                    if (get(values[i]) != get(expected[i]))
                        throw std::runtime_error("projected uint32 key order");
                // Every payload field must survive; padding is not compared.
                std::sort(values.begin(), values.end(),
                          [](const Record& a, const Record& b) { return a.value < b.value; });
                if (values != input) throw std::runtime_error("projected uint32 payload preservation");
                ++checks;
            }
        }
    }
}
}

int main() {
    try {
        std::mt19937_64 rng(271828);
        for(std::size_t n:{0,1,2,3,4,7,8,127,128,129,130,131,255,1024,4097,65536}) {
            for(unsigned pattern=0;pattern<8;++pattern) {
                std::vector<std::uint64_t> v(n);
                for(auto& x:v) {
                    x=rng();
                    if(pattern==1)x%=16;
                    if(pattern==2)x=UINT64_MAX;
                    if(pattern==3)x&=UINT32_MAX;
                    if(pattern==4)x&=0xffffffff00000000ull;
                    if(pattern==5 && rng()%100)x=0;
                }
                if(pattern==6)std::sort(v.begin(),v.end());
                if(pattern==7)std::sort(v.begin(),v.end(),std::greater<>{});
                check(v);
            }
        }
        // All six-digit activity subsets, odd/even pass parity and scalar tails.
        for(unsigned active=1;active<64;++active) {
            std::uint64_t mask=0;
            for(unsigned p=0;p<6;++p)if(active&(1u<<p))
                mask|=std::uint64_t(p==5?511:2047)<<(p*11);
            std::vector<std::uint64_t> v(133);
            for(auto& x:v)x=0x8123456789abcdefull^(rng()&mask);
            check(v);
        }
        check({0,UINT64_MAX,1ull<<63,(1ull<<63)-1,UINT32_MAX,std::uint64_t(UINT32_MAX)+1,0,UINT64_MAX});
        struct alignas(32) Record {std::uint64_t value,key,extra,tag;bool operator==(const Record&) const=default;};
        std::vector<Record> records(4097);
        for(std::size_t i=0;i<records.size();++i)records[i]={rng()%7,rng()%11,rng(),i};
        auto expected=records;
        radixkit::sort_by_key(std::span(records),[](const Record& r){return r.key;});
        if(!std::is_sorted(records.begin(),records.end(),[](auto& a,auto& b){return a.key<b.key;}))return 1;
        std::sort(records.begin(),records.end(),[](auto& a,auto& b){return a.tag<b.tag;});
        if(records!=expected)return 1;
        ++checks;

        check_record32<radixkit::key_value32>([](std::uint32_t key, std::uint64_t id, std::uint64_t) {
            return radixkit::key_value32{key, id};
        });
        struct Record16 {
            std::uint64_t value;
            std::uint32_t key, extra;
            bool operator==(const Record16&) const = default;
        };
        struct alignas(32) Record32 {
            std::uint64_t value;
            std::uint32_t key, extra;
            std::uint64_t left, right;
            bool operator==(const Record32&) const = default;
        };
        check_record32<Record16>([](std::uint32_t key, std::uint64_t id, std::uint64_t payload) {
            return Record16{id, key, std::uint32_t(payload)};
        });
        check_record32<Record32>([](std::uint32_t key, std::uint64_t id, std::uint64_t payload) {
            return Record32{id, key, std::uint32_t(payload), payload, ~payload};
        });
        std::cout<<"Passed "<<checks<<" uint64 and record differential checks\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
