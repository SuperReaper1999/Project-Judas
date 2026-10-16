// M91: bit-for-bit reference check against 4628494c's interval operations.
// The reference expressions intentionally retain the former evaluation order.
#include "Contacts.h"
#include "ContactGeometryInternal.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>
using namespace contact_geometry;
namespace {
Iv MultiplyReference(Iv a,Iv b){
    if((a.lo==0&&a.hi==0)||(b.lo==0&&b.hi==0))return Iv(0);
    if(a.lo==1&&a.hi==1)return b;
    if(b.lo==1&&b.hi==1)return a;
    if(a.lo==-1&&a.hi==-1)return -b;
    if(b.lo==-1&&b.hi==-1)return -a;
    if(a.lo==a.hi&&b.lo==b.hi){const double p=a.lo*b.lo,e=std::fma(a.lo,b.lo,-p);
        if(std::isfinite(p)&&std::abs(p)>=std::numeric_limits<double>::min()&&std::ilogb(std::abs(a.lo))+std::ilogb(std::abs(b.lo))>=-968)return {e<0?Down(p):p,e>0?Up(p):p};}
    const std::array<double,4> v{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};
    return {Down(*std::min_element(v.begin(),v.end())),Up(*std::max_element(v.begin(),v.end()))};
}
Iv DivideReference(Iv a,Iv b){
    if(b.lo<=0&&b.hi>=0)throw ArithmeticFailure();
    if(b.lo==1&&b.hi==1)return a;
    const std::array<double,4> v{a.lo/b.lo,a.lo/b.hi,a.hi/b.lo,a.hi/b.hi};
    return {Down(*std::min_element(v.begin(),v.end())),Up(*std::max_element(v.begin(),v.end()))};
}
bool Same(Iv a,Iv b){return std::memcmp(&a.lo,&b.lo,8)==0&&std::memcmp(&a.hi,&b.hi,8)==0;}
struct OperandPair {Iv a,b;};
}
int main(int argc,char**){
    std::mt19937_64 random(910016);std::vector<OperandPair> values;values.reserve(300000);
    for(unsigned i=0;i<300000;++i){
        auto scalar=[&](){std::uint64_t bits=random();double value;std::memcpy(&value,&bits,8);return std::isfinite(value)?value:0.;};
        const double a=scalar(),b=scalar();Iv aa(a),bb(b);
        if(i%3==0)aa={std::min(a,b),std::max(a,b)};
        if(i%3==1)bb={std::min(a,b),std::max(a,b)};
        values.push_back({aa,bb});
    }
    const double edges[]={0.,-0.,1.,-1.,std::numeric_limits<double>::denorm_min(),-std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::min(),std::numeric_limits<double>::max(),std::nextafter(1.,0.),std::nextafter(1.,2.)};
    for(double a:edges)for(double b:edges){values.push_back({Iv(a),Iv(b)});values.push_back({Iv(a),Iv(std::min(a,b),std::max(a,b))});}
    unsigned differences=0,divisions=0,throws=0;
    for(const auto& p:values){differences+=!Same(p.a*p.b,MultiplyReference(p.a,p.b));
        if(p.b.lo<=0&&p.b.hi>=0){try{(void)(p.a/p.b);++differences;}catch(const ArithmeticFailure&){++throws;}}
        else{++divisions;differences+=!Same(p.a/p.b,DivideReference(p.a,p.b));}
    }
    std::printf("M91 interval equivalence multiplications=%zu divisions=%u zero_divisor_rejections=%u bit_differences=%u\n",values.size(),divisions,throws,differences);
    if(argc>1){
        // Representative ordinary contact operands; unlike the exhaustive
        // exponent test these avoid expensive overflow/underflow exceptions.
        values.clear();std::uniform_real_distribution<double> ordinary(-2,2);
        for(unsigned i=0;i<32768;++i){double a=ordinary(random),b=ordinary(random);values.push_back({Iv(a),i%2?Iv(b):Iv(b,b+1e-12)});}
        for(unsigned repeat=0;repeat<3;++repeat)for(bool reference:{true,false}){
            volatile double sum=0;const auto start=std::chrono::steady_clock::now();
            for(unsigned pass=0;pass<64;++pass)for(const auto& p:values){const auto result=reference?MultiplyReference(p.a,p.b):p.a*p.b;sum=sum+result.lo;}
            std::printf("KERNEL repeat=%u reference=%d ms=%.6f checksum=%.17g\n",repeat,int(reference),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),double(sum));
        }
    }
    return differences?1:0;
}
