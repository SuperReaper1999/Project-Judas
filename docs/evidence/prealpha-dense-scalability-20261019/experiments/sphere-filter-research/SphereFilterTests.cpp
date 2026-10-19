#include "SpherePredicateFilter.h"
#include <cstdio>
#include <string>
#include <cstdint>

using namespace contact_geometry;
namespace {
std::uint64_t checks=0,failures=0,certified=0,ambiguous=0,legacyExactBranches=0;
std::uint64_t exactZeros=0,legacyZeroBranches=0,exactCompared=0,rawChangesLegacyBranch=0;
std::uint32_t state=0x41e9a771;
std::uint32_t Random(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
float Between(float lo,float hi){return lo+(hi-lo)*float(Random()>>8)/float(1u<<24);}
float Scaled(){return std::ldexp(Between(-1,1),int(Random()%87)-43);}
void Check(bool value,const char* text,unsigned row){++checks;if(!value){++failures;if(failures<20)std::printf("FAIL %s row=%u\n",text,row);}}
int IntervalSign(Iv value){
    if(std::isfinite(value.lo)&&std::isfinite(value.hi)){
        if(value.lo>0)return 1;
        if(value.hi<0)return -1;
        if(value.lo==0&&value.hi==0)return 0;
    }
    return SpherePredicateFilter::Unresolved;
}
void Corpus(bool omitBound){
    for(unsigned row=0;row<200000;++row){
        ContactPose a,b;
        auto quaternion=[](){glm::quat q{Between(-2,2),Between(-2,2),Between(-2,2),Between(-2,2)};if(glm::dot(q,q)==0)q.w=1;return q;};
        a.position={Between(-100,100),Between(-100,100),Between(-100,100)};
        b.position=a.position+glm::vec3(Between(-3,3),Between(-3,3),Between(-3,3));
        a.localCenter={Between(-1,1),Between(-1,1),Between(-1,1)};
        b.localCenter={Between(-1,1),Between(-1,1),Between(-1,1)};
        a.orientation=quaternion();b.orientation=quaternion();
        glm::vec3 half{Between(.05f,2),Between(.05f,2),Between(.05f,2)};
        float ra=Between(.05f,2),rb=Between(.05f,2),margin=row%4==0?Between(0,.1f):0;
        const bool sphere=row%2==0;
        if(row%10==0){
            a.position={Scaled(),Scaled(),Scaled()};b.position={Scaled(),Scaled(),Scaled()};
            a.localCenter={Scaled(),Scaled(),Scaled()};b.localCenter={Scaled(),Scaled(),Scaled()};
            a.orientation={Scaled(),Scaled(),Scaled(),Scaled()};b.orientation={Scaled(),Scaled(),Scaled(),Scaled()};
            half=glm::abs(glm::vec3(Scaled(),Scaled(),Scaled()));ra=std::abs(Scaled());rb=std::abs(Scaled());
        }
        if(row%11==0){
            // Exact3-4-5 tangency with arbitrary homogeneous denominators.
            // This exposes a raw-double sign which changes the original
            // interval/exact branch even when no geometry tolerance is used.
            a.position={0,0,0};b.position={3,4,0};a.localCenter=b.localCenter={0,0,0};
            ra=2;rb=3;margin=0;
            if(!sphere){a.position={4,5,1};b.position={0,0,0};half={1,1,1};ra=5;
                b.orientation={Between(.01f,3),0,0,0};}
        }
        if(row%13==0){
            a.position={65536,-32768,131072};b.position=a.position+glm::vec3(1,0,0);
            a.localCenter=b.localCenter={0,0,0};a.orientation=b.orientation={1,0,0,0};
            half={.5f,.5f,.5f};ra=rb=.5f;margin=0;
        }
        if(row%17==0){
            a.position={std::nextafter(1.f,row%34?0.f:2.f),0,0};b.position={0,0,0};
            a.localCenter=b.localCenter={0,0,0};a.orientation=b.orientation={1,0,0,0};
            half={.5f,.5f,.5f};ra=rb=.5f;margin=0;
        }
        if(row%19==0){
            // The positive clamp is exercised exactly on faces/edges and one
            // represented float on either side. No guessed branch is certified.
            b.orientation={1,0,0,0};a.position={.5f,.5f,.5f};b.position={0,0,0};
            a.localCenter=b.localCenter={0,0,0};half={.5f,.5f,.5f};
            a.position[row%3]=std::nextafter(.5f,row%38?0.f:1.f);
        }
        const Pair<double> number(a,b);const Pair<Iv> interval(a,b);
        const SpherePredicateFilter filter(a,b,number);
        const double r=sphere?double(ra)+double(rb)+double(margin):double(ra)+double(margin);
        const Iv ri=sphere?Iv(double(ra))+Iv(double(rb))+Iv(double(margin)):Iv(double(ra))+Iv(double(margin));
        const double scalar=sphere?SpherePredicate(number,r):SphereBoxPredicate(number,half,r);
        const Iv legacy=sphere?SpherePredicate(interval,ri):SphereBoxPredicate(interval,half,ri);
        const int expectedInterval=IntervalSign(legacy);
        int actual=sphere?filter.Sphere(scalar,ra,rb,margin):filter.SphereBox(scalar,ra,half,margin);
        legacyExactBranches+=expectedInterval==SpherePredicateFilter::Unresolved;
        legacyZeroBranches+=expectedInterval==0;
        const int raw=scalar>0?1:scalar<0?-1:SpherePredicateFilter::Unresolved;
        if(raw!=SpherePredicateFilter::Unresolved&&raw!=expectedInterval)++rawChangesLegacyBranch;
        if(omitBound)actual=raw;
        if(actual==SpherePredicateFilter::Unresolved)++ambiguous;
        else {
            ++certified;
            Check(actual==expectedInterval,"certificate preserves legacy interval-resolved sign and exact-gap branch",row);
        }
        Check(actual!=0,"filter never substitutes an exact-zero decision",row);
        if(row<512 || (row%997==0&&row%10!=0) || row%1100==11){
            try{
                const Pair<Exact> exact(a,b);
                const Exact re=sphere?Exact(double(ra))+Exact(double(rb))+Exact(double(margin)):Exact(double(ra))+Exact(double(margin));
                const auto value=sphere?SpherePredicate(exact,re):SphereBoxPredicate(exact,half,re);
                const int truth=value.Sign();++exactCompared;exactZeros+=truth==0;
                if(actual!=SpherePredicateFilter::Unresolved)Check(actual==truth,"certificate agrees with independent expansion",row);
                if(expectedInterval!=SpherePredicateFilter::Unresolved)Check(expectedInterval==truth,"legacy interval agrees with independent expansion",row);
            }catch(const ArithmeticFailure&){Check(actual==SpherePredicateFilter::Unresolved||expectedInterval==actual,"legacy exact range limitation is not silently bypassed",row);}
        }
    }
    Check(certified>10000,"substantial certified positive and negative population",200000);
    Check(ambiguous>1000,"substantial real ambiguity/fallback population",200000);
    Check(exactCompared>500,"independent exact arithmetic was exercised",200000);
    Check(exactZeros>10,"genuine exact zeros retain legacy branch",200000);
    Check(rawChangesLegacyBranch>100,"negative scalar-only control genuinely changes legacy branch",200000);
}
void DomainFallbacks(){
    for(unsigned row=0;row<12;++row){
        ContactPose a,b;glm::vec3 half(.5f);float radius=.5f,margin=0;
        if(row==0)a.position.x=0x1p49f;
        if(row==1)b.localCenter.y=0x1p-49f;
        if(row==2)a.orientation.x=std::numeric_limits<float>::denorm_min();
        if(row==3)half.z=0;
        if(row==4)radius=0;
        if(row==5)a.orientation={0,0,0,0};
        if(row==6)margin=-.01f;
        if(row==7)a.position.x=std::numeric_limits<float>::infinity();
        if(row==8)b.orientation.z=std::numeric_limits<float>::quiet_NaN();
        if(row==9)half.x=-.5f;
        if(row==10)radius=std::numeric_limits<float>::infinity();
        if(row==11)margin=std::numeric_limits<float>::quiet_NaN();
        const Pair<double> pair(a,b);const SpherePredicateFilter filter(a,b,pair);
        Check(filter.SphereBox(1,radius,half,margin)==SpherePredicateFilter::Unresolved,"invalid/out-of-domain sphere-box retains legacy fallback",row);
        if(row!=3&&row!=9)Check(filter.Sphere(1,radius,.5f,margin)==SpherePredicateFilter::Unresolved,"invalid/out-of-domain sphere pair retains legacy fallback",row);
    }
    const int original=std::fegetround();
    for(const int mode:{FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){
        if(std::fesetround(mode)!=0)continue;
        ContactPose a,b;const Pair<double> pair(a,b);const SpherePredicateFilter filter(a,b,pair);
        Check(filter.Sphere(1,.5f,.5f)==SpherePredicateFilter::Unresolved,"directed rounding retains original sphere path",unsigned(mode));
        Check(filter.SphereBox(-1,.5f,{.5f,.5f,.5f})==SpherePredicateFilter::Unresolved,"directed rounding retains original sphere-box path",unsigned(mode));
    }
    std::fesetround(original);
}
} // namespace
int main(int argc,char** argv){
    const bool omitBound=argc==2&&std::string(argv[1])=="--omit-bound";
    if(argc>2||(argc==2&&!omitBound))return 2;
    Corpus(omitBound);DomainFallbacks();
    std::printf("sphere_filter rows=200000 checks=%llu failures=%llu certified=%llu ambiguous=%llu legacy_exact_branches=%llu legacy_zero_branches=%llu independent_exact=%llu exact_zeros=%llu scalar_changes_legacy_branch=%llu\n",
        (unsigned long long)checks,(unsigned long long)failures,(unsigned long long)certified,(unsigned long long)ambiguous,
        (unsigned long long)legacyExactBranches,(unsigned long long)legacyZeroBranches,(unsigned long long)exactCompared,
        (unsigned long long)exactZeros,(unsigned long long)rawChangesLegacyBranch);
    return failures?1:0;
}
