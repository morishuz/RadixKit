#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <radixkit/radixkit.hpp>
#include <iostream>
#include <random>
#include <stdexcept>
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
        std::cout<<"Passed "<<checks<<" uint64 and record differential checks\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
