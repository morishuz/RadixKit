#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <radixkit/radixkit.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

std::size_t cases = 0;
void require(bool ok) { if (!ok) throw std::runtime_error("selection contract failed"); }
template<class T> auto key(const T& v) {
    if constexpr (std::is_integral_v<T>) return v;
    else return v.key;
}
template<class T> void check(const std::vector<T>& input, std::size_t k) {
    ++cases;
    auto expected=input;
    auto less=[](const T& a,const T& b){ return key(a)<key(b); };
    std::sort(expected.begin(),expected.end(),less);
    for (int operation=0;operation<4;++operation) {
        auto actual=input;
        if(operation==0)radixkit::nth_element(actual,k);
        if(operation==1)radixkit::partial_sort(actual,k);
        if(operation==2)radixkit::top_k(actual,k);
        if(operation==3)radixkit::sort(actual);
        if(operation==0 && k<input.size()) {
            require(key(actual[k])==key(expected[k]));
            for(std::size_t i=0;i<k;++i)require(key(actual[i])<=key(actual[k]));
            for(std::size_t i=k+1;i<input.size();++i)require(key(actual[i])>=key(actual[k]));
        }
        if(operation==1 || operation==3) {
            for(std::size_t i=0;i<(operation==3?actual.size():k);++i)
                require(key(actual[i])==key(expected[i]));
        }
        if(operation==2) {
            auto prefix=std::vector<T>(actual.begin(),actual.begin()+k);
            std::sort(prefix.begin(),prefix.end(),less);
            for(std::size_t i=0;i<k;++i)require(key(prefix[i])==key(expected[input.size()-k+i]));
        }
        if((operation==0&&k==input.size()) || ((operation==1||operation==2)&&k==0) ||
           (operation==2&&k==input.size())) require(actual==input);
        if constexpr (std::is_integral_v<T>) {
            std::sort(actual.begin(),actual.end());require(actual==expected);
        } else {
            // Unique payload IDs catch losses, duplication and broken associations.
            std::sort(actual.begin(),actual.end(),[](const T&a,const T&b){return a.value<b.value;});
            require(actual==input);
        }
    }
    for(int op=0;op<3;++op) {
        auto copy=input;bool threw=false;
        try {
            if(op==0)radixkit::nth_element(copy,copy.size()+1);
            if(op==1)radixkit::partial_sort(copy,copy.size()+1);
            if(op==2)radixkit::top_k(copy,copy.size()+1);
        }catch(const std::out_of_range&){threw=true;}
        require(threw&&copy==input);
    }
}
template<class T> void suite() {
    using K=decltype(key(T{}));
    std::mt19937_64 rng(987654321);
    // Every top-byte bucket, then long empty-bucket walks ending at byte 255.
    // Low bytes vary too, so narrowing must preserve the residual rank.
    for (bool sparse : {false, true}) {
        std::vector<T> input(513);
        for (std::size_t i = 0; i < input.size(); ++i) {
            const K high = sparse ? (i < 3 ? 0 : 255) : K(i % 256);
            const K value = (high << (sizeof(K) * 8 - 8)) | K((i * 71) & 255);
            if constexpr (std::is_integral_v<T>) input[i] = value;
            else input[i] = {value, i};
        }
        for (std::size_t rank : {1, 2, 3, 255, 256, 510, 511}) check(input, rank);
    }
    for(unsigned iteration=0;iteration<140;++iteration) {
        std::size_t sizes[]={0,1,2,3,7,127,128,129,255,256,257,4095,4096,4097,32768,65537};
        std::size_t n=iteration<16?sizes[iteration]:rng()%1200;
        std::vector<T> input(n);
        for(std::size_t i=0;i<n;++i) {
            K v=static_cast<K>(rng());
            switch(iteration%8) {
                case 0:v=0;break;
                case 1:v%=5;break;
                case 2:if(i%100)v=0;break;
                case 3:v=K(i);break;
                case 4:v=K(n-i);break;
                case 5:v&=(K(1)<<(sizeof(K)*8-1))|K(1);break;
                case 6:v=~K(v%4);break;
            }
            if constexpr(std::is_integral_v<T>)input[i]=v;
            else input[i]={v,i};
        }
        for(auto k:{std::size_t(0),n?std::size_t(1):0,n/2,n?n-1:0,n})check(input,k);
    }
    // Heap eligibility boundaries, both ends, and replacement-budget fallback.
    // Descending keys force repeated replacements; the payload IDs must survive
    // the partially completed heap followed by radix selection.
    for (std::size_t n : {2047, 2048, 2049, 16383, 16384, 65535, 65536}) {
        for (unsigned pattern = 0; pattern < 3; ++pattern) {
            std::vector<T> input(n);
            for (std::size_t i = 0; i < n; ++i) {
                K v = pattern == 0 ? K(n - i) :
                      pattern == 1 ? static_cast<K>(rng()) : K(i % 4);
                if constexpr (std::is_integral_v<T>) input[i] = v;
                else input[i] = {v, i};
            }
            for (std::size_t distance : {1, 2, 15, 16, 31, 32, 63, 64}) {
                check(input, distance);
                check(input, n - 1 - distance);
            }
        }
    }
    // Exhaustive three-symbol arrays, all ranks and counts.
    for(std::size_t n=0,power=1;n<=6;++n,power*=3)for(std::size_t code=0;code<power;++code) {
        std::vector<T> v(n);auto x=code;
        for(std::size_t i=0;i<n;++i,x/=3) {
            K k=x%3==2?~K(0):K(x%3);
            if constexpr(std::is_integral_v<T>)v[i]=k;else v[i]={k,i};
        }
        for(std::size_t k=0;k<=n;++k)check(v,k);
    }
}
int main(){try{
    suite<std::uint32_t>();suite<std::uint64_t>();
    suite<radixkit::key_value32>();suite<radixkit::key_value64>();
    struct Custom {std::uint32_t k;std::array<std::uint64_t,3> payload;};
    std::array<Custom,3> rows{{{7,{1,2,3}},{1,{4,5,6}},{3,{7,8,9}}}};
    auto extract=[](const Custom&r){return r.k;};
    radixkit::nth_element_by_key(std::span(rows),1,extract);
    require(rows[1].k==3&&rows[1].payload[0]==7);
    radixkit::top_k_by_key(std::span(rows),1,extract);
    require(rows[0].k==7&&rows[0].payload[2]==3);
    radixkit::partial_sort_by_key(std::span(rows),2,extract);
    require(rows[0].k==1&&rows[1].k==3&&rows[0].payload[1]==5);
    radixkit::sort_by_key(std::span(rows),extract);
    std::vector<Custom> many(4097);
    for (std::size_t i=0;i<many.size();++i) many[i]={std::uint32_t((i*71)%257),{i,i^123,i*3}};
    auto original=many;
    for(int op=0;op<4;++op) {
        many=original;
        if(op==0)radixkit::sort_by_key(std::span(many),extract);
        if(op==1)radixkit::nth_element_by_key(std::span(many),128,extract);
        if(op==2)radixkit::partial_sort_by_key(std::span(many),128,extract);
        if(op==3)radixkit::top_k_by_key(std::span(many),128,extract);
        auto expected=original;std::sort(expected.begin(),expected.end(),[](const Custom&a,const Custom&b){return a.k<b.k;});
        if(op==0||op==2)for(std::size_t i=0;i<(op==0?many.size():128);++i)require(many[i].k==expected[i].k);
        if(op==1) {
            require(many[128].k==expected[128].k);
            for(std::size_t i=0;i<128;++i)require(many[i].k<=many[128].k);
            for(std::size_t i=129;i<many.size();++i)require(many[i].k>=many[128].k);
        }
        if(op==3) {
            std::sort(many.begin(),many.begin()+128,[](const Custom&a,const Custom&b){return a.k<b.k;});
            for(std::size_t i=0;i<128;++i)require(many[i].k==expected[many.size()-128+i].k);
        }
        std::vector<bool> seen(many.size());
        for(const auto&r:many){auto id=r.payload[0];require(id<many.size()&&!seen[id]);seen[id]=true;require(r.k==original[id].k&&r.payload==original[id].payload);}
    }
    std::uint64_t raw[]{9,1,7};radixkit::top_k(raw,1);require(raw[0]==9);
    std::cout << cases << " input/rank cases across all four operations, plus invalid-boundary and custom-record checks passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
