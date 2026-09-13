// SPDX-License-Identifier: MIT
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <radixkit/radixkit.hpp>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>

template<class T> auto key(const T& x) {
    if constexpr(std::is_integral_v<T>)return x;else return x.key;
}
template<class T> void run(const char* name,std::size_t n) {
    std::mt19937_64 rng(13579);
    for(auto distribution:{"uniform","duplicates"}) {
        std::vector<T> input(n);
        for(std::size_t i=0;i<n;++i) {
            auto k=rng();if(distribution[0]=='d')k%=16;
            if constexpr(std::is_integral_v<T>)input[i]=static_cast<T>(k);
            else input[i]={static_cast<decltype(T{}.key)>(k),i};
        }
        auto less=[](const T&a,const T&b){return key(a)<key(b);};
        auto expected=input;std::sort(expected.begin(),expected.end(),less);
        std::vector<std::size_t> counts{1,n/100,n/2,n};
        counts.erase(std::unique(counts.begin(),counts.end()),counts.end());
        for(auto k:counts)for(int op=0;op<3;++op)
        for(int repeat=-1;repeat<5;++repeat)for(int slot=0;slot<2;++slot) {
            bool ours=(slot+(repeat+1))%2==0;
            auto v=input;
            auto start=std::chrono::steady_clock::now();
            if(op==0) {
                if(ours)radixkit::nth_element(v,k);
                else if(k<n)std::nth_element(v.begin(),v.begin()+k,v.end(),less);
            }else if(op==1) {
                if(ours)radixkit::partial_sort(v,k);
                else std::partial_sort(v.begin(),v.begin()+k,v.end(),less);
            }else {
                if(ours)radixkit::top_k(v,k);
                else if(k<n)std::nth_element(v.begin(),v.begin()+k,v.end(),[&](const T&a,const T&b){return less(b,a);});
            }
            auto stop=std::chrono::steady_clock::now();
            if(op==0&&k<n) {
                if(key(v[k])!=key(expected[k]))std::abort();
                for(std::size_t i=0;i<k;++i)if(key(v[i])>key(v[k]))std::abort();
                for(std::size_t i=k+1;i<n;++i)if(key(v[i])<key(v[k]))std::abort();
            }
            if(op==1)for(std::size_t i=0;i<k;++i)if(key(v[i])!=key(expected[i]))std::abort();
            if(op==2) {
                auto prefix=std::vector<T>(v.begin(),v.begin()+k);std::sort(prefix.begin(),prefix.end(),less);
                for(std::size_t i=0;i<k;++i)if(key(prefix[i])!=key(expected[n-k+i]))std::abort();
            }
            if constexpr(std::is_integral_v<T>){auto copy=v;std::sort(copy.begin(),copy.end());if(copy!=expected)std::abort();}
            else {
                std::vector<bool> seen(n);
                for(const auto&r:v){if(r.value>=n||seen[r.value]||r.key!=input[r.value].key)std::abort();seen[r.value]=true;}
            }
            if(repeat>=0)std::cout<<name<<','<<n<<','<<distribution<<','<<k<<','<<op<<','<<repeat<<','<<(ours?"radixkit":"std")<<','<<std::chrono::duration<double,std::milli>(stop-start).count()<<'\n';
        }
    }
}
int main(int argc,char**argv){std::size_t n=argc>1?std::stoull(argv[1]):100000;if(n<100)return 1;
std::cout<<"type,n,distribution,k,operation,repeat,method,ms\n";
run<std::uint32_t>("u32",n);run<std::uint64_t>("u64",n);
run<radixkit::key_value32>("kv32",n);run<radixkit::key_value64>("kv64",n);}
