#include <cstddef>
#include <cstdint>
#include <span>
#include <radixkit/sort.hpp>
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

std::vector<std::uint64_t> make_keys(std::size_t n,unsigned bits,bool one_bucket) {
    std::mt19937_64 rng(8751+bits+n);
    const std::uint64_t low_mask=bits?(UINT64_C(1)<<bits)-1:0;
    std::vector<std::uint64_t> keys;keys.reserve(n);
    const auto add=[&](unsigned bucket,std::size_t count) {
        while(count-- && keys.size()<n)keys.push_back((std::uint64_t(bucket)<<54)|(rng()&low_mask));
    };
    if(!one_bucket){add(1,1);add(2,127);add(3,128);add(4,129);add(5,257);}
    add(1023,n-keys.size());std::shuffle(keys.begin(),keys.end(),rng);
    return keys;
}
void check_keys(const std::vector<std::uint64_t>& input,bool direct) {
    auto expected=input;std::sort(expected.begin(),expected.end());auto v=input;
    if(direct)radixkit::detail::wide::partitioned_radix64_sort<10,9,1>(std::span(v),radixkit::detail::wide::identity_key64{});
    else radixkit::sort(std::span(v),radixkit::sort_policy::adaptive);
    if(v!=expected)throw std::runtime_error("key partition boundary mismatch");
}
void check_records(const std::vector<std::uint64_t>& input) {
    std::vector<radixkit::key_value64> records(input.size());
    for(std::size_t i=0;i<input.size();++i)records[i]={input[i],i};
    radixkit::detail::wide::partitioned_radix64_sort<10,9,1>(std::span(records),[](const auto& r){return r.key;});
    auto expected=input;std::sort(expected.begin(),expected.end());
    std::vector<bool> seen(input.size());
    for(std::size_t i=0;i<records.size();++i) {
        const auto& r=records[i];
        if(r.key!=expected[i] || r.value>=input.size() || seen[r.value] || input[r.value]!=r.key)
            throw std::runtime_error("record partition boundary/payload mismatch");
        seen[r.value]=true;
    }
}
int main(){try {
    std::size_t checks=0;
    for(std::size_t n:{32767u,32768u,32769u})for(unsigned bits:{0u,9u,18u,27u,54u}) {
        auto keys=make_keys(n,bits,false);
        check_keys(keys,true);check_keys(keys,false);check_records(keys);checks+=3;
    }
    auto same_high=make_keys(32769,54,true);check_keys(same_high,true);check_records(same_high);checks+=2;
    // Force broad spaced samples in both public dispatchers and the kernel,
    // while leaving almost all data in one high bucket. Test the actual public
    // threshold as well as the directly exercised internal bucket boundaries.
    for(std::size_t n:{2097151u,2097152u,2097153u}) {
        auto keys=make_keys(n,54,false);
        for(std::size_t i=0;i<32;++i)keys[i*((n-1)/31)+i*((n-1)%31)/31]=std::uint64_t((i*61)%1024)<<54;
        for(std::size_t i=0;i<16;++i)keys[(n-1)/15*i]=std::uint64_t((i*61)%1024)<<54;
        check_keys(keys,false);++checks;
    }
    std::cout<<checks<<" partition boundary checks passed (including complete payload identity)\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
