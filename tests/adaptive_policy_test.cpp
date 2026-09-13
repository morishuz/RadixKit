#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <radixkit/sort.hpp>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <random>
#include <string>

const char* distributions[]={"uniform","uniform16","zeros99","equal","shifted16","sparse8","ascending","descending","sample_trap","zipf_like","sparse16","nearly_sorted","ordered_sample_trap","majority_sample_trap","dictionary256","irregular8","late_outlier","nonzero99","maximum99","zeros90"};
const char* methods[]={"default","adaptive","radix"};
template<class T> std::vector<T> make_input(std::size_t n,unsigned d,std::uint64_t seed) {
    std::mt19937_64 rng(seed); std::vector<T> v(n);
    std::array<unsigned,sizeof(T)*8> positions{};
    std::iota(positions.begin(),positions.end(),0);std::shuffle(positions.begin(),positions.end(),rng);
    for(std::size_t i=0;i<n;++i) {
        T x=static_cast<T>(rng());
        switch(d) {
        case 1:x&=65535;break;
        case 2:if(rng()%100) x=0;break;
        case 3:x=~T(0);break;
        case 4:x=T(x&65535)<<(sizeof(T)*8-16);break;
        case 5:{ T y=0; for(unsigned j=0;j<8;++j) y|=T((x>>j)&1)<<(j*sizeof(T)); x=y;break; }
        case 6:x=static_cast<T>(i);break;
        case 7:x=static_cast<T>(n-i);break;
        case 8:x&=255;break;
        case 9:x=static_cast<T>(65535.0*std::pow(std::generate_canonical<double,53>(rng),8));break;
        case 10:{T y=0;for(unsigned j=0;j<16;++j)y|=T((x>>j)&1)<<(j*(sizeof(T)/2));x=y;break;}
        }
        if(d==11 || d==12) x=static_cast<T>(i);
        if(d==14) { const auto k=x&255; x=static_cast<T>(k*UINT64_C(0x9e3779b97f4a7c15)); }
        if(d==15) { T y=0; for(unsigned j=0;j<8;++j)y|=T((x>>j)&1)<<positions[j]; x=y; }
        if(d==16)x&=255;
        if(d==17 && rng()%100)x=std::numeric_limits<T>::max()/3;
        if(d==18 && rng()%100)x=std::numeric_limits<T>::max();
        if(d==19 && rng()%10)x=0;
        v[i]=x;
    }
    if(d==8 && n>64) v[17]=~T(0); // outlier omitted by the strategy sample at tested large sizes
    if(d==11 && n>64) std::swap(v[n/2],v[n/2+1]);
    if(d==12 && n>64) { v[17]=~T(0); v[n/2]=0; }
    if(d==13 && n>64) for(std::size_t i=0;i<32;++i) v[i*((n-1)/31)+i*((n-1)%31)/31]=0;
    if(d==16 && n>64)v[n-2]=std::numeric_limits<T>::max();
    return v;
}
template<class T> void run(std::vector<T>& v,unsigned m) {
    if(m==0)radixkit::sort(std::span(v));
    else radixkit::sort(std::span(v), m==1 ? radixkit::sort_policy::adaptive : radixkit::sort_policy::radix);
}
template<class T> std::size_t tests() {
    std::size_t checks=0;
    for(unsigned d=0;d<std::size(distributions);++d) for(std::size_t n:{0u,1u,2u,31u,256u,511u,512u,513u,1023u,1024u,1025u,4095u,4096u,4097u,65535u,65536u,65537u}) for(unsigned seed=0;seed<3;++seed) {
        const auto input=make_input<T>(n,d,seed);auto expected=input;std::sort(expected.begin(),expected.end());
        for(unsigned m=0;m<std::size(methods);++m){auto v=input;run(v,m);if(v!=expected)throw std::runtime_error(std::string("failed: ")+methods[m]+" / "+distributions[d]);++checks;}
    }
    // Check reconstruction when the non-varying bits are ones, not just zeros.
    for(unsigned d:{5u,10u}) for(unsigned seed=0;seed<4;++seed) {
        auto input=make_input<T>(65537,d,seed); T varying=0;
        for(auto x:input) varying |= x^input[0];
        for(auto&x:input) x |= ~varying;
        auto expected=input;std::sort(expected.begin(),expected.end());
        for(unsigned m=0;m<std::size(methods);++m) {
            auto v=input;run(v,m);
            if(v!=expected)throw std::runtime_error("fixed-bit reconstruction failed");
            ++checks;
        }
    }
    for(unsigned bits:{1u,7u,8u,9u,15u,16u,17u}) {
        std::mt19937_64 rng(9871+bits); std::array<unsigned,sizeof(T)*8> pos{};
        std::iota(pos.begin(),pos.end(),0);std::shuffle(pos.begin(),pos.end(),rng);
        T mask=0;for(unsigned b=0;b<bits;++b)mask|=T(1)<<pos[b];
        const auto n=std::max(std::size_t(1024),std::size_t(16)<<bits);
        std::vector<T> input(n);
        for(auto&x:input)x=T(rng())&mask;
        for(auto&x:input)x|=~mask; // fixed-one bits must survive reconstruction
        auto expected=input;std::sort(expected.begin(),expected.end());
        auto v=input;run(v,1);if(v!=expected)throw std::runtime_error("irregular domain failure");++checks;
        if(bits<=16) {
            // Counter acceptance must agree with an independent sorted oracle.
            v=input;
            if(!radixkit::detail::adaptive_keys::try_histogram(std::span(v),input[0],mask) || v!=expected)
                throw std::runtime_error("adaptive histogram acceptance failure");
            ++checks;
            for(std::size_t index:{std::size_t(0),std::size_t(255),std::size_t(256),std::size_t(257),n-2,n-1}) {
                v=input;v[index]^=T(1)<<pos[bits];const auto before=v;
                if(radixkit::detail::adaptive_keys::try_histogram(std::span(v),input[1],mask) || v!=before)
                    throw std::runtime_error("adaptive histogram rejection mutated input");
                ++checks;
            }
        }
    }
    for(bool late_outlier:{false,true}) {
        std::vector<T> input(16384);
        for(std::size_t i=0;i<input.size();++i)input[i]=static_cast<T>(i/16);
        std::swap(input[511],input[512]);input[10000]=0;
        if(late_outlier)input[input.size()-2]=std::numeric_limits<T>::max();
        auto repaired=input;
        if(radixkit::detail::wide::bounded_order_repair(std::span(repaired),[](T x){return x;}) || repaired==input)
            throw std::runtime_error("repair fallback test did not exercise mutation");
        auto expected=input;std::sort(expected.begin(),expected.end());run(input,1);
        if(input!=expected)throw std::runtime_error("partially repaired input lost values");
        ++checks;
    }
    for(unsigned bits:{2u,16u}) {
        T mask=T(1)<<(sizeof(T)*8-1);
        for(unsigned b=0;b<bits-1;++b)mask|=T(1)<<(b*2);
        const auto n=std::size_t(16)<<bits;
        std::mt19937_64 rng(1298);std::vector<T> v(n);
        for(auto&x:v)x=(T(rng())&mask)|~mask;
        auto expected=v;std::sort(expected.begin(),expected.end());
        if(!radixkit::detail::adaptive_keys::try_histogram(std::span(v),v[0],mask) || v!=expected)
            throw std::runtime_error("MSB histogram enumeration failed");
        ++checks;
    }
    for(unsigned bits : {8u,9u,15u,16u}) {
        const std::size_t domain=std::size_t(1)<<bits;
        const std::size_t density=bits<=8?2:(sizeof(T)==4?16:8);
        const T mask=(T(domain)-1)<<(sizeof(T)*8-bits);
        for(int offset : {-1,0,1}) {
            const auto n=domain*density+offset;
            std::mt19937_64 rng(367+bits);std::vector<T> v(n);
            for(auto&x:v)x=(T(rng())&mask)|~mask;
            auto expected=v;std::sort(expected.begin(),expected.end());run(v,1);
            if(v!=expected)throw std::runtime_error("domain cutoff failure");
            ++checks;
        }
    }
    // Direct small fused path: invalid indices must be bounded before rejection.
    for(std::size_t n : {512u,513u,4095u,4096u,4097u}) {
        for(std::size_t index : {std::size_t(0),std::size_t(255),n/2,n-1}) {
            std::vector<T> v(n);
            for(std::size_t i=0;i<n;++i)v[i]=T((i*71)%256);
            v[index]=std::numeric_limits<T>::max();const auto before=v;
            if(radixkit::detail::adaptive_keys::try_histogram(std::span(v),T(0),T(255)) || v!=before)
                throw std::runtime_error("small/large rejection mutated input");
            run(v,1);auto expected=before;std::sort(expected.begin(),expected.end());
            if(v!=expected)throw std::runtime_error("outlier fallback failure");
            ++checks;
        }
    }
    // A bounded suffix repair must never report success when an insertion
    // belongs before the already-verified prefix (both order directions).
    for(std::size_t index : {63u,64u,65u,127u,128u,511u,8191u}) {
        for(std::size_t distance : {1u,62u,63u,64u,65u}) {
            if(distance>index)continue;
            for(bool descending : {false,true}) {
                std::vector<T> v(16384);
                for(std::size_t i=0;i<v.size();++i)v[i]=T(2*i+2);
                v[index]=v[index-distance]-1;
                if(descending)for(auto&x:v)x=std::numeric_limits<T>::max()-x;
                auto expected=v;
                if(descending)std::sort(expected.begin(),expected.end(),std::greater<>{});
                else std::sort(expected.begin(),expected.end());
                const bool accepted=descending?
                    radixkit::detail::adaptive_keys::check_and_repair<true>(std::span(v)):
                    radixkit::detail::adaptive_keys::check_and_repair<false>(std::span(v));
                if(accepted && v!=expected)throw std::runtime_error("repair escaped checked boundary");
                if(accepted!=(distance<64))throw std::runtime_error("repair budget not respected");
                run(v,1);std::sort(expected.begin(),expected.end());
                if(v!=expected)throw std::runtime_error("repair rejection lost values");
                ++checks;
            }
        }
    }
    return checks;
}
int main() {
    try {
        std::cout << tests<std::uint32_t>() + tests<std::uint64_t>() << " adaptive policy checks passed\n";
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n';return 1;
    }
}
