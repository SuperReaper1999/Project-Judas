#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <cstring>
#include <random>
#define private public
#include "ContactBlock.h"
#undef private

namespace {
using namespace contact_block;
unsigned checks=0,failures=0;
void Check(bool value,const char* label) {
    ++checks;
    if(!value){++failures;std::printf("FAIL %s\n",label);}
}
// The prior control flow: no fast path, same established factorization and
// candidate validator. This checks canonical output and future active-set
// selection, rather than merely accepting any feasible complementarity result.
bool OriginalSolve(NormalPatch& patch,const Vector& q,const Vector& previous,Vector& out) {
    if(!patch.count || patch.count>4) return false;
    const unsigned all=(1u<<patch.count)-1;
    if(!patch.Build(all).valid) return false;
    if(patch.Try(patch.preferred,q,previous,out)) return true;
    for(unsigned mask=all;;--mask) {
        if(mask!=patch.preferred && patch.Try(mask,q,previous,out)) {
            patch.preferred=mask;return true;
        }
        if(!mask) break;
    }
    return false;
}
bool Same(const Vector& a,const Vector& b) {
    for(unsigned i=0;i<a.size();++i)
        if(std::memcmp(&a[i],&b[i],sizeof(double))) return false;
    return true;
}
bool Compare(NormalPatch& patch,NormalPatch& original,const Vector& q,const Vector& previous) {
    Vector a{31,32,33,34},b=a;
    const bool actual=patch.Solve(q,previous,a),expected=OriginalSolve(original,q,previous,b);
    Check(actual==expected,"acceptance agrees with original control flow");
    Check(Same(a,b),"all four output impulse representations agree exactly");
    Check(patch.preferred==original.preferred,"canonical preferred active set is preserved");
    return actual;
}
}
int main() {
    std::mt19937 random(0x4e4f4f50u);
    const auto sample=[&](){return double(int(random()%2001)-1000)/128;};
    for(unsigned trial=0;trial<256;++trial) {
        const unsigned n=1+trial%4,rank=trial%3==0?n:(n>1?n-1:1);
        Matrix basis{},matrix{};
        for(unsigned i=0;i<n;++i)for(unsigned j=0;j<rank;++j)basis[i][j]=sample();
        for(unsigned i=0;i<n;++i)for(unsigned j=i;j<n;++j) {
            double value=0;for(unsigned k=0;k<rank;++k)value+=basis[i][k]*basis[j][k];
            matrix[i][j]=matrix[j][i]=value;
        }
        NormalPatch patch,original;patch.Prepare(matrix,n);original.Prepare(matrix,n);
        Vector previous{};
        for(unsigned step=0;step<16;++step) {
            Vector q{},old{};
            if(step%4==1) for(unsigned i=0;i<n;++i)q[i]=sample();
            if(step%4==2) {for(unsigned i=0;i<n;++i)q[i]=sample();old=previous;}
            if(step%4==3) for(unsigned i=0;i<n;++i)q[i]=i%2?-0.0:0.0;
            Compare(patch,original,q,old);
            // Feed identical nonzero distributions into later singular solves.
            for(unsigned i=0;i<n;++i)previous[i]=std::abs(sample());
            if(step==7) {patch.Prepare(matrix,n);original.Prepare(matrix,n);}
        }
    }
    {
        Matrix matrix{};matrix[0][0]=matrix[1][1]=1;matrix[0][1]=matrix[1][0]=-1;
        NormalPatch patch,original;patch.Prepare(matrix,2);original.Prepare(matrix,2);
        const Vector previous{.3,.3,0,0};Vector out{};
        Check(patch.Solve({},previous,out)&&out[0]>0&&out[1]>0,
              "zero residual retains a genuine positive null-space distribution");
        patch.Prepare(matrix,2);Compare(patch,original,{},previous);
        Compare(patch,original,{},{}); // zero following a nonzero singular load
    }
    for(double invalid:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        Matrix matrix{};matrix[0][0]=invalid;
        NormalPatch patch,original;patch.Prepare(matrix,1);original.Prepare(matrix,1);
        Check(!Compare(patch,original,{},{}),"zero input cannot bypass invalid PSD response rejection");
    }
    {
        Matrix matrix{};for(unsigned i=0;i<4;++i)matrix[i][i]=1;
        NormalPatch patch,original;patch.Prepare(matrix,4);original.Prepare(matrix,4);
        for(double value:{0.0,-0.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            Vector q{};q[0]=value;Compare(patch,original,q,{});
            Vector previous{};previous[1]=value;Compare(patch,original,{},previous);
        }
        for(unsigned size:{0u,33u}) {
            patch.Prepare(matrix,size);original.Prepare(matrix,size);
            Check(!Compare(patch,original,{},{}),"invalid patch size remains rejected before mask arithmetic");
        }
    }
    std::printf("Contact block exact zero: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
