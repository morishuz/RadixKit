#include <cstddef>
#include <functional>
#include <radixkit/sort.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Rng {
    std::uint64_t state;
    std::uint64_t operator()() {
        auto z=(state+=UINT64_C(0x9e3779b97f4a7c15));
        z=(z^(z>>30))*UINT64_C(0xbf58476d1ce4e5b9);
        z=(z^(z>>27))*UINT64_C(0x94d049bb133111eb);
        return z^(z>>31);
    }
    std::size_t below(std::size_t n) { return n ? (*this)()%n : 0; }
};
constexpr const char* families[]={"uniform","arbitrary_mask","hidden_bit","constant",
    "majority","lying_majority_sample","lying_order_sample","ordered_repairs",
    "late_disorder","dictionary_boundary","narrow_range_wrap","digit_runs",
    "alternating_extremes","organ_pipe","rotated_sorted","sawtooth",
    "mask_density_boundary","partial_repair_then_outlier","partition_boundary",
    "dictionary_hash_collisions","aligned_range_edge","mutation_mix"};
struct Config {
    std::uint64_t seed=2026091101, cases=10000;
    std::size_t max_size=4194305;
    double seconds=0;
    std::optional<std::uint64_t> replay;
    std::string checkpoint, failure="stress-failure";
    bool exhaustive=false;
};
struct Totals { std::uint64_t cases=0, checks=0, elements=0, u32=0, u64=0;
    std::array<std::uint64_t,std::size(families)> families{}; };
Config config;
Totals totals;
std::uint64_t current_index=0;
std::string current_stage;

template<class T> [[noreturn]] void fail(const std::vector<T>& input,const char* reason) {
    const auto file=config.failure+"-u"+std::to_string(sizeof(T)*8)+"-"+std::to_string(current_index)+".txt";
    std::ofstream out(file);
    out<<"seed "<<config.seed<<" case "<<current_index<<" bits "<<sizeof(T)*8
       <<" stage "<<current_stage<<" size "<<input.size()<<" reason "<<reason<<'\n';
    for(auto x:input)out<<x<<'\n';
    throw std::runtime_error(std::string(reason)+"; input: "+file);
}
template<class T> void check(const std::vector<T>& input, unsigned method=1) {
    auto expected=input;std::sort(expected.begin(),expected.end());
    // Alternate exact allocations (ASan redzones immediately after input)
    // with offset subspans surrounded by canaries (detect boundary writes).
    const std::size_t padding=current_index%3==0?0:17;
    const T sentinel=static_cast<T>(UINT64_C(0xa5c34fe1926db807));
    std::vector<T> storage(input.size()+2*padding,sentinel);
    std::copy(input.begin(),input.end(),storage.begin()+padding);
    auto view=std::span<T>(storage).subspan(padding,input.size());
    if(method==0)radixkit::sort(view);
    else radixkit::sort(view,method==1?radixkit::sort_policy::adaptive:radixkit::sort_policy::radix);
    if(!std::equal(view.begin(),view.end(),expected.begin()))fail(input,"output differs from std::sort");
    if(!std::all_of(storage.begin(),storage.begin()+padding,[=](T x){return x==sentinel;}) ||
       !std::all_of(storage.end()-padding,storage.end(),[=](T x){return x==sentinel;}))
        fail(input,"write outside input span");
    ++totals.checks;totals.elements+=input.size();
}
std::size_t size_for(Rng& rng,std::uint64_t index) {
    constexpr std::size_t edges[]={0,1,2,3,4,7,8,9,31,32,33,63,64,65,127,128,129,
        255,256,257,511,512,513,1023,1024,1025,4095,4096,4097,8191,8192,8193,
        16383,16384,16385,32767,32768,32769,65535,65536,65537,262143,262144,
        262145,524287,524288,524289,1048575,1048576,1048577,2097151,2097152,
        2097153,4194303,4194304,4194305};
    // Broad small cases dominate throughput; rare large cases reach MSD and
    // histogram cutoffs. Width alternates, so use paired index for scheduling.
    const auto pair=index/2;
    std::size_t n;
    if(pair%128==0)n=edges[39+rng.below(std::size(edges)-39)];
    else if(pair%3==0)n=edges[rng.below(39)];
    else n=rng.below(8194);
    return std::min(n,config.max_size);
}
template<class T> std::vector<T> generate(std::uint64_t index) {
    Rng rng{config.seed^(index*UINT64_C(0xd1342543de82ef95))};
    const unsigned family=unsigned((index/2)%std::size(families));
    auto n=size_for(rng,index);
    const unsigned width=sizeof(T)*8;
    std::array<unsigned,sizeof(T)*8> positions{};
    std::iota(positions.begin(),positions.end(),0);
    for(std::size_t i=positions.size();i>1;--i)std::swap(positions[i-1],positions[rng.below(i)]);
    constexpr unsigned bit_edges[]={0,1,2,7,8,9,15,16,17,31,32,63,64};
    const unsigned bits=std::min(width,bit_edges[rng.below(std::size(bit_edges))]);
    T mask=0;for(unsigned i=0;i<bits;++i)mask|=T(1)<<positions[i];
    const T fixed=T(rng())&~mask;
    if(family==16) {
        const unsigned b=1+unsigned(rng.below(16));
        mask=0;for(unsigned i=0;i<b;++i)mask|=T(1)<<positions[i];
        const auto density=b<=8?2u:(sizeof(T)==4?16u:8u);
        const std::size_t target=(std::size_t(1)<<b)*density;
        n=std::min(config.max_size,target-1+rng.below(3));
    }
    std::vector<T> v(n);
    const T max=std::numeric_limits<T>::max();
    const T pivots[]={0,1,T(max/2),T(max-1),max,T(rng())};
    const T pivot=pivots[rng.below(std::size(pivots))];
    const unsigned percent[]={49,50,51,85,87,88,90,95,99,100};
    const unsigned dominance=percent[rng.below(std::size(percent))];
    const std::size_t alphabet=std::array<std::size_t,8>{1,2,23,24,63,64,65,256}[rng.below(8)];
    std::vector<T> collision_keys;
    if(family==19) {
        collision_keys.push_back(0);
        while(collision_keys.size()<alphabet) {
            const T candidate=T(rng());
            const std::uint64_t wide=candidate;
            const auto hash=((wide^(wide>>33))*UINT64_C(0x9e3779b97f4a7c15))>>56;
            if(hash==0 && std::find(collision_keys.begin(),collision_keys.end(),candidate)==collision_keys.end())
                collision_keys.push_back(candidate);
        }
    }
    const unsigned run=7+unsigned(rng.below(3));
    const T base=T(rng());
    for(std::size_t i=0;i<n;++i) {
        T x=T(rng());
        switch(family) {
        case 1:case 2:case 16:x=(x&mask)|(fixed&~mask);break;
        case 3:x=pivot;break;
        case 4:case 18:if(rng.below(100)<dominance)x=pivot;break;
        case 5:case 6:break;
        case 7:case 8:case 14:x=T(i*2);break;
        case 9:x=T((x%alphabet)*UINT64_C(0x9e3779b97f4a7c15));break;
        case 10:x=T(max-T(rng.below(16))+T(x&255));break; // intentional unsigned wrap
        case 11:x=T((i/run)%3)<<(positions[0]); x|=T((rng()&3))<<positions[1];break;
        case 12:x=(i&1)?max:T(0);break;
        case 13:x=T(std::min(i,n-i));break;
        case 15:x=T(i%(1+rng.below(257)))+base;break;
        case 17:x=T(i/16);break;
        case 19:x=collision_keys[x%alphabet];break;
        case 20:x=T((base&~T(255)) + (x&511));break;
        case 21:x=T(rng.below(2)?rng():pivot);break;
        }
        v[i]=x;
    }
    if(n) {
        if(family==2 && bits<width) {
            const std::size_t spots[]={0,n-1,n/2,std::min(n-1,std::size_t(255)),std::min(n-1,std::size_t(256)),n>1?n-2:0,rng.below(n)};
            v[spots[rng.below(std::size(spots))]]^=T(1)<<positions[bits];
        }
        if(family==5 || family==6) {
            for(std::size_t i=0;i<std::min(n,std::size_t(64));++i)v[i]=family==5?pivot:T(i);
            // Deliberately satisfy BOTH spaced and contiguous probes.
            for(std::size_t i=0;i<32;++i) {
                const auto p=i*((n-1)/31)+i*((n-1)%31)/31;
                v[p]=family==5?pivot:T(p);
            }
        }
        if(family==7) {
            const unsigned swaps=unsigned(rng.below(70));
            for(unsigned j=0;j<swaps && n>1;++j) {
                const auto p=rng.below(n-1);std::swap(v[p],v[p+1]);
            }
            if(rng.below(2))for(auto&x:v)x=max-x;
        }
        if(family==8 && n>2)v[n-2]=0;
        if(family==14)std::rotate(v.begin(),v.begin()+rng.below(n),v.end());
        if(family==17 && n>514) {
            std::swap(v[511],v[512]);v[n-2]=rng.below(2)?0:max;
        }
        if(family==18)std::stable_partition(v.begin(),v.end(),[=](T x){return x==pivot;});
        if(family==21) {
            for(unsigned j=0;j<32;++j) {
                const auto p=rng.below(n);
                switch(rng.below(4)) {
                case 0:v[p]^=T(1)<<rng.below(width);break;
                case 1:std::swap(v[p],v[rng.below(n)]);break;
                case 2:v[p]=max;break;
                case 3:v[p]=0;break;
                }
            }
        }
    }
    return v;
}
template<class T> void one(std::uint64_t index) {
    auto input=generate<T>(index);
    current_stage=families[(index/2)%std::size(families)];
    check(input);
    if((index/2)%17==0) {check(input,0);check(input,2);}
    if((index/2)%31==0) { // Same multiset, new order and new sampled positions.
        std::reverse(input.begin(),input.end());check(input);
        std::sort(input.begin(),input.end());check(input);
    }
    if((index/2)%11==0) {
        for(bool descending:{false,true}) {
            auto repaired=input;
            const bool accepted=descending?
                radixkit::detail::adaptive_keys::check_and_repair<true>(std::span(repaired)):
                radixkit::detail::adaptive_keys::check_and_repair<false>(std::span(repaired));
            auto expected=input;
            if(descending)std::sort(expected.begin(),expected.end(),std::greater<>{});
            else std::sort(expected.begin(),expected.end());
            if(accepted && repaired!=expected)fail(input,"repair accepted an unordered result");
            std::sort(repaired.begin(),repaired.end());std::sort(expected.begin(),expected.end());
            if(repaired!=expected)fail(input,"failed repair changed the multiset");
            ++totals.checks;
        }
    }
    ++totals.cases;++totals.families[(index/2)%std::size(families)];
    if constexpr(sizeof(T)==4)++totals.u32;else ++totals.u64;
}
template<class T> void exhaustive() {
    const std::array<T,4> alphabet{0,1,T(1)<<(sizeof(T)*8-1),std::numeric_limits<T>::max()};
    for(unsigned n=0;n<=8;++n) {
        const auto combinations=std::uint64_t(1)<<(2*n);
        for(std::uint64_t code=0;code<combinations;++code) {
            current_index=code;current_stage="exhaustive-n"+std::to_string(n);
            std::vector<T> v(n);auto digits=code;
            for(auto&x:v){x=alphabet[digits&3];digits>>=2;}
            check(v);
            // Lift short patterns past the adaptive cutoff so enumeration also
            // exercises sampled dispatch, counting, and non-vector-aligned tails.
            if(n && n<=5)for(std::size_t size:{512u,513u}) {
                std::vector<T> lifted(size);
                for(std::size_t i=0;i<size;++i)lifted[i]=v[i%n];
                check(lifted);
            }
        }
    }
}
void progress(double elapsed) {
    std::cout<<"cases="<<totals.cases<<" u32="<<totals.u32<<" u64="<<totals.u64
             <<" checks="<<totals.checks<<" sorted_elements="<<totals.elements
             <<" elapsed_seconds="<<elapsed<<std::endl;
}
}
int main(int argc,char**argv) {try {
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];
        if(arg=="--exhaustive"){config.exhaustive=true;continue;}
        if(i+1==argc)throw std::runtime_error("missing option value");
        const std::string value=argv[++i];
        if(arg=="--seed")config.seed=std::stoull(value);
        else if(arg=="--cases")config.cases=std::stoull(value);
        else if(arg=="--seconds")config.seconds=std::stod(value);
        else if(arg=="--max-size")config.max_size=std::stoull(value);
        else if(arg=="--case")config.replay=std::stoull(value);
        else if(arg=="--checkpoint")config.checkpoint=value;
        else if(arg=="--failure-prefix")config.failure=value;
        else throw std::runtime_error("unknown option: "+arg);
    }
    if(config.max_size>8388608 || (!config.cases && !(config.seconds>0)) || !std::isfinite(config.seconds) || config.seconds<0)
        throw std::runtime_error("use max-size <=8388608 and a finite positive case/time limit");
    const auto start=std::chrono::steady_clock::now();
    const auto elapsed=[&]{return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();};
    std::cout<<"seed="<<config.seed<<" max_size="<<config.max_size<<std::endl;
    if(config.exhaustive){exhaustive<std::uint32_t>();exhaustive<std::uint64_t>();progress(elapsed());}
    const auto first=config.replay.value_or(0);
    double last=elapsed();
    for(std::uint64_t i=first;;++i) {
        if(config.replay && i!=first)break;
        if(!config.replay && ((config.cases && totals.cases>=config.cases) ||
            (config.seconds>0 && elapsed()>=config.seconds)))break;
        current_index=i;
        if(!config.checkpoint.empty()) {
            std::ofstream checkpoint(config.checkpoint);
            if(!checkpoint)throw std::runtime_error("cannot write checkpoint");
            checkpoint<<"--seed "<<config.seed<<" --case "<<i<<" --max-size "<<config.max_size<<'\n';
        }
        if(i&1)one<std::uint64_t>(i);else one<std::uint32_t>(i);
        if(elapsed()-last>=10){progress(elapsed());last=elapsed();}
    }
    progress(elapsed());
    for(unsigned f=0;f<std::size(families);++f)std::cout<<families[f]<<'='<<totals.families[f]<<'\n';
    std::cout<<"PASS\n";
}catch(const std::exception&e){std::cerr<<"FAIL seed="<<config.seed<<" case="<<current_index<<" stage="<<current_stage<<": "<<e.what()<<'\n';return 1;}}
