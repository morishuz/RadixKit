#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <radixkit/radixkit.hpp>
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>
#include <stdexcept>
#include <vector>

// Failure injection exists only in this executable, never in installed headers.
namespace injection {
bool armed=false;
std::size_t calls=0, fail_at=std::numeric_limits<std::size_t>::max();
void* allocate(std::size_t bytes) {
    if(armed && calls++==fail_at)throw std::bad_alloc{};
    if(void* p=std::malloc(bytes?bytes:1))return p;
    throw std::bad_alloc{};
}
}
void* operator new(std::size_t n){return injection::allocate(n);}
void* operator new[](std::size_t n){return injection::allocate(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

template<class T> std::size_t test() {
    std::size_t checks=0, failures=0;
    for(unsigned scenario=0;scenario<5;++scenario) {
        const std::size_t n=scenario==1?1048576:scenario==2?2097153:16384;
        std::mt19937_64 rng(7201+scenario);std::vector<T> input(n);
        for(std::size_t i=0;i<n;++i) {
            T x=static_cast<T>(rng());
            if(scenario==1)x=(x&65535)<<(sizeof(T)*8-16);
            if(scenario==3)x=T(i/16);
            if(scenario==4)x&=255;
            input[i]=x;
        }
        if(scenario==3){std::swap(input[511],input[512]);input[n-2]=0;}
        if(scenario==4)input[n-2]=std::numeric_limits<T>::max();
        auto expected=input;std::sort(expected.begin(),expected.end());
        auto v=input;
        injection::calls=0;injection::fail_at=std::numeric_limits<std::size_t>::max();injection::armed=true;
        try {radixkit::sort(std::span(v),radixkit::sort_policy::adaptive);}
        catch(...){injection::armed=false;throw;}
        injection::armed=false;
        const auto allocations=injection::calls;
        if(!allocations || v!=expected)throw std::runtime_error("allocation scenario did not exercise a correct allocating path");
        ++checks;
        for(std::size_t fail_at=0;fail_at<allocations;++fail_at) {
            v=input;bool caught=false;
            injection::calls=0;injection::fail_at=fail_at;injection::armed=true;
            try{radixkit::sort(std::span(v),radixkit::sort_policy::adaptive);}
            catch(const std::bad_alloc&){caught=true;}
            catch(...){injection::armed=false;throw;}
            injection::armed=false;
            if(!caught)throw std::runtime_error("injected failure did not propagate");
            auto after=v;std::sort(after.begin(),after.end());
            if(after!=expected)throw std::runtime_error("allocation failure lost keys");
            radixkit::sort(std::span(v),radixkit::sort_policy::adaptive);
            if(v!=expected)throw std::runtime_error("retry after allocation failure failed");
            ++checks;++failures;
        }
    }
    std::cout<<sizeof(T)*8<<"-bit: "<<checks<<" checks, "<<failures<<" injected failures\n";
    return checks;
}
template<class T> void selection_allocations() {
    std::mt19937_64 rng(81923);
    std::vector<T> input(4097);
    for(std::size_t i=0;i<input.size();++i) {
        if constexpr(std::is_integral_v<T>)input[i]=static_cast<T>(rng());
        else input[i]={static_cast<decltype(T{}.key)>(rng()),i};
    }
    auto key=[](const T& v) {
        if constexpr(std::is_integral_v<T>)return v;else return v.key;
    };
    auto canonical=[&](const T&a,const T&b) {
        if(key(a)!=key(b))return key(a)<key(b);
        if constexpr(std::is_integral_v<T>)return false;else return a.value<b.value;
    };
    auto expected=input;std::sort(expected.begin(),expected.end(),canonical);
    for (bool descending : {false, true}) for (std::size_t rank : {1, 4095}) {
        auto v = input;
        if (descending) std::sort(v.begin(), v.end(), [&](const T& a, const T& b) {
            return key(a) > key(b);
        });
        injection::calls = 0; injection::fail_at = 0; injection::armed = true;
        try { radixkit::nth_element(v, rank); }
        catch (...) { injection::armed = false; throw; }
        injection::armed = false;
        if (injection::calls || key(v[rank]) != key(expected[rank]))
            throw std::runtime_error("near-end selection allocation contract failed");
        std::sort(v.begin(), v.end(), canonical);
        if (v != expected) throw std::runtime_error("near-end selection lost records");
    }

    for(int op=0;op<3;++op) {
        auto v=input;bool caught=false;
        injection::calls=0;injection::fail_at=0;injection::armed=true;
        try {
            if(op==0)radixkit::nth_element(v,2048);
            if(op==1)radixkit::top_k(v,2048);
            if(op==2)radixkit::partial_sort(v,2048);
        }catch(const std::bad_alloc&){caught=true;}
        catch(...){injection::armed=false;throw;}
        injection::armed=false;
        if(caught!=(op==2) || (op<2&&injection::calls))
            throw std::runtime_error("selection allocation contract failed");
        auto after=v;std::sort(after.begin(),after.end(),canonical);
        if(after!=expected)throw std::runtime_error("selection failure lost records");
        if(op==2) {
            radixkit::partial_sort(v,2048);
            for(std::size_t i=0;i<2048;++i)if(key(v[i])!=key(expected[i]))
                throw std::runtime_error("partial sort retry failed");
        }
    }
}
int main(){try{
selection_allocations<std::uint32_t>();selection_allocations<std::uint64_t>();
selection_allocations<radixkit::key_value32>();selection_allocations<radixkit::key_value64>();
std::cout<<"28 selection allocation scenarios passed\n";
const auto count=test<std::uint32_t>()+test<std::uint64_t>();std::cout<<count<<" checks passed\n";}
catch(const std::exception&e){injection::armed=false;std::cerr<<e.what()<<'\n';return 1;}}
